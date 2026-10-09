#include "audio.h"
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

void free_audio_buffer(AudioBuffer* buffer)
{
	if (!buffer)
		return;

	clear_audio_buffer(buffer);
	free(buffer);
}



void impulse_response_to_audio_buffer(AudioSource* speaker)
{

	AudioBuffer* buffer = speaker->ir_buffer;

	if (!buffer)
		return;

	clear_audio_buffer(buffer);


	buffer->left = calloc(MAX_IR_SAMPLES, sizeof(float));
	buffer->right = calloc(MAX_IR_SAMPLES, sizeof(float));

	if (!buffer->left || !buffer->right)
	{
		fprintf(stderr, "Failed to allocate impulse response\n");
		clear_audio_buffer(buffer);
		return;
	}

	buffer->sampleCount = MAX_IR_SAMPLES;
	buffer->sampleRate = SAMPLE_RATE;

	for (int i = 0; i < MAX_IR_SAMPLES; i++) {

		float x = 0.0f;

		float l = 0.0f;
		float r = 0.0f;

		for (int b = 0; b < NUM_FREQ_BANDS; b++) {
			l += sqrtf(speaker->raw_l_ir[b][i]);
			r += sqrtf(speaker->raw_r_ir[b][i]);
		}
		l /= NUM_FREQ_BANDS;
		r /= NUM_FREQ_BANDS;

		l = fmaxf(-1.0f, fminf(1.0f, l));
		r = fmaxf(-1.0f, fminf(1.0f, r));

		buffer->left[i] = l;
		buffer->right[i] = r;
	}

}


bool import_audio_buffer(const char* filepath, AudioBuffer* dest)
{
	AVFormatContext* format_ctx = NULL;
	AVCodecContext* decoder_ctx = NULL;
	const AVCodec* decoder = NULL;
	SwrContext* swr = NULL;
	AVPacket* pkt = NULL;
	AVFrame* frame = NULL;

	int audio_stream_index = -1;

	AudioBuffer result = { 0 };
	result.sampleRate = SAMPLE_RATE;

	// Open file 
	if (avformat_open_input(&format_ctx, filepath, NULL, NULL) < 0)
		return;

	if (avformat_find_stream_info(format_ctx, NULL) < 0)
		goto cleanup;

	// Find first audio stream
	for (unsigned int i = 0; i < format_ctx->nb_streams; i++)
	{
		if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audio_stream_index = i;
			break;
		}
	}

	if (audio_stream_index < 0)
		goto cleanup;

	AVStream* stream = format_ctx->streams[audio_stream_index];

	decoder = avcodec_find_decoder(stream->codecpar->codec_id);
	if (!decoder)
		goto cleanup;

	decoder_ctx = avcodec_alloc_context3(decoder);
	if (!decoder_ctx)
		goto cleanup;

	if (avcodec_parameters_to_context(decoder_ctx, stream->codecpar) < 0)
		goto cleanup;

	if (avcodec_open2(decoder_ctx, decoder, NULL) < 0)
		goto cleanup;

	// Resample / convert to stereo SAMPLE_RATE
	swr = swr_alloc();
	if (!swr)
		goto cleanup;

	AVChannelLayout out_layout;
	av_channel_layout_default(&out_layout, 2);

	av_opt_set_chlayout(swr, "in_chlayout", &decoder_ctx->ch_layout, 0);
	av_opt_set_chlayout(swr, "out_chlayout", &out_layout, 0);

	av_opt_set_int(swr, "in_sample_rate", decoder_ctx->sample_rate, 0);
	av_opt_set_int(swr, "out_sample_rate", SAMPLE_RATE, 0);

	av_opt_set_sample_fmt(swr, "in_sample_fmt", decoder_ctx->sample_fmt, 0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

	if (swr_init(swr) < 0)
		goto cleanup;

	pkt = av_packet_alloc();
	frame = av_frame_alloc();

	if (!pkt || !frame)
		goto cleanup;

	int capacity = SAMPLE_RATE; // initial 1 second

	if (!grow_audio_buffer(&result, capacity))
		goto cleanup;

	while (av_read_frame(format_ctx, pkt) >= 0)
	{
		if (pkt->stream_index == audio_stream_index)
		{
			if (avcodec_send_packet(decoder_ctx, pkt) == 0)
			{
				while (avcodec_receive_frame(decoder_ctx, frame) == 0)
				{
					int out_samples = av_rescale_rnd(swr_get_delay(swr, decoder_ctx->sample_rate) + frame->nb_samples,SAMPLE_RATE,decoder_ctx->sample_rate,AV_ROUND_UP);

					uint8_t* out_buf = NULL;
					int out_linesize = 0;

					if (av_samples_alloc(&out_buf,&out_linesize,2,out_samples,AV_SAMPLE_FMT_FLT,0) < 0)
					{
						goto cleanup;
					}

					int samples_written = swr_convert(swr,&out_buf,out_samples,(const uint8_t**)frame->extended_data,frame->nb_samples);

					if (samples_written < 0)
					{
						av_freep(&out_buf);
						goto cleanup;
					}

					float* pcm = (float*)out_buf;

					// Grow buffer if needed 
					if (samples_written < 0 ||
						samples_written > INT_MAX - result.sampleCount)
					{
						av_freep(&out_buf);
						goto cleanup;
					}

					while (samples_written > capacity - result.sampleCount)
					{
						if (capacity > INT_MAX / 2)
						{
							av_freep(&out_buf);
							goto cleanup;
						}

						capacity *= 2;

						if (!grow_audio_buffer(&result, capacity))
						{
							av_freep(&out_buf);
							goto cleanup;
						}
					}

					for (int i = 0; i < samples_written; i++)
					{
						result.left[result.sampleCount] = pcm[i * 2];
						result.right[result.sampleCount] = pcm[i * 2 + 1];
						result.sampleCount++;
					}

					av_freep(&out_buf);
				}
			}
		}

		av_packet_unref(pkt);
	}

	clear_audio_buffer(dest);
	*dest = result;
	return true;

cleanup:

	if (pkt) av_packet_free(&pkt);
	if (frame) av_frame_free(&frame);
	if (swr) swr_free(&swr);
	if (decoder_ctx) avcodec_free_context(&decoder_ctx);
	if (format_ctx) avformat_close_input(&format_ctx);

	clear_audio_buffer(&result);
	return false;
}

