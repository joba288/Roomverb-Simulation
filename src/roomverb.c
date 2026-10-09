#include "roomverb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void init_listener(Listener* listener, Vector3 pos)
{
	Sphere head =
	{
		.centre = pos,
		.radius = 0.15f
	};

	Sphere left_ear = {
		.centre = {0.0f, 0.0f},
		.radius = 0.1f
	};

	Sphere right_ear = {
		.centre = {0.0f, 0.0f},
		.radius = 0.1f
	};

	listener->head_collider = head;
	listener->l_collider = left_ear;
	listener->r_collider = right_ear;

	listener->l_ir = calloc(NUM_FREQ_BANDS, sizeof * listener->l_ir);
	listener->r_ir = calloc(NUM_FREQ_BANDS, sizeof * listener->r_ir);

}


void init_camera(Camera* camera)
{
	camera->position = (Vector3){ 10.0f, 10.0f, 10.0f }; 
	camera->target = (Vector3){ 0.0f, 0.0f, 0.0f };      
	camera->up = (Vector3){ 0.0f, 1.0f, 0.0f };          
	camera->fovy = 45.0f;                                
	camera->projection = CAMERA_PERSPECTIVE;             
}
void init_room(Room* room, Listener* listener, float width, float height, float depth)
{
	room->listener = listener;
	float hw = width * 0.5f;
	float hd = depth * 0.5f;

	// floor at y = 0
	// ceiling at y = height
	float floor_y = 0.0f;
	float ceil_y = height;

	// room vertical center
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


void init_speaker(
	Application* app,
	Vector3 pos,
	const char* source_audio_filepath)
{
	if (app->speakers_head >= MAX_SOURCES)
	{
		printf("Sources list is full\n");
		return;
	}

	int index = app->speakers_head;

	AudioSource* source = &app->speakers[index];

	// VERY IMPORTANT:
	// Start with everything zeroed.
	*source = (AudioSource){ 0 };

	source->pos = pos;

	source->collider = (Sphere){
		.centre = pos,
		.radius = 0.1f
	};

	strncpy(
		source->source_filepath,
		source_audio_filepath,
		sizeof(source->source_filepath) - 1
	);

	source->source_filepath[
		sizeof(source->source_filepath) - 1
	] = '\0';


	// Audio buffers
	source->source_audio = calloc(1, sizeof(AudioBuffer));
	source->ir_buffer = calloc(1, sizeof(AudioBuffer));
	source->convoluted = calloc(1, sizeof(AudioBuffer));


	// Raw impulse responses
	source->raw_l_ir =
		calloc(NUM_FREQ_BANDS, sizeof(*source->raw_l_ir));

	source->raw_r_ir =
		calloc(NUM_FREQ_BANDS, sizeof(*source->raw_r_ir));


	// Check EVERYTHING
	if (!source->source_audio ||
		!source->ir_buffer ||
		!source->convoluted ||
		!source->raw_l_ir ||
		!source->raw_r_ir)
	{
		fprintf(stderr, "Failed to allocate AudioSource\n");

		free(source->source_audio);
		free(source->ir_buffer);
		free(source->convoluted);
		free(source->raw_l_ir);
		free(source->raw_r_ir);

		*source = (AudioSource){ 0 };

		return;
	}


	printf(
		"Created source %d: L=%p R=%p\n",
		index,
		(void*)source->raw_l_ir,
		(void*)source->raw_r_ir
	);


	app->selected = index;
	app->speakers_head++;
}