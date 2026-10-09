#pragma once

#include "raylib.h"
#include "rcamera_blender.h"
#include <raygizmo.h>


#define MAX_SOURCES 4

#define SAMPLE_RATE 44100
#define RAY_MAX 50
#define MAX_BOUNCES 1000
#define EPSILON 0.01f
#define MAX_TIME 2
#define MAX_IR_SAMPLES (MAX_TIME * SAMPLE_RATE)
#define NUM_THREADS 4
#define SPEED_OF_SOUND 343.0f
#define NUM_FREQ_BANDS 6
#define EAR_DISTANCE 0.18f

#define SPEAKER_COLOUR GRAY
#define L_COLOUR BLUE
#define R_COLOUR ORANGE
#define LISTENER_COLOUR PURPLE
#define OUTLINE_COLOUR RED

#define MAX_EVENTS 20

#define NONE_SELECTED_INDEX MAX_SOURCES
#define LISTENER_SELECTED_INDEX MAX_SOURCES+1
#define ROOM_SELECTED_INDEX MAX_SOURCES+2


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
	Vector3 centre;
	float radius;
} Sphere;

typedef struct
{
	Vector3 centre;
	Vector3 normal;
	float absorption[NUM_FREQ_BANDS];
	float scattering;
} Plane;

typedef struct
{
	Vector3 pos;
	Sphere collider; // for visuals only

	float (*raw_l_ir)[MAX_IR_SAMPLES];
	float (*raw_r_ir)[MAX_IR_SAMPLES];

	AudioBuffer* source_audio;
	AudioBuffer* ir_buffer;
	AudioBuffer* convoluted;
	char source_filepath[1024];
	char ir_filepath[1024];
	char convoluted_filepath[1024];

} AudioSource;

typedef struct
{
	Vector3 rotation;
	Sphere head_collider;
	Sphere l_collider;
	Sphere r_collider;
	float (*l_ir)[MAX_IR_SAMPLES];
	float (*r_ir)[MAX_IR_SAMPLES];
}Listener;

typedef struct
{
	Vector3 point;
	Listener* listener;
	Plane planes[6];
}Room;

typedef struct
{
	Vector2 pos;
	Rectangle panel;
	const char* name;

}SelectedInfoPanel;

// Global variables TODO do not rely on these
typedef struct Application
{
	// GUI
	bool addAudioSourceButtonPressed;
	bool renderButtonPressed;
	bool browseSourceFilesButtonPressed;
	bool rendered;
	BlenderCamera camera;
	SelectedInfoPanel selected_info_panel;
	RenderTexture viewportTex;
	Transform selectedTransformGizmo;
	GizmoFlags currentGizmoMode;

	// Logic
	AudioSource speakers[MAX_SOURCES];
	Listener listener;
	int speakers_head;
	int selected;
	int selected_toggle;
	Room room;

	// Final output
	AudioBuffer* mixed_audio;

	char output_filepath[1024];

} Application;

void init_listener(Listener* listener, Vector3 pos);
void init_speaker(Application* app, Vector3 pos, const char* source_audio_filepath);
void init_room(Room* room, Listener* listener, float width, float height, float depth);
void update_listener(Listener* listener); // position
void init_camera(Camera* camera);

