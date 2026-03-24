// joba288

#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>



int main()
{
	
	AVFormatContext* format_context = NULL;
	const char* input_filepath = "resources/test.wav";
	
	// Opens file and sets context based on filetype
	if (avformat_open_input(&format_context, input_filepath, NULL, NULL) < 0) {
		fprintf(stderr, "Could not open file\n");
		return -1;
	}
	// Reads header
	avformat_find_stream_info(format_context, NULL);

	// Find audio stream
	AVCodec* codec = NULL;
	int audio_stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);

	// Find decoder and initialise
	AVCodecContext* decoder = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(decoder, format_context->streams[audio_stream_index]->codecpar);
	avcodec_open2(decoder, codec, NULL);

	// Read packets
	AVFrame* frame = av_frame_alloc();
	AVPacket* packet = av_packet_alloc();

	enum AVSampleFormat fmt = decoder->sample_fmt;
	printf("Sample format: %s\n", av_get_sample_fmt_name(fmt));

	while (av_read_frame(format_context, packet) >= 0)
	{
		if (packet->stream_index == audio_stream_index) // Check stream
		{
			avcodec_send_packet(decoder, packet);
			if (avcodec_receive_frame(decoder, frame) == 0) // Frame Ready
			{
				int16_t* samples = (int16_t*)frame->data[0];
				int count = frame->nb_samples;
				
				for (int i = 0; i < count; ++i)
				{
					float current_sample = samples[i] / 32768.0f; // -1 to 1
					if (current_sample > 0.0f)
						printf("Greater than 0: %f\n", current_sample);
				}


			}
		}
		av_packet_unref(packet);
	}


	// Cleanup
	av_frame_free(&frame);
	av_packet_free(&packet);
	avcodec_free_context(&decoder);
	avformat_close_input(&format_context);
	

	return 0;
}