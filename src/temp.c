// joba288

#include "cglm/cglm.h"
#include <stdio.h>
#include <time.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <omp.h>

#include "raylib.h"
#include "raymath.h"
#include "raygui.h"

#define MAX_SOURCES 4

#define SAMPLE_RATE 44100
#define RAY_MAX 500000
#define MAX_BOUNCES 1000
#define EPSILON 0.01f
#define MAX_TIME 2
#define MAX_IR_SAMPLES (MAX_TIME * SAMPLE_RATE)
#define NUM_THREADS 4

#define SPEED_OF_SOUND 343.0f
#define NUM_FREQ_BANDS 6

#define EAR_DISTANCE 0.18f

typedef struct
{
	float* left;
	float* right;
	int sampleCount;
	int sampleRate;
} AudioBuffer;

typedef struct
{
	float* left;
	float* right;
	int bands;
	int sampleCount;
	int sampleRate;
} IRBuffer;



typedef struct
{
	Vector3 origin;
	Vector3 dir;
	float energy[NUM_FREQ_BANDS];
	float distance;
}SoundRay;

typedef struct
{
	Vector3 point;
	Vector3 normal;
	float t;
}SoundRayHit;

typedef struct
{
	Vector3 pos;

	float (*raw_l_ir)[MAX_IR_SAMPLES];
	float (*raw_r_ir)[MAX_IR_SAMPLES];

	AudioBuffer* source_audio;
	AudioBuffer* ir_buffer;
	AudioBuffer* convoluted;
	const char* source_filepath;
	const char* ir_filepath;
	const char* convoluted_filepath;

} AudioSource;



typedef struct
{
	Vector3 centre;
	float radius;
}Sphere;

typedef struct
{
	Vector3 centre;
	Vector3 normal;
	float absorption[NUM_FREQ_BANDS];
	float scattering;
}Plane;


typedef struct
{
	Sphere* head_collider;
	Sphere* l_collider;
	Sphere* r_collider;
	float (*l_ir)[MAX_IR_SAMPLES];
	float (*r_ir)[MAX_IR_SAMPLES];
}Listener;

typedef struct
{
	Vector3 point;
	Listener* listener;
	Plane planes[6];
}Room;


// buffer and ffmpeg stuff
void init_audio_buffer(AudioBuffer* buffer, int samples, int rate);
void free_audio_buffer(AudioBuffer* buffer);
void init_ir_buffer(IRBuffer* ir, int bands, int samples, int rate);
void free_ir_buffer(IRBuffer* ir);
void normalise_ir_buffer(IRBuffer* ir);
void normalise_ir(Listener* listener);

void input_wav_stereo(const char* filepath, AudioBuffer* dest);
void output_wav_stereo(const char* filename, AudioBuffer* buffer);
void ir_buffer_to_audio_buffer(IRBuffer* ir, AudioBuffer* dest);
void audio_buffer_to_ir_buffer(AudioBuffer* buffer, IRBuffer* dest);

void convolute_stereo(AudioBuffer* audio, AudioBuffer* ir, AudioBuffer* dest);

// Raycasting Functions
Vector3 ray_get_pos(float t, const SoundRay* ray);
int ray_hit_sphere(const Sphere* sphere, const SoundRay* ray, float tmin, float tmax, SoundRayHit* hit);
int ray_hit_plane(const Plane* plane, const SoundRay* ray, float tmin, float tmax, SoundRayHit* hit);
Vector3 reflect_ray(Vector3 dir, Vector3 normal);
Vector3 sample_cosine_hemisphere(Vector3 normal);
Vector3 get_dir_fibonacci_sphere(int i, int n);
void emit_rays(int n, Vector3 pos, const Room* room);
void record_impulse(Listener* listener, float distance, float* energy, int ear);
void record_thread_impulse(float(*l_ir)[MAX_IR_SAMPLES], float (*r_ir)[MAX_IR_SAMPLES], float distance, float* energy, int ear);

// redo
void output_impulse_response(const char* filename, Listener* listener);
void impulse_response_to_audio_buffer(Listener* listener, AudioBuffer* buffer); // must change ir to belong to audio source methinks


// pos stuff
void init_room(Room* room, float width, float height, float depth);
void draw_room(Room* room);
void update_listener(Listener* listener); // position
void init_camera(Camera* camera);


