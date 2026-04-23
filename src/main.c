// joba288

#include "cglm/cglm.h"
#include <stdio.h>
#include <time.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#define SAMPLE_RATE 44100
#define RAY_MAX 100000
#define MAX_BOUNCES 1000
#define EPSILON 0.01f
#define MAX_TIME 3
#define MAX_IR_SAMPLES (MAX_TIME * SAMPLE_RATE)

#define SPEED_OF_SOUND 343.0f
#define NUM_FREQ_BANDS 6


typedef struct
{
	vec3 origin;
	vec3 dir;
	float energy[NUM_FREQ_BANDS];
	float distance;
}Ray;

typedef struct
{
	vec3 point;
	vec3 normal;
	float t;
}RayHit;

typedef struct
{
	vec3 centre;
	float radius;
}Sphere;

typedef struct
{
	vec3 centre;
	vec3 normal;
	float absorption[NUM_FREQ_BANDS];
	float scattering;
}Plane;


typedef struct
{
	Sphere* sphere;
	float (*ir)[MAX_IR_SAMPLES];
}Listener;

typedef struct
{
	vec3 point;
	Listener* listener;
	Plane planes[6];
}Room;



void generate_room(Room* room, float width, float height, float depth);
void ray_get_pos(float t, const Ray* ray, vec3 dest);
void reflect(vec3 dir, vec3 normal, vec3 reflected);
void sample_cosine_hemisphere(vec3 normal, vec3 dest);
int ray_hit_sphere(const Sphere* sphere, const Ray* ray, float tmin, float tmax, RayHit* hit);
int ray_hit_plane(const Plane* plane, const Ray* ray, float tmin, float tmax, RayHit* hit);
void emit_rays(int n, vec3 pos, const Room* room);
void get_dir_fibonacci_sphere(int i, int n, vec3 dest);
void record_impulse(Listener* listener, float distance, float* energy);
void output_impulse_response(const char* filename, Listener* listener);

void normalise_ir(Listener* listener);

int main()
{
	vec3 emit_pos = { 0.0f, 2.0f, 0.0f };
	
	Sphere sphere =
	{
		.centre = {9.0f, 2.0f, 0.0f},
		.radius = 4.0f
	};

	Listener listener =
	{
		.sphere = &sphere
	};

	listener.ir = calloc(NUM_FREQ_BANDS, sizeof * listener.ir);


	Room room = { 0 };
	room.listener = &listener;

	generate_room(&room, 20.0f, 200.0f, 20.0f);

	srand(0);

	emit_rays(RAY_MAX, emit_pos, &room);
	normalise_ir(&listener);
	output_impulse_response("test.wav", &listener);

	free(listener.ir);

	return 0;
}




void ray_get_pos(float t, const Ray* ray, vec3 dest)
{
	// origin + t * dir
	glm_vec3_scale(ray->dir, t, dest);
	glm_vec3_add(dest, ray->origin, dest);
}

int ray_hit_sphere(const Sphere* sphere, const Ray* ray, float tmin, float tmax, RayHit* hit)
{
	vec3 oc;
	glm_vec3_sub(sphere->centre, ray->origin, oc);

	float a = glm_vec3_norm2(ray->dir);
	float h = glm_dot(ray->dir, oc);
	float c = glm_vec3_norm2(oc) - sphere->radius * sphere->radius;
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
	ray_get_pos(hit->t, ray, hit->point);
	glm_vec3_sub(hit->point, sphere->centre, hit->normal);
	glm_vec3_scale(hit->normal, 1 / sphere->radius, hit->normal);


	return 1;
}

int ray_hit_plane(const Plane* plane, const Ray* ray, float tmin, float tmax, RayHit* hit)
{
	float denom = glm_vec3_dot(plane->normal, ray->dir);

	// parrallel
	if (fabs(denom) < 1e-6f)
		return 0;

	vec3 diff;
	glm_vec3_sub(plane->centre, ray->origin, diff);

	float t = glm_vec3_dot(diff, plane->normal) / denom;

	if (t < tmin || t > tmax)
		return 0;

	hit->t = t;
	ray_get_pos(t, ray, hit->point);

	if (denom > 0.0f)
	{
		glm_vec3_negate_to(plane->normal, hit->normal);
	}
	else
	{
		glm_vec3_copy(plane->normal, hit->normal);
	}
	return 1;

}