static int grow_audio_buffer(AudioBuffer* buffer, int new_capacity)
{
	if (!buffer || new_capacity <= 0 ||
		(size_t)new_capacity > SIZE_MAX / sizeof(float))
		return 0;

	size_t bytes = (size_t)new_capacity * sizeof(float);

	float* new_left = malloc(bytes);
	float* new_right = malloc(bytes);

	if (!new_left || !new_right)
	{
		free(new_left);
		free(new_right);
		return 0;
	}

	size_t used = (size_t)buffer->sampleCount * sizeof(float);

	if (used > 0)
	{
		memcpy(new_left, buffer->left, used);
		memcpy(new_right, buffer->right, used);
	}

	free(buffer->left);
	free(buffer->right);

	buffer->left = new_left;
	buffer->right = new_right;

	return 1;
}

void output_audio_buffer(const char* filename, AudioBuffer* buffer)
{
	AVFormatContext* format_ctx = NULL;
	AVStream* stream = NULL;
	AVCodecContext* encoder_ctx = NULL;
	const AVCodec* encoder = NULL;

	avformat_alloc_output_context2(&format_ctx, NULL, "wav", filename);
	if (!format_ctx)
		return;

	encoder = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE);
	if (!encoder)
		goto cleanup;

	stream = avformat_new_stream(format_ctx, NULL);
	if (!stream)
		goto cleanup;

	stream->time_base = (AVRational){ 1, SAMPLE_RATE };

	encoder_ctx = avcodec_alloc_context3(encoder);
	if (!encoder_ctx)
		goto cleanup;

	encoder_ctx->codec_id = AV_CODEC_ID_PCM_S16LE;
	encoder_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
	encoder_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
	encoder_ctx->sample_rate = SAMPLE_RATE;

	av_channel_layout_default(&encoder_ctx->ch_layout, 2);

	if (avcodec_open2(encoder_ctx, encoder, NULL) < 0)
		goto cleanup;

	if (avcodec_parameters_from_context(stream->codecpar, encoder_ctx) < 0)
		goto cleanup;

	if (avio_open(&format_ctx->pb, filename, AVIO_FLAG_WRITE) < 0)
		goto cleanup;

	if (avformat_write_header(format_ctx, NULL) < 0)
		goto cleanup;

	// convert float [-1.0,1.0] to interleaved int16 stereo 
	int sample_count = buffer->sampleCount;
	int16_t* pcm = malloc(sample_count * 2 * sizeof(int16_t));

	if (!pcm)
		goto cleanup;

	for (int i = 0; i < sample_count; i++)
	{
		float l = buffer->left[i];
		float r = buffer->right[i];

		if (l > 1.0f) l = 1.0f;
		if (l < -1.0f) l = -1.0f;

		if (r > 1.0f) r = 1.0f;
		if (r < -1.0f) r = -1.0f;

		pcm[i * 2 + 0] = (int16_t)(l * 32767.0f);
		pcm[i * 2 + 1] = (int16_t)(r * 32767.0f);
	}

	int data_size = sample_count * 2 * sizeof(int16_t);

	AVPacket* pkt = av_packet_alloc();
	if (!pkt)
	{
		free(pcm);
		goto cleanup;
	}

	pkt->data = (uint8_t*)pcm;
	pkt->size = data_size;
	pkt->stream_index = stream->index;
	pkt->pts = 0;
	pkt->dts = 0;
	pkt->duration = sample_count;

	av_write_frame(format_ctx, pkt);
	av_packet_free(&pkt);

	free(pcm);

	av_write_trailer(format_ctx);

