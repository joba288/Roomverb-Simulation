#pragma once
#include "roomverb.h"

typedef struct
{
	Vector3 origin;
	Vector3 dir;
	float energy[NUM_FREQ_BANDS];
	float distance;
}SoundRay;

// Information for a ray collision
typedef struct
{
	Vector3 point;
	Vector3 normal;
	float t;
}SoundRayHit;


void emit_rays(int n, const Room* room, AudioSource* speaker);

// Disperses rays in a sphere shape
Vector3 get_dir_fibonacci_sphere(int i, int n);
// Checks if ray intersects a sphere
int ray_hit_sphere(const Sphere* sphere, const SoundRay* ray, float tmin, float tmax, SoundRayHit* hit);
// Checks if ray intersects a sphere
int ray_hit_plane(const Plane* plane, const SoundRay* ray, float tmin, float tmax, SoundRayHit* hit);
// Returns position of ray at point in time
Vector3 ray_get_pos(float t, const SoundRay* ray);

Vector3 reflect_ray(Vector3 dir, Vector3 normal);
Vector3 sample_cosine_hemisphere(Vector3 normal);

void record_impulse(Listener* listener, float distance, float* energy, int ear);
// For multithreaded implementation
void record_thread_impulse(float(*l_ir)[MAX_IR_SAMPLES], float(*r_ir)[MAX_IR_SAMPLES], float distance, float* energy, int ear);
void normalise_ir(AudioSource* speaker);

void clear_raw_ir(AudioSource* source);