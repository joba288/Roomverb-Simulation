// joba288

#include <stdio.h>
#include <stdbool.h>
//#include <libavformat/avformat.h>
//#include <libavcodec/avcodec.h>




#define IR_SAMPLE_RATE 1024 // per second
#define MAX_IR_TIME 10 // seconds
#define MAX_IR_SAMPLES 10240



typedef struct { 
	int x, y, z; 
}ivec3;

typedef struct 
{
	ivec3 pos;
	ivec3 direction;
	const char* filepath;
}Source;

typedef struct 
{
	ivec3 pos;
	ImpulseResponse ir;
}Listener;


typedef struct
{
	ivec3 origin;
	ivec3 direction;
	int energy;
	int distance_travelled;
}Ray;

typedef struct {
	float energy[MAX_IR_SAMPLES]; 
} ImpulseResponse;


typedef struct
{
	ivec3 max;
	ivec3 min;

} AABB;


typedef struct
{
	AABB walls[6];
}Room;

typedef struct
{
	int radius;
	ivec3 pos;
}Sphere;

Room make_room(ivec3 centre, int width, int height, int depth, int thickness);
void emit_rays_from_source(Source source, int count); // Uniform distribution of rays
void trace_ray(Ray ray);

void register_ir_energy(Ray ray, Listener* listener);

bool ray_intersects_sphere(Ray ray, Sphere sphere);
bool ray_instersects_AABB(Ray ray, AABB aabb);


int main()
{
	
	
	Room room = make_room((ivec3) {0,0,0}, 1000, 1000, 1000, 10);


	Source sources[1];
	sources[0].pos = ((ivec3){-500,500,0});

	Listener listener;
	listener.pos = (ivec3){500, 600, 0};
	
	for (size_t i = 0; i < sizeof(sources)/sizeof(Source); ++i)
	{
		emit_rays_from_source(sources[i], 1000);
	}



	return 0;
}

Room make_room(ivec3 centre, int width, int height, int depth, int thickness)
{
	Room room;

	int halfW = width / 2;
	int halfH = height / 2;
	int halfD = depth / 2;

	int x0 = centre.x - halfW;
	int x1 = centre.x + halfW;

	int y0 = centre.y - halfH;
	int y1 = centre.y + halfH;

	int z0 = centre.z - halfD;
	int z1 = centre.z + halfD;

	// FLOOR 
	room.walls[0].min = (ivec3){ x0, y0, z0 };
	room.walls[0].max = (ivec3){ x1, y0 + thickness, z1 };

	// CEILING 
	room.walls[1].min = (ivec3){ x0, y1 - thickness, z0 };
	room.walls[1].max = (ivec3){ x1, y1, z1 };

	// BACK wall 
	room.walls[2].min = (ivec3){ x0, y0, z0 };
	room.walls[2].max = (ivec3){ x1, y1, z0 + thickness };

	// FRONT wall 
	room.walls[3].min = (ivec3){ x0, y0, z1 - thickness };
	room.walls[3].max = (ivec3){ x1, y1, z1 };

	// LEFT wall 
	room.walls[4].min = (ivec3){ x0, y0, z0 };
	room.walls[4].max = (ivec3){ x0 + thickness, y1, z1 };

	// RIGHT wall 
	room.walls[5].min = (ivec3){ x1 - thickness, y0, z0 };
	room.walls[5].max = (ivec3){ x1, y1, z1 };

	return room;
}
void emit_rays_from_source(Source source, int count)
{

}

void trace_ray(Ray ray)
{

	// check if intersect with listener
	// 
	// 
	// find first intersection
	// loop through walls until there is an intersection
	// find point of that intersection
	// reflect ray based off of wall normal
	// reduce energy based off of surface or exceed max stop



}

void register_ir_energy(Ray ray, Listener* listener)
{
	
	// speed of sound 343 m/s

	// time of intersection
	int t = ray.distance_travelled / 343;
	// get that in sample time
	int sample_index = t * IR_SAMPLE_RATE;

	// if that time is within the bounds of capturing
	if (sample_index < MAX_IR_SAMPLES)
	{
		// record ray energy at time of intersection
		listener->ir.energy[sample_index] += ray.energy;
	}
}

bool ray_intersects_sphere(Ray ray, Sphere sphere)
{
	return false;
}

bool ray_instersects_AABB(Ray ray, AABB aabb)
{
	return false;
}




/*
void loadWav()
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
}
*/