cleanup:

	if (encoder_ctx)
		avcodec_free_context(&encoder_ctx);

	if (format_ctx)
	{
		if (format_ctx->pb)
			avio_close(format_ctx->pb);

		avformat_free_context(format_ctx);
	}

	printf("Outputted");
}



void convolute_buffer(AudioBuffer* audio, AudioBuffer* ir, AudioBuffer* dest)
{
	if (!audio || !ir || !dest)
		return;

	if (!audio->left || !audio->right ||
		!ir->left || !ir->right)
		return;

	if (audio->sampleCount <= 0 ||
		ir->sampleCount <= 0)
		return;

	size_t output_len =
		(size_t)audio->sampleCount + ir->sampleCount - 1;

	if (output_len > INT_MAX ||
		output_len > SIZE_MAX / sizeof(float))
		return;

	float* new_left = malloc(output_len * sizeof(float));
	float* new_right = malloc(output_len * sizeof(float));

	if (!new_left || !new_right)
	{
		free(new_left);
		free(new_right);

		fprintf(stderr, "Failed to allocate convolution output\n");
		return;
	}

	if (!new_left || !new_right)
	{
		free(new_left);
		free(new_right);
		return;
	}

	int left_ok = fft_convolve(
		audio->left,
		audio->sampleCount,
		ir->left,
		ir->sampleCount,
		new_left
	);

	int right_ok = fft_convolve(
		audio->right,
		audio->sampleCount,
		ir->right,
		ir->sampleCount,
		new_right
	);

	if (!left_ok || !right_ok)
	{
		free(new_left);
		free(new_right);
		return;
	}



	clear_audio_buffer(dest);
	free(dest->left);
	free(dest->right);

	dest->left = new_left;
	dest->right = new_right;

	dest->sampleCount = (int)output_len;
	dest->sampleRate = audio->sampleRate;
}

void mix_audio_buffers(AudioBuffer** buffers, int buffer_count, AudioBuffer* dest)
{
	if (!buffers || buffer_count <= 0 || !dest)
		return;
	clear_audio_buffer(dest);

	int max_samples = 0;

	// Find longest buffer
	for (int i = 0; i < buffer_count; ++i)
	{
		if (buffers[i] &&
			buffers[i]->sampleCount > max_samples)
		{
			max_samples = buffers[i]->sampleCount;
		}
	}

	if (max_samples <= 0)
		return;

	dest->left = calloc(max_samples, sizeof(float));
	dest->right = calloc(max_samples, sizeof(float));

	if (!dest->left || !dest->right)
	{
		fprintf(stderr, "Failed to allocate mixed audio buffer\n");
		clear_audio_buffer(dest);
		return;
	}

	dest->sampleCount = max_samples;
	dest->sampleRate = SAMPLE_RATE;



	// Add every source into destination
	for (int source = 0; source < buffer_count; ++source)
	{
		AudioBuffer* buffer = buffers[source];

		if (!buffer)
			continue;

		for (int sample = 0; sample < buffer->sampleCount; ++sample)
		{
			dest->left[sample] += buffer->left[sample];
			dest->right[sample] += buffer->right[sample];
		}
	}

	for (int sample = 0; sample < dest->sampleCount; ++sample)
	{
		dest->left[sample] /= buffer_count;
		dest->right[sample] /= buffer_count;
	}
}