int main()
{

	AudioSource sources[MAX_SOURCES];

	sources[0].source_filepath = "resources/sound1.wav";
	sources[0].source_audio = malloc(sizeof(AudioBuffer));
	sources[0].ir_buffer = malloc(sizeof(AudioBuffer));
	sources[0].convoluted = malloc(sizeof(AudioBuffer));
	int selected_source = 0;

	///

	const int screenWidth = 1080; const int screenHeight = 768;
	InitWindow(screenWidth, screenHeight, "Roomverb Sim");

	Camera camera = { 0 }; init_camera(&camera);

	SetTargetFPS(60);
	//

	Vector3 emit_pos = { 0.0f, 1.0f, 0.0f };



	Sphere head =
	{
		.centre = {4.0f, 1.0f, 0.0f},
		.radius = 0.15f
	};

	Sphere left_ear = {
	.centre = {4.0f - (EAR_DISTANCE) / 2, 1.0f, 0.0f},
	.radius = 0.1f
	};

	Sphere right_ear = {
		.centre = {4.0f + (EAR_DISTANCE) / 2, 1.0f, 0.0f},
		.radius = 0.1f
	};

	Listener listener =
	{
		.head_collider = &head,
		.l_collider = &left_ear,
		.r_collider = &right_ear,
	};

	listener.l_ir = calloc(NUM_FREQ_BANDS, sizeof * listener.l_ir);
	listener.r_ir = calloc(NUM_FREQ_BANDS, sizeof * listener.r_ir);


	omp_set_num_threads(NUM_THREADS);
	int rays_per_thread = RAY_MAX / NUM_THREADS;



	Room room = { 0 };
	room.listener = &listener;

	init_room(&room, 1.0f, 1.0f, 1.0f);
	char posX[16] = "1.0";
	char posY[16] = "2.0";
	char posZ[16] = "0.0";
	char lposX[16] = "-1.0";
	char lposY[16] = "2.0";
	char lposZ[16] = "0.0";
	char sclX[16] = "1.0";
	char sclY[16] = "1.0";
	char sclZ[16] = "1.0";

	bool editPosX = false, editPosY = false, editPosZ = false;
	bool editListenerPosX = false, editListenerPosY = false, editListenerPosZ = false;
	bool editSclX = false, editSclY = false, editSclZ = false;
	//
	bool cameraMode = false;

	while (!WindowShouldClose())
	{
		update_listener(&listener);
		if (IsKeyPressed(KEY_E))
		{
			cameraMode = !cameraMode;

			if (cameraMode) DisableCursor();
			else EnableCursor();
		}

		if (cameraMode)
			UpdateCamera(&camera, CAMERA_ORBITAL);

		BeginDrawing();

		ClearBackground(RAYWHITE);

		BeginMode3D(camera);

		//DrawGrid(100, 0.5f);

		DrawSphere(sources[0].pos, 0.15f, RED);

		DrawSphere(listener.head_collider->centre, listener.head_collider->radius, BLUE);
		DrawSphere(listener.l_collider->centre, listener.l_collider->radius, PURPLE);
		DrawSphere(listener.r_collider->centre, listener.r_collider->radius, PURPLE);

		draw_room(&room);



		EndMode3D();

		// GUI


		Rectangle panel = { 20, 20, 140, 260 * 3 };
		GuiPanel(panel, "Transform");

		GuiLabel((Rectangle) { 35, 55, 140, 20 }, "Speaker 1 Position");
		GuiLabel((Rectangle) { 35, 85, 20, 20 }, "X");
		if (GuiTextBox((Rectangle) { 35, 105, 90, 28 }, posX, 15, editPosX)) editPosX = !editPosX;
		GuiLabel((Rectangle) { 35, 145, 20, 20 }, "Y");
		if (GuiTextBox((Rectangle) { 35, 165, 90, 28 }, posY, 15, editPosY)) editPosY = !editPosY;
		GuiLabel((Rectangle) { 35, 205, 20, 20 }, "Z");
		if (GuiTextBox((Rectangle) { 35, 225, 90, 28 }, posZ, 15, editPosZ)) editPosZ = !editPosZ;

		GuiLabel((Rectangle) { 35, 265, 140, 20 }, "Listener Position");
		GuiLabel((Rectangle) { 35, 295, 20, 20 }, "X");
		if (GuiTextBox((Rectangle) { 35, 315, 90, 28 }, lposX, 15, editListenerPosX)) editListenerPosX = !editListenerPosX;
		GuiLabel((Rectangle) { 35, 355, 20, 20 }, "Y");
		if (GuiTextBox((Rectangle) { 35, 375, 90, 28 }, lposY, 15, editListenerPosY)) editListenerPosY = !editListenerPosY;
		GuiLabel((Rectangle) { 35, 415, 20, 20 }, "Z");
		if (GuiTextBox((Rectangle) { 35, 435, 90, 28 }, lposZ, 15, editListenerPosZ)) editListenerPosZ = !editListenerPosZ;

		GuiLabel((Rectangle) { 35, 475, 140, 20 }, "Room Scale");
		GuiLabel((Rectangle) { 35, 505, 20, 20 }, "X");
		if (GuiTextBox((Rectangle) { 35, 525, 90, 28 }, sclX, 15, editSclX)) editSclX = !editSclX;
		GuiLabel((Rectangle) { 35, 565, 20, 20 }, "Y");
		if (GuiTextBox((Rectangle) { 35, 585, 90, 28 }, sclY, 15, editSclY)) editSclY = !editSclY;
		GuiLabel((Rectangle) { 35, 625, 20, 20 }, "Z");
		if (GuiTextBox((Rectangle) { 35, 645, 90, 28 }, sclZ, 15, editSclZ)) editSclZ = !editSclZ;

		sources[0].pos.x = (float)atof(posX);
		sources[0].pos.y = (float)atof(posY);
		sources[0].pos.z = (float)atof(posZ);

		listener.head_collider->centre.x = (float)atof(lposX);
		listener.head_collider->centre.y = (float)atof(lposY);
		listener.head_collider->centre.z = (float)atof(lposZ);

		Vector3 scale;
		scale.x = (float)atof(sclX);
		scale.y = (float)atof(sclY);
		scale.z = (float)atof(sclZ);

		if (editSclX | editSclY | editSclZ) init_room(&room, scale.x, scale.y, scale.z);

		DrawText(TextFormat("Pos: %.2f %.2f %.2f", sources[0].pos.x, sources[0].pos.y, sources[0].pos.z), 340, 80, 20, DARKGRAY);
		DrawText(TextFormat("Scale: %.2f %.2f %.2f", scale.x, scale.y, scale.z), 340, 115, 20, DARKGRAY);





		EndDrawing();
	}

	CloseWindow();






	//// GENERATING IMPULSE RESPONSE
	printf("Max threads: %d\n", omp_get_max_threads());
	srand(0);

	emit_rays(RAY_MAX, sources[0].pos, &room);
	printf("rays emitted");
	normalise_ir(&listener);
	printf("normalised");
	impulse_response_to_audio_buffer(&listener, sources[0].ir_buffer);
	printf("converted");
	import_audio_buffer(sources[0].source_filepath, sources[0].source_audio);
	printf("imported");
	convolute_buffer(sources[0].source_audio, sources[0].ir_buffer, sources[0].convoluted);
	printf("convoluted");
	output_audio_buffer("ir.wav", sources[0].ir_buffer);
	output_audio_buffer("convoluted.wav", sources[0].convoluted);
	printf("outputted");

	free(listener.l_ir);
	free(listener.r_ir);

	free_audio_buffer(sources[0].source_audio);
	free_audio_buffer(sources[0].ir_buffer);
	free_audio_buffer(sources[0].convoluted);


	return 0;
}