void emit_rays(int n, vec3 pos, const Room* room)
{
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


	for (int i = 0; i < n; ++i)
	{
		Ray ray;
		RayHit hit;
		glm_vec3_copy(pos, ray.origin);
		get_dir_fibonacci_sphere(i, n, ray.dir);
		glm_vec3_normalize(ray.dir);
		for (int b = 0; b < NUM_FREQ_BANDS; ++b)
		{
			ray.energy[b] = 1.0f;
		}
		ray.distance = 0.0f;

		for (int bounce = 0; bounce < MAX_BOUNCES; ++bounce)
		{

			
			RayHit closest_hit = {0};
			float closest_t = 1e9;
			int hit_type = 0;
			Plane* hit_plane = NULL;
			RayHit temp;

			// listener hit
			if (ray_hit_sphere(room->listener->sphere, &ray, EPSILON, 10000, &temp))
			{
				if (temp.t < closest_t)
				{
					closest_t = temp.t;
					closest_hit = temp;
					hit_type = 1;
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
						hit_type = 2;
					}

				}
			}

			if (hit_type == 1)
			{
				// listener hit
				//printf("=================================Listener Hit\n");
				listener_hit_count++;

				// record ir
				float total_distance = ray.distance + closest_hit.t;
				record_impulse(room->listener, total_distance, ray.energy);
				break;
				

			}
			else if (hit_type == 2)
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
				vec3 reflected_dir;

				if (xi < 1.0f - s)
				{
					// specular reflection
					reflect(ray.dir, closest_hit.normal, reflected_dir);
				}
				else
				{
					// diffuse
					sample_cosine_hemisphere(closest_hit.normal, reflected_dir);
				}

				ray.distance += closest_hit.t;


				glm_vec3_copy(closest_hit.point, ray.origin);
				vec3 offset;
				glm_vec3_scale(reflected_dir, EPSILON, offset);
				glm_vec3_add(ray.origin, offset, ray.origin);
				glm_vec3_normalize(reflected_dir);
				glm_vec3_copy(reflected_dir, ray.dir);
				
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
	printf("Raycasting done\n");
	for (int i = 0; i < MAX_IR_SAMPLES; ++i)
	{
		//printf("%f\n", room->listener->ir[i]);
	}

	printf("Collision Count:    %d\n", count);
	printf("Listener Hit Count:    %d\n", listener_hit_count);
	printf("absorbed Count:    %d\n", absorbed_count);

}

void get_dir_fibonacci_sphere(int i, int n, vec3 dest)
{
	const float phi = M_PI * (3.0f - sqrtf(5.0f)); 

	dest[1] = 1.0f - (2.0f * (float)i + 1.0f) / (float)n;  
	float r = sqrtf(1.0f - dest[1] * dest[1]);

	float theta = phi * i;

	dest[0] = cosf(theta) * r;
	dest[2] = sinf(theta) * r;

}

void record_impulse(Listener* listener, float distance, float* energy)
{
	float time = distance / SPEED_OF_SOUND;

	if (time > MAX_TIME)
		return;

	int sample_time = (int)(time * SAMPLE_RATE);

	if (sample_time >= 0 && sample_time < MAX_IR_SAMPLES)
	{
		for (int b = 0; b < NUM_FREQ_BANDS; ++b)
		{
			float attenuated = energy[b];// (4.0f * M_PI * distance * distance);
			listener->ir[b][sample_time] += attenuated;
		}
	}
}




void reflect(vec3 dir, vec3 normal, vec3 reflected)
{
	float dot = glm_vec3_dot(dir, normal);
	vec3 temp;
	glm_vec3_scale(normal, 2.0f*dot, temp);
	glm_vec3_sub(dir, temp, reflected);
	glm_vec3_normalize(reflected);
}


void sample_cosine_hemisphere(vec3 normal, vec3 dest)
{
	float xi1 = (float)rand() / RAND_MAX;
	float xi2 = (float)rand() / RAND_MAX;

	float theta = acosf(sqrtf(xi1));
	float phi = 2.0f * M_PI * xi2;

	float x = sinf(theta) * cosf(phi);
	float y = sinf(theta) * sinf(phi);
	float z = cosf(theta);

	vec3 up = { 0.0f, 1.0f, 0.0f };
	vec3 tangent;
	if (fabs(normal[1]) < 0.999f)
	{
		vec3 up = { 0.0f, 1.0f, 0.0f };
		glm_vec3_cross(up, normal, tangent);
	}
	else
	{
		vec3 right = { 1.0f, 0.0f, 0.0f };
		glm_vec3_cross(right, normal, tangent);
	}

	glm_vec3_normalize(tangent);

	vec3 bitangent;
	glm_vec3_cross(normal, tangent, bitangent);

	// world space
	vec3 temp;
	glm_vec3_scale(tangent, x, dest);
	glm_vec3_scale(bitangent, y, temp);
	glm_vec3_add(dest, temp, dest);
	glm_vec3_scale(normal, z, temp);
	glm_vec3_add(dest, temp, dest);

	glm_vec3_normalize(dest);
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
	av_channel_layout_default(&encoder_ctx->ch_layout, 1);
	avcodec_open2(encoder_ctx, encoder, NULL);
	avcodec_parameters_from_context(stream->codecpar, encoder_ctx);

	avio_open(&format_ctx->pb, filename, AVIO_FLAG_WRITE);
	avformat_write_header(format_ctx, NULL);

	// WAV Content

	// convert float to int and collapse bands
	int16_t* pcm = malloc(MAX_IR_SAMPLES * sizeof(int16_t));

	for (int i = 0; i < MAX_IR_SAMPLES; i++) {

		float x = 0.0f;

		for (int b = 0; b < NUM_FREQ_BANDS; b++) {
			x += sqrtf(listener->ir[b][i]);
		}
		x /= NUM_FREQ_BANDS;

		if (x > 1.0f) x = 1.0f;
		if (x < -1.0f) x = -1.0f;

		pcm[i] = (int16_t)(x * 32767.0f);
	}

	int data_size = MAX_IR_SAMPLES * sizeof(int16_t);
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

void normalise_ir(Listener* listener)
{
	float max = 0.0f;

	// find peak
	for (int b = 0; b < NUM_FREQ_BANDS; b++) {
		for (int i = 0; i < MAX_IR_SAMPLES; i++) {
			float val = fabsf(listener->ir[b][i]);
			if (val > max) max = val;
		}
	}

	if (max == 0.0f) return;

	// normalise
	for (int b = 0; b < NUM_FREQ_BANDS; b++) {
		for (int i = 0; i < MAX_IR_SAMPLES; i++) {
			listener->ir[b][i] /= max;
		}
	}
};


void generate_room(Room* room, float width, float height, float depth)
{
	float hw = width * 0.5f;
	float hh = height * 0.5f;
	float hd = depth * 0.5f;

	float wall_absorption[NUM_FREQ_BANDS] = { 0.01f, 0.01f, 0.02f, 0.02f, 0.02f, 0.02f };
	float floor_absorption[NUM_FREQ_BANDS] = { 0.08f, 0.24f, 0.57f, 0.69f, 0.71f, 0.73f };
	float ceil_absorption[NUM_FREQ_BANDS] = { 0.08f, 0.24f, 0.57f, 0.69f, 0.71f, 0.73f };

	float wall_scatter = 0.9f;
	float floor_scatter = 0.9f;
	float ceil_scatter = 0.9f;

	// left wall 
	room->planes[0] = (Plane){
		.centre = {-hw, 0.0f, 0.0f},
		.normal = { 1.0f, 0.0f, 0.0f},
		.scattering = wall_scatter
	};
	memcpy(room->planes[0].absorption, wall_absorption, sizeof(wall_absorption));

	// eight wALL
	room->planes[1] = (Plane){
		.centre = { hw, 0.0f, 0.0f},
		.normal = {-1.0f, 0.0f, 0.0f},
		.scattering = wall_scatter
	};
	memcpy(room->planes[1].absorption, wall_absorption, sizeof(wall_absorption));

	// back wall
	room->planes[2] = (Plane){
		.centre = {0.0f, 0.0f,-hd},
		.normal = {0.0f, 0.0f, 1.0f},
		.scattering = wall_scatter
	};
	memcpy(room->planes[2].absorption, wall_absorption, sizeof(wall_absorption));

	// front wall
	room->planes[3] = (Plane){
		.centre = {0.0f, 0.0f, hd},
		.normal = {0.0f, 0.0f,-1.0f},
		.scattering = wall_scatter
	};
	memcpy(room->planes[3].absorption, wall_absorption, sizeof(wall_absorption));

	//  floor
	room->planes[4] = (Plane){
		.centre = {0.0f,-hh, 0.0f},
		.normal = {0.0f, 1.0f, 0.0f},
		.scattering = floor_scatter
	};
	memcpy(room->planes[4].absorption, floor_absorption, sizeof(floor_absorption));

	// ceiling
	room->planes[5] = (Plane){
		.centre = {0.0f, hh, 0.0f},
		.normal = {0.0f,-1.0f, 0.0f},
		.scattering = ceil_scatter
	};
	memcpy(room->planes[5].absorption, ceil_absorption, sizeof(ceil_absorption));
}