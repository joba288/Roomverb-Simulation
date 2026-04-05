// joba288

#include "cglm/cglm.h"
#include <stdio.h>

#define RAY_MAX 10000

typedef struct
{
	vec3 origin;
	vec3 dir;
}Ray;

typedef struct
{
	vec3 centre;
	float radius;
}Sphere;



void ray_get_pos(float t, const Ray* ray, vec3 dest);
float ray_hit_sphere(const Sphere* sphere, const Ray* ray);
void emit_rays(int n, vec3 pos, const Sphere* sphere);
void get_dir_fibonacci_sphere(int i, int n, vec3 dest);


int main()
{
	vec3 emit_pos;
	glm_vec3_zero(emit_pos);
	

	Sphere sphere =
	{
		.centre = {10.0f, 2.0f, 0.0f},
		.radius = 2.0f
	};


	emit_rays(RAY_MAX, emit_pos, &sphere);
	return 0;
}



void ray_get_pos(float t, const Ray* ray, vec3 dest)
{
	// origin + t * dir
	glm_vec3_scale(ray->dir, t, dest);
	glm_vec3_add(dest, ray->origin, dest);
}

float ray_hit_sphere(const Sphere* sphere, const Ray* ray)
{
	vec3 oc;
	glm_vec3_sub(sphere->centre, ray->origin, oc);

	float a = glm_dot(ray->dir, ray->dir);
	float b = -2.0f * glm_dot(ray->dir, oc);
	float c = glm_dot(oc, oc) - sphere->radius * sphere->radius;
	float discriminant = b * b - 4 * a * c;
	return (discriminant>=0); // two roots
}

void emit_rays(int n, vec3 pos, const Sphere* sphere)
{
	for (int i = 0; i < n; ++i)
	{
		Ray ray;
		glm_vec3_copy(pos, ray.origin);
		get_dir_fibonacci_sphere(i, n, ray.dir);
		if (ray_hit_sphere(sphere, &ray))
		{
			printf("Collision: %d\n", i);
		}
	}
}

void get_dir_fibonacci_sphere(int i, int n, vec3 dest)
{
	const float phi = M_PI * (3.0f - sqrtf(5.0f)); // golden angle

	dest[1] = 1.0f - (2.0f * (float)i + 1.0f) / (float)n;  // y in [-1,1]
	float r = sqrtf(1.0f - dest[1] * dest[1]);

	float theta = phi * i;

	dest[0] = cosf(theta) * r;
	dest[2] = sinf(theta) * r;

}

/*