void init_audio_buffer(AudioBuffer* buffer, int samples, int rate) {}// to write
void free_audio_buffer(AudioBuffer* buffer)
{
	free(buffer->left);
	free(buffer->right);
	free(buffer);
}
void init_ir_buffer(IRBuffer* ir, int bands, int samples, int rate) {}// to write
void free_ir_buffer(IRBuffer* ir) {}// to write
void normalise_ir_buffer(IRBuffer* ir) {}// to write
void normalise_ir(Listener* listener)
{
	float max = 0.0f;

	// find peak
	for (int b = 0; b < NUM_FREQ_BANDS; b++) {
		for (int i = 0; i < MAX_IR_SAMPLES; i++) {
			float l = fabsf(listener->l_ir[b][i]);
			float r = fabsf(listener->r_ir[b][i]);

			if (l > max) max = l;
			if (r > max) max = r;
		}
	}

	if (max == 0.0f) return;

	// normalise
	for (int b = 0; b < NUM_FREQ_BANDS; b++) {
		for (int i = 0; i < MAX_IR_SAMPLES; i++) {
			listener->l_ir[b][i] /= max;
			listener->r_ir[b][i] /= max;
		}
	}
}
// to write
void input_wav_stereo(const char* filepath, AudioBuffer* dest)
{
	AVFormatContext* format_ctx = NULL;
	AVCodecContext* decoder_ctx = NULL;
	const AVCodec* decoder = NULL;
	SwrContext* swr = NULL;
	AVPacket* pkt = NULL;
	AVFrame* frame = NULL;

	int audio_stream_index = -1;

	dest->left = NULL;
	dest->right = NULL;
	dest->sampleCount = 0;
	dest->sampleRate = SAMPLE_RATE;

	/* Open file */
	if (avformat_open_input(&format_ctx, filepath, NULL, NULL) < 0)
		return;

	if (avformat_find_stream_info(format_ctx, NULL) < 0)
		goto cleanup;

	/* Find first audio stream */
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

	/* Resample / convert to stereo float @ SAMPLE_RATE */
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

	int capacity = SAMPLE_RATE; /* initial 1 second */

	dest->left = malloc(sizeof(float) * capacity);
	dest->right = malloc(sizeof(float) * capacity);

	if (!dest->left || !dest->right)
		goto cleanup;

	while (av_read_frame(format_ctx, pkt) >= 0)
	{
		if (pkt->stream_index == audio_stream_index)
		{
			if (avcodec_send_packet(decoder_ctx, pkt) == 0)
			{
				while (avcodec_receive_frame(decoder_ctx, frame) == 0)
				{
					int out_samples = av_rescale_rnd(
						swr_get_delay(swr, decoder_ctx->sample_rate) + frame->nb_samples,
						SAMPLE_RATE,
						decoder_ctx->sample_rate,
						AV_ROUND_UP
					);

					uint8_t* out_buf = NULL;
					int out_linesize = 0;

					if (av_samples_alloc(
						&out_buf,
						&out_linesize,
						2,
						out_samples,
						AV_SAMPLE_FMT_FLT,
						0) < 0)
					{
						goto cleanup;
					}

					int samples_written = swr_convert(
						swr,
						&out_buf,
						out_samples,
						(const uint8_t**)frame->extended_data,
						frame->nb_samples
					);

					float* pcm = (float*)out_buf;

					/* Grow buffer if needed */
					while (dest->sampleCount + samples_written > capacity)
					{
						capacity *= 2;

						dest->left = realloc(dest->left, sizeof(float) * capacity);
						dest->right = realloc(dest->right, sizeof(float) * capacity);

						if (!dest->left || !dest->right)
							goto cleanup;
					}

					for (int i = 0; i < samples_written; i++)
					{
						dest->left[dest->sampleCount] = pcm[i * 2 + 0];
						dest->right[dest->sampleCount] = pcm[i * 2 + 1];
						dest->sampleCount++;
					}

					av_freep(&out_buf);
				}
			}
		}

		av_packet_unref(pkt);
	}

cleanup:

	if (pkt) av_packet_free(&pkt);
	if (frame) av_frame_free(&frame);
	if (swr) swr_free(&swr);
	if (decoder_ctx) avcodec_free_context(&decoder_ctx);
	if (format_ctx) avformat_close_input(&format_ctx);
} // to write// to write
void output_wav_stereo(const char* filename, AudioBuffer* buffer)
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

	/* convert float [-1.0,1.0] to interleaved int16 stereo */
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
}// to write
void ir_buffer_to_audio_buffer(IRBuffer* ir, AudioBuffer* dest) {}// to write
void audio_buffer_to_ir_buffer(AudioBuffer* buffer, IRBuffer* dest) {}// to write
void convolute_stereo(AudioBuffer* audio, AudioBuffer* ir, AudioBuffer* dest)
{

	int convoluted_sample_count =
		audio->sampleCount + ir->sampleCount - 1;

	dest->left = calloc(convoluted_sample_count, sizeof(float));
	dest->right = calloc(convoluted_sample_count, sizeof(float));

	if (!dest->left || !dest->right)
		return;

	dest->sampleCount = convoluted_sample_count;
	dest->sampleRate = audio->sampleRate;

	for (int i = 0; i < audio->sampleCount; i++)
	{
		float inL = audio->left[i];
		float inR = audio->right[i];

		for (int k = 0; k < ir->sampleCount; k++)
		{
			dest->left[i + k] += inL * ir->left[k];
			dest->right[i + k] += inR * ir->right[k];
		}
	}


	for (int i = 0; i < convoluted_sample_count; i++)
	{
		if (dest->left[i] > 1.0f) dest->left[i] = 1.0f;
		if (dest->left[i] < -1.0f) dest->left[i] = -1.0f;

		if (dest->right[i] > 1.0f) dest->right[i] = 1.0f;
		if (dest->right[i] < -1.0f) dest->right[i] = -1.0f;
	}
}