typedef struct
{
	double real;
	double imag;
} Complex;

static void fft(Complex* a, size_t n, int inverse)
{
	// Bit-reversal permutation
	for (size_t i = 1, j = 0; i < n; i++)
	{
		size_t bit = n >> 1;

		while (j & bit)
		{
			j ^= bit;
			bit >>= 1;
		}

		j ^= bit;

		if (i < j)
		{
			Complex temp = a[i];
			a[i] = a[j];
			a[j] = temp;
		}
	}

	// Cooley-Tukey FFT
	for (size_t len = 2; len <= n; len *= 2)
	{
		double angle = 2.0 * 3.14159265358979323846 / len;

		if (!inverse)
			angle = -angle;

		double wlen_real = cos(angle);
		double wlen_imag = sin(angle);

		for (size_t i = 0; i < n; i += len)
		{
			double w_real = 1.0;
			double w_imag = 0.0;

			for (size_t j = 0; j < len / 2; j++)
			{
				Complex u = a[i + j];
				Complex v = a[i + j + len / 2];

				double vr = v.real * w_real - v.imag * w_imag;
				double vi = v.real * w_imag + v.imag * w_real;

				a[i + j].real = u.real + vr;
				a[i + j].imag = u.imag + vi;

				a[i + j + len / 2].real = u.real - vr;
				a[i + j + len / 2].imag = u.imag - vi;

				double next_real =
					w_real * wlen_real - w_imag * wlen_imag;

				double next_imag =
					w_real * wlen_imag + w_imag * wlen_real;

				w_real = next_real;
				w_imag = next_imag;
			}
		}

		// Avoid overflowing len on the final iteration
		if (len == n)
			break;
	}

	// Normalise inverse FFT
	if (inverse)
	{
		for (size_t i = 0; i < n; i++)
		{
			a[i].real /= n;
			a[i].imag /= n;
		}
	}
}

static int fft_convolve(const float* audio, int audio_len, const float* ir,int ir_len,float* output)
{
	if (!audio || !ir || !output ||
		audio_len <= 0 || ir_len <= 0)
		return 0;

	size_t output_len = (size_t)audio_len + (size_t)ir_len - 1;

	// Find the next power of two
	size_t n = 1;

	while (n < output_len)
	{
		if (n > SIZE_MAX / 2)
			return 0;

		n *= 2;
	}

	if (n > SIZE_MAX / sizeof(Complex))
		return 0;

	Complex* a = calloc(n, sizeof(Complex));
	Complex* b = calloc(n, sizeof(Complex));

	if (!a || !b)
	{
		free(a);
		free(b);
		return 0;
	}

	// Copy audio into real components
	for (int i = 0; i < audio_len; i++)
		a[i].real = audio[i];

	// Copy impulse response
	for (int i = 0; i < ir_len; i++)
		b[i].real = ir[i];

	// Transform into frequency domain
	fft(a, n, 0);
	fft(b, n, 0);

	// Complex multiplication
	for (size_t i = 0; i < n; i++)
	{
		double real =
			a[i].real * b[i].real -
			a[i].imag * b[i].imag;

		double imag =
			a[i].real * b[i].imag +
			a[i].imag * b[i].real;

		a[i].real = real;
		a[i].imag = imag;
	}

	// Transform back into time domain
	fft(a, n, 1);

	// Copy output
	for (size_t i = 0; i < output_len; i++)
		output[i] = (float)a[i].real;

	free(a);
	free(b);

	return 1;
}

void clear_audio_buffer(AudioBuffer* buffer)
{
	if (!buffer)
		return;

	free(buffer->left);
	free(buffer->right);

	*buffer = (AudioBuffer){ 0 };
}