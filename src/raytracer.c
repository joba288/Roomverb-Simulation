#include "raytracer.h"

#include <stdlib.h>
#include <math.h>
#include <omp.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void emit_rays(int n, const Room* room, AudioSource* speaker)
{
	// Clear any previous impulse response
	clear_raw_ir(speaker);

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
	#pragma omp parallel for reduction(+:count, listener_hit_count, absorbed_count)
	for (i = 0; i < n; ++i)
	{
		int thread_id = omp_get_thread_num();
		float (*l_ir)[MAX_IR_SAMPLES] = thread_l_ir[thread_id];
		float (*r_ir)[MAX_IR_SAMPLES] = thread_r_ir[thread_id];

		SoundRay ray;
		SoundRayHit hit;
		ray.origin = speaker->pos;

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
			if (ray_hit_sphere(&room->listener->l_collider, &ray, EPSILON, 10000, &temp))
			{
				if (temp.t < closest_t)
				{
					closest_t = temp.t;
					closest_hit = temp;
					hit_type = 1;
				}
			}

			// right
			if (ray_hit_sphere(&room->listener->r_collider, &ray, EPSILON, 10000, &temp))
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
				listener_hit_count++;

				// record ir
				float total_distance = ray.distance + closest_hit.t;
				record_thread_impulse(l_ir, r_ir, total_distance, ray.energy, hit_type);
				break;

			}
			else if (hit_type == 3)
			{
				// plane hit
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
				speaker->raw_l_ir[b][i] += thread_l_ir[t][b][i];
				speaker->raw_r_ir[b][i] += thread_r_ir[t][b][i];
			}
		}
	}



	free(thread_l_ir);
	free(thread_r_ir);

	printf("Raycasting done\n");
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
Vector3 ray_get_pos(float t, const SoundRay* ray)
{
	// origin + t * dir
	return Vector3Add(ray->origin, Vector3Scale(ray->dir, t));
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


//
void normalise_ir(AudioSource* speaker)
{
	float max = 0.0f;

	// find peak
	for (int b = 0; b < NUM_FREQ_BANDS; b++) {
		for (int i = 0; i < MAX_IR_SAMPLES; i++) {
			float l = fabsf(speaker->raw_l_ir[b][i]);
			float r = fabsf(speaker->raw_r_ir[b][i]);

			if (l > max) max = l;
			if (r > max) max = r;
		}
	}

	if (max == 0.0f) return;

	// normalise
	for (int b = 0; b < NUM_FREQ_BANDS; b++) {
		for (int i = 0; i < MAX_IR_SAMPLES; i++) {
			speaker->raw_l_ir[b][i] /= max;
			speaker->raw_r_ir[b][i] /= max;
		}
	}
}




void clear_raw_ir(AudioSource* source)
{
	printf(
		"CLEAR: L=%p R=%p\n",
		(void*)source->raw_l_ir,
		(void*)source->raw_r_ir
	);

	memset(
		source->raw_l_ir,
		0,
		NUM_FREQ_BANDS * sizeof(*source->raw_l_ir)
	);

	memset(
		source->raw_r_ir,
		0,
		NUM_FREQ_BANDS * sizeof(*source->raw_r_ir)
	);
}