Vector3 ray_get_pos(float t, const SoundRay* ray)
{
	// origin + t * dir
	return Vector3Add(ray->origin, Vector3Scale(ray->dir, t));
}
int ray_hit_sphere(const Sphere* sphere, const SoundRay* ray, float tmin, float tmax, SoundRayHit* hit)
{
	Vector3 oc = Vector3Subtract(sphere->centre, ray->origin);

	float a = Vector3DotProduct(ray->dir, ray->dir);
	float h = Vector3DotProduct(ray->dir, oc);
	float c = Vector3DotProduct(oc, oc) - sphere->radius * sphere->radius;
	float discriminant = h * h - a * c;
	if (discriminant <= 0) return 0; // less than two roots

	float sqrtd = sqrt(discriminant);

	// nearest root 
	float root = (h - sqrtd) / a;
	if (root <= tmin || root >= tmax) {
		root = (h + sqrtd) / a;
		if (root <= tmin || root >= tmax)
			return 0;
	}

	hit->t = root;
	hit->point = ray_get_pos(hit->t, ray);
	hit->normal = Vector3Scale(Vector3Subtract(hit->point, sphere->centre), 1 / sphere->radius);


	return 1;
}
int ray_hit_plane(const Plane* plane, const SoundRay* ray, float tmin, float tmax, SoundRayHit* hit)
{
	float denom = Vector3DotProduct(plane->normal, ray->dir);
	// parrallel
	if (fabs(denom) < 1e-6f)
		return 0;

	Vector3 diff = Vector3Subtract(plane->centre, ray->origin);

	float t = Vector3DotProduct(diff, plane->normal) / denom;

	if (t < tmin || t > tmax)
		return 0;

	hit->t = t;
	hit->point = ray_get_pos(t, ray);

	if (denom > 0.0f)
	{
		hit->normal = Vector3Negate(plane->normal);
	}
	else
	{
		hit->normal = plane->normal;
	}
	return 1;

}
Vector3 reflect_ray(Vector3 dir, Vector3 normal)
{
	float dot = Vector3DotProduct(dir, normal);
	Vector3 temp = Vector3Scale(normal, 2.0f * dot);
	Vector3 result = Vector3Subtract(dir, temp);
	result = Vector3Normalize(result);
	return result;
}
Vector3 sample_cosine_hemisphere(Vector3 normal)
{
	float xi1 = (float)rand() / RAND_MAX;
	float xi2 = (float)rand() / RAND_MAX;

	float theta = acosf(sqrtf(xi1));
	float phi = 2.0f * M_PI * xi2;

	float x = sinf(theta) * cosf(phi);
	float y = sinf(theta) * sinf(phi);
	float z = cosf(theta);


	Vector3 tangent;
	if (fabs(normal.y) < 0.999f)
	{
		Vector3 up = { 0.0f, 1.0f, 0.0f };
		tangent = Vector3CrossProduct(up, normal);
	}
	else
	{
		Vector3 right = { 1.0f, 0.0f, 0.0f };
		tangent = Vector3CrossProduct(right, normal);
	}

	tangent = Vector3Normalize(tangent);

	Vector3 bitangent = Vector3CrossProduct(normal, tangent);

	// world space
	Vector3 result;
	Vector3 temp;
	result = Vector3Scale(tangent, x);
	temp = Vector3Scale(bitangent, y);
	result = Vector3Add(result, temp);
	temp = Vector3Scale(normal, z);
	result = Vector3Add(result, temp);
	result = Vector3Normalize(result);

	return result;
}
Vector3 get_dir_fibonacci_sphere(int i, int n)
{
	Vector3 result;
	const float phi = M_PI * (3.0f - sqrtf(5.0f));

	result.y = 1.0f - (2.0f * (float)i + 1.0f) / (float)n;
	float r = sqrtf(1.0f - result.y * result.y);

	float theta = phi * i;

	result.x = cosf(theta) * r;
	result.z = sinf(theta) * r;

	return result;

}
void emit_rays(int n, Vector3 pos, const Room* room)
{
	// each thread gets its own IR
	int max_threads = omp_get_max_threads();

	float (*thread_l_ir)[NUM_FREQ_BANDS][MAX_IR_SAMPLES] = calloc(max_threads, sizeof * thread_l_ir);
	float (*thread_r_ir)[NUM_FREQ_BANDS][MAX_IR_SAMPLES] = calloc(max_threads, sizeof * thread_r_ir);

	int count = 0;
	int listener_hit_count = 0;
	int absorbed_count = 0;

	float air_absorption[NUM_FREQ_BANDS] = {
	0.0001f, // 125 Hz
	0.0002f,
	0.0005f,
	0.001f,
	0.002f,
	0.004f  // 4 kHz
	};


	int i = 0;
#pragma omp parallel for
	for (i = 0; i < n; ++i)
	{
		int thread_id = omp_get_thread_num();
		float (*l_ir)[MAX_IR_SAMPLES] = thread_l_ir[thread_id];
		float (*r_ir)[MAX_IR_SAMPLES] = thread_r_ir[thread_id];

		SoundRay ray;
		SoundRayHit hit;
		ray.origin = pos;

		ray.dir = get_dir_fibonacci_sphere(i, n);
		ray.dir = Vector3Normalize(ray.dir);

		for (int b = 0; b < NUM_FREQ_BANDS; ++b)
		{
			ray.energy[b] = 1.0f;
		}
		ray.distance = 0.0f;


		for (int bounce = 0; bounce < MAX_BOUNCES; ++bounce)
		{


			SoundRayHit closest_hit = { 0 };
			float closest_t = 1e9;
			int hit_type = 0;
			Plane* hit_plane = NULL;
			SoundRayHit temp;

			// listener hit
			// left
			if (ray_hit_sphere(room->listener->l_collider, &ray, EPSILON, 10000, &temp))
			{
				if (temp.t < closest_t)
				{
					closest_t = temp.t;
					closest_hit = temp;
					hit_type = 1;
				}
			}

			// right
			if (ray_hit_sphere(room->listener->r_collider, &ray, EPSILON, 10000, &temp))
			{
				if (temp.t < closest_t)
				{
					closest_t = temp.t;
					closest_hit = temp;
					hit_type = 2;
				}
			}



			// find closest plane hit
			for (int plane = 0; plane < 6; ++plane)
			{
				if (ray_hit_plane(&room->planes[plane], &ray, EPSILON, 10000, &temp))
				{
					if (temp.t < closest_t)
					{
						closest_hit = temp;
						closest_t = temp.t;
						hit_plane = (Plane*)&room->planes[plane];
						hit_type = 3;
					}

				}
			}


			if (hit_type == 1 || hit_type == 2)
			{
				// listener hit
				//printf("=================================Listener Hit\n");
				listener_hit_count++;

				// record ir
				float total_distance = ray.distance + closest_hit.t;
				record_thread_impulse(l_ir, r_ir, total_distance, ray.energy, hit_type);
				break;

			}
			else if (hit_type == 3)
			{
				// plane hit
				// 
				// 
				// 

				//printf("Collision of ray %d: %d\n", i, count);



				count++;


				// absorb
				float s = hit_plane->scattering;
				int dead = 1;
				for (int b = 0; b < NUM_FREQ_BANDS; ++b)
				{
					float alpha = hit_plane->absorption[b];
					ray.energy[b] *= (1.0f - alpha);

					if (ray.energy[b] > EPSILON)
					{
						dead = 0;
					}
				}


				if (dead)
				{
					absorbed_count++;
					break;
				}

				// bounce
				float xi = (float)rand() / RAND_MAX;
				Vector3 reflected_dir;

				if (xi < 1.0f - s)
				{
					// specular reflection
					reflected_dir = reflect_ray(ray.dir, closest_hit.normal);
				}
				else
				{
					// diffuse
					reflected_dir = sample_cosine_hemisphere(closest_hit.normal);
				}

				ray.distance += closest_hit.t;

				ray.origin = closest_hit.point;
				Vector3 offset = Vector3Scale(reflected_dir, EPSILON);
				ray.origin = Vector3Add(ray.origin, offset);
				reflected_dir = Vector3Normalize(reflected_dir);
				ray.dir = reflected_dir;

				for (int b = 0; b < NUM_FREQ_BANDS; ++b)
				{
					float beta = air_absorption[b];
					ray.energy[b] *= expf(-beta * closest_hit.t);
				}

			}
			else
			{
				break;
			}



		}





	}
	printf("%d\n", omp_get_thread_num());
	//merge threads

	for (int t = 0; t < max_threads; t++) {
		for (int b = 0; b < NUM_FREQ_BANDS; b++) {
			for (int i = 0; i < MAX_IR_SAMPLES; i++) {
				room->listener->l_ir[b][i] += thread_l_ir[t][b][i];
				room->listener->r_ir[b][i] += thread_r_ir[t][b][i];
			}
		}
	}



	free(thread_l_ir);
	free(thread_r_ir);

	printf("Raycasting done\n");

}
void record_impulse(Listener* listener, float distance, float* energy, int ear)
{
	float (*ir_hit)[MAX_IR_SAMPLES] = (ear == 1) ? listener->l_ir : listener->r_ir;


	float time = distance / SPEED_OF_SOUND;

	if (time > MAX_TIME)
		return;

	int sample_time = (int)(time * SAMPLE_RATE);

	if (sample_time >= 0 && sample_time < MAX_IR_SAMPLES)
	{
		for (int b = 0; b < NUM_FREQ_BANDS; ++b)
		{
			float attenuated = energy[b];// (4.0f * M_PI * distance * distance);
			ir_hit[b][sample_time] += attenuated;
		}
	}
}
void record_thread_impulse(float(*l_ir)[MAX_IR_SAMPLES], float(*r_ir)[MAX_IR_SAMPLES], float distance, float* energy, int ear)
{
	float (*ir_hit)[MAX_IR_SAMPLES] = (ear == 1) ? l_ir : r_ir;

	float time = distance / SPEED_OF_SOUND;

	if (time > MAX_TIME)
		return;

	int sample_time = (int)(time * SAMPLE_RATE);

	if (sample_time >= 0 && sample_time < MAX_IR_SAMPLES)
	{
		for (int b = 0; b < NUM_FREQ_BANDS; ++b)
		{
			float attenuated = energy[b];// (4.0f * M_PI * distance * distance);
			ir_hit[b][sample_time] += attenuated;
		}
	}

}


void output_impulse_response(const char* filename, Listener* listener)
{
	AVFormatContext* format_ctx = NULL;
	AVStream* stream = NULL;
	AVCodecContext* encoder_ctx = NULL;
	AVCodec* encoder = NULL;

	avformat_alloc_output_context2(&format_ctx, NULL, "wav", filename);
	if (!format_ctx)
		return;

	encoder = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE);
	if (!encoder)
		return;

	stream = avformat_new_stream(format_ctx, NULL);
	if (!stream)
		return;
	stream->time_base = (AVRational){ 1, SAMPLE_RATE };

	encoder_ctx = avcodec_alloc_context3(encoder);
	encoder_ctx->codec_id = AV_CODEC_ID_PCM_S16LE;
	encoder_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
	encoder_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
	encoder_ctx->sample_rate = SAMPLE_RATE;

	av_channel_layout_default(&encoder_ctx->ch_layout, 2);
	encoder_ctx->ch_layout.nb_channels = 2;
	avcodec_open2(encoder_ctx, encoder, NULL);
	avcodec_parameters_from_context(stream->codecpar, encoder_ctx);

	avio_open(&format_ctx->pb, filename, AVIO_FLAG_WRITE);
	avformat_write_header(format_ctx, NULL);

	// WAV Content

	// convert float to int and collapse bands
	int16_t* pcm = malloc(MAX_IR_SAMPLES * 2 * sizeof(int16_t));

	for (int i = 0; i < MAX_IR_SAMPLES; i++) {

		float x = 0.0f;

		float l = 0.0f;
		float r = 0.0f;

		for (int b = 0; b < NUM_FREQ_BANDS; b++) {
			l += sqrtf(listener->l_ir[b][i]);
			r += sqrtf(listener->r_ir[b][i]);
		}
		l /= NUM_FREQ_BANDS;
		r /= NUM_FREQ_BANDS;

		l = fmaxf(-1.0f, fminf(1.0f, l));
		r = fmaxf(-1.0f, fminf(1.0f, r));

		pcm[i * 2 + 0] = (int16_t)(l * 32767.0f);
		pcm[i * 2 + 1] = (int16_t)(r * 32767.0f);
	}

	int data_size = MAX_IR_SAMPLES * 2 * sizeof(int16_t);
	AVPacket* pkt = av_packet_alloc();
	pkt->data = (uint8_t*)pcm;
	pkt->size = data_size;
	pkt->stream_index = stream->index;
	pkt->pts = 0;
	pkt->dts = 0;
	pkt->duration = MAX_IR_SAMPLES;
	av_write_frame(format_ctx, pkt);
	av_packet_free(&pkt);

	free(pcm);
	//
	av_write_trailer(format_ctx);
	avcodec_free_context(&encoder_ctx);
	avio_close(format_ctx->pb);
	avformat_free_context(format_ctx);

	printf("Outputted");


}
void impulse_response_to_audio_buffer(Listener* listener, AudioBuffer* buffer)
{

	buffer->left = malloc(MAX_IR_SAMPLES * sizeof(float));
	buffer->right = malloc(MAX_IR_SAMPLES * sizeof(float));
	buffer->sampleCount = MAX_IR_SAMPLES;
	buffer->sampleRate = SAMPLE_RATE;

	for (int i = 0; i < MAX_IR_SAMPLES; i++) {

		float x = 0.0f;

		float l = 0.0f;
		float r = 0.0f;

		for (int b = 0; b < NUM_FREQ_BANDS; b++) {
			l += sqrtf(listener->l_ir[b][i]);
			r += sqrtf(listener->r_ir[b][i]);
		}
		l /= NUM_FREQ_BANDS;
		r /= NUM_FREQ_BANDS;

		l = fmaxf(-1.0f, fminf(1.0f, l));
		r = fmaxf(-1.0f, fminf(1.0f, r));

		buffer->left[i] = l;
		buffer->right[i] = r;
	}

}


void init_camera(Camera* camera)
{
	camera->position = (Vector3){ 10.0f, 10.0f, 10.0f }; // Camera position
	camera->target = (Vector3){ 0.0f, 0.0f, 0.0f };      // Camera looking at point
	camera->up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
	camera->fovy = 45.0f;                                // Camera field-of-view Y
	camera->projection = CAMERA_PERSPECTIVE;             // Camera projection type
}
void init_room(Room* room, float width, float height, float depth)
{
	float hw = width * 0.5f;
	float hd = depth * 0.5f;

	// floor at y = 0
	// ceiling at y = height
	float floor_y = 0.0f;
	float ceil_y = height;

	// room vertical center becomes height * 0.5
	float cy = height * 0.5f;

	float wall_absorption[NUM_FREQ_BANDS] = { 0.01f, 0.01f, 0.02f, 0.02f, 0.02f, 0.02f };
	float floor_absorption[NUM_FREQ_BANDS] = { 0.08f, 0.24f, 0.57f, 0.69f, 0.71f, 0.73f };
	float ceil_absorption[NUM_FREQ_BANDS] = { 0.08f, 0.24f, 0.57f, 0.69f, 0.71f, 0.73f };

	float wall_scatter = 0.9f;
	float floor_scatter = 0.9f;
	float ceil_scatter = 0.9f;

	// left wall
	room->planes[0] = (Plane){
		.centre = { -hw, cy, 0.0f },
		.normal = { 1.0f, 0.0f, 0.0f },
		.scattering = wall_scatter
	};
	memcpy(room->planes[0].absorption, wall_absorption, sizeof(wall_absorption));

	// right wall
	room->planes[1] = (Plane){
		.centre = { hw, cy, 0.0f },
		.normal = { -1.0f, 0.0f, 0.0f },
		.scattering = wall_scatter
	};
	memcpy(room->planes[1].absorption, wall_absorption, sizeof(wall_absorption));

	// back wall
	room->planes[2] = (Plane){
		.centre = { 0.0f, cy, -hd },
		.normal = { 0.0f, 0.0f, 1.0f },
		.scattering = wall_scatter
	};
	memcpy(room->planes[2].absorption, wall_absorption, sizeof(wall_absorption));

	// front wall
	room->planes[3] = (Plane){
		.centre = { 0.0f, cy, hd },
		.normal = { 0.0f, 0.0f, -1.0f },
		.scattering = wall_scatter
	};
	memcpy(room->planes[3].absorption, wall_absorption, sizeof(wall_absorption));

	// floor
	room->planes[4] = (Plane){
		.centre = { 0.0f, floor_y, 0.0f },
		.normal = { 0.0f, 1.0f, 0.0f },
		.scattering = floor_scatter
	};
	memcpy(room->planes[4].absorption, floor_absorption, sizeof(floor_absorption));

	// ceiling
	room->planes[5] = (Plane){
		.centre = { 0.0f, ceil_y, 0.0f },
		.normal = { 0.0f, -1.0f, 0.0f },
		.scattering = ceil_scatter
	};
	memcpy(room->planes[5].absorption, ceil_absorption, sizeof(ceil_absorption));
}
void draw_room(Room* room)
{
	for (int i = 0; i < 6; i++)
	{
		const Plane* p = &room->planes[i];

		Color color;

		switch (i)
		{
		case 0: color = Fade(YELLOW, 0.35f); break;
		case 1: color = Fade(YELLOW, 0.35f); break;
		case 2: color = Fade(GREEN, 0.35f); break;
		case 3: color = Fade(GREEN, 0.35f); break;
		case 4: color = Fade(GRAY, 0.35f); break;
		case 5: color = Fade(GRAY, 0.35f); break;
		default: color = Fade(DARKPURPLE, 0.35f); break;
		}


		// perpendicular to x
		if (fabsf(p->normal.x) > 0.9f)
		{
			float height = fabsf(room->planes[5].centre.y - room->planes[4].centre.y);
			float depth = fabsf(room->planes[3].centre.z - room->planes[2].centre.z);

			DrawCubeV(
				p->centre,
				(Vector3) {
				0.02f, height, depth
			},
				color
			);


		}
		// floor / ceiling
		else if (fabsf(p->normal.y) > 0.9f)
		{
			float width = fabsf(room->planes[1].centre.x - room->planes[0].centre.x);
			float depth = fabsf(room->planes[3].centre.z - room->planes[2].centre.z);

			DrawCubeV(
				p->centre,
				(Vector3) {
				width, 0.02f, depth
			},
				color
			);


		}
		// front / back wall
		else if (fabsf(p->normal.z) > 0.9f)
		{
			float width = fabsf(room->planes[1].centre.x - room->planes[0].centre.x);
			float height = fabsf(room->planes[5].centre.y - room->planes[4].centre.y);

			DrawCubeV(
				p->centre,
				(Vector3) {
				width, height, 0.02f
			},
				color
			);


		}
	}
}

void update_listener(Listener* listener)
{
	listener->l_collider->centre = (Vector3){ listener->head_collider->centre.x - (EAR_DISTANCE) / 2, listener->head_collider->centre.y, listener->head_collider->centre.z };
	listener->r_collider->centre = (Vector3){ listener->head_collider->centre.x + (EAR_DISTANCE) / 2, listener->head_collider->centre.y, listener->head_collider->centre.z };

}
