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
#include <raygizmo.h>
#include "rcamera_blender.h"

#include <nfd.h>

#include <stdlib.h>



#define MAX_SOURCES 4

#define SAMPLE_RATE 44100
#define RAY_MAX 5000
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




typedef unsigned int uint;



void init_room(Room* room, float width, float height, float depth);
void draw_room(Room* room);
void update_listener(Listener* listener); // position
void init_camera(Camera* camera);
//
#define SPEAKER_COLOUR GRAY
#define L_COLOUR BLUE
#define R_COLOUR ORANGE
#define LISTENER_COLOUR PURPLE
#define OUTLINE_COLOUR RED

#define MAX_EVENTS 20

#define NONE_SELECTED_INDEX MAX_SOURCES
#define LISTENER_SELECTED_INDEX MAX_SOURCES+1
#define ROOM_SELECTED_INDEX MAX_SOURCES+2

// Program
void on_init();
void on_update();
void on_draw();
void on_draw_gui();
void on_cleanup();
// 
void draw_speaker(int i);
void draw_listener(const Listener* listener);
void draw_sphere(Sphere sphere, Color c);
void draw_sphere_selected(Sphere sphere, Color c);

void draw_selected_info_panel();

void init_listener(Listener* listener, Vector3 pos);
void init_speaker(Vector3 pos, const char* source_audio_filepath);
void init_speakers();
void init_selected_info_panel();

void update_selection();


bool is_sphere_clicked(const Sphere* sphere);

// Impulse Response Generation
void emit_rays(int n, const Room* room, AudioSource* speaker);

Vector3 get_dir_fibonacci_sphere(int i, int n);
int ray_hit_sphere(const Sphere* sphere, const SoundRay* ray, float tmin, float tmax, SoundRayHit* hit);
Vector3 ray_get_pos(float t, const SoundRay* ray);
int ray_hit_plane(const Plane* plane, const SoundRay* ray, float tmin, float tmax, SoundRayHit* hit);
Vector3 reflect_ray(Vector3 dir, Vector3 normal);
Vector3 sample_cosine_hemisphere(Vector3 normal);
void record_impulse(Listener* listener, float distance, float* energy, int ear);
void record_thread_impulse(float(*l_ir)[MAX_IR_SAMPLES], float(*r_ir)[MAX_IR_SAMPLES], float distance, float* energy, int ear);
//
void normalise_ir(AudioSource* speaker);
void impulse_response_to_audio_buffer(AudioSource* speaker);


void import_audio_buffer(const char* filepath, AudioBuffer* dest);
void output_audio_buffer(const char* filpath, AudioBuffer* buffer);

void convolute_buffer(AudioBuffer* audio, AudioBuffer* ir, AudioBuffer* dest);


void free_audio_buffer(AudioBuffer* buffer);


bool addAudioSourceButtonPressed = false;
bool renderButtonPressed = false;
bool browseSourceFilesButtonPressed = false;

bool rendered = false;

BlenderCamera camera;
AudioSource speakers[MAX_SOURCES];
Listener listener;
int speakers_head = 0;
int selected = LISTENER_SELECTED_INDEX;
int selected_toggle = 0;
SelectedInfoPanel selected_info_panel;

RenderTexture viewportTex;

Transform selectedTransformGizmo;

GizmoFlags currentGizmoMode = GIZMO_TRANSLATE;


Room room;



int main()
{

	const int screenWidth = 1080; const int screenHeight = 768;
	const float vpwScale = 1; const float vphScale = 1;
	
	
	InitWindow(screenWidth, screenHeight, "Roomverb Sim");
	viewportTex = LoadRenderTexture(screenWidth * vpwScale, screenHeight * vphScale);





	SetTargetFPS(60);
	//
	on_init();
	while (!WindowShouldClose())
	{

		on_update();





		BeginTextureMode(viewportTex);

			on_draw();

		EndTextureMode();

		BeginDrawing();

		DrawTextureRec(viewportTex.texture, (Rectangle) {0,0,viewportTex.texture.width,-viewportTex.texture.height},(Vector2) {0, 0},WHITE);

			on_draw_gui();

		EndDrawing();
	}
	on_cleanup();

	UnloadRenderTexture(viewportTex);
	CloseWindow();

	return 0;
}



void on_init() 
{
	camera = CreateBlenderCamera();
	//init_camera(&camera.camera);
	init_listener(&listener, (Vector3) {2.0f, 0.0f, 0.0f});
	init_speaker((Vector3) {0.0f, 0.0f, 0.0f}, "resources/sound1.wav");
	init_selected_info_panel();

	init_room(&room, 2.0f, 2.f, 5.0f);
}

void on_update() 
{
	

	
	update_listener(&listener);

	update_selection();
	//UpdateCamera(&camera, CAMERA_FREE);


	BlenderCameraUpdate(&camera);

	if (addAudioSourceButtonPressed)
	{
		nfdchar_t* outPath = NULL;
		nfdresult_t result = NFD_OpenDialog("wav;", NULL, &outPath);

		if (result == NFD_OKAY) {
			const char* ext = strrchr(outPath, '.');

			if (ext && strcmp(ext, ".wav") == 0)
			{
				init_speaker((Vector3) { 0.0f, 0.0f, 0.0f }, outPath);
			}
			else
			{
				printf("Please select a .wav file\n");
			}

			free(outPath);
		}
	}

	if (browseSourceFilesButtonPressed)
	{
		nfdchar_t* outPath = NULL;
		nfdresult_t result = NFD_OpenDialog("wav;", NULL, &outPath);

		if (result == NFD_OKAY) {
			const char* ext = strrchr(outPath, '.');

			if (ext && strcmp(ext, ".wav") == 0)
			{

				strncpy(speakers[selected].source_filepath, outPath, sizeof(speakers[selected].source_filepath) - 1);
				speakers[selected].source_filepath[sizeof(speakers[selected].source_filepath) - 1] = '\0';
			}
			else
			{
				printf("Please select a .wav file\n");
			}

			free(outPath);
		}
	}
	

	if (selected < MAX_SOURCES)
	{
		if (currentGizmoMode == GIZMO_ROTATE) currentGizmoMode = GIZMO_TRANSLATE;

		speakers[selected].pos = selectedTransformGizmo.translation;
		speakers[selected].collider.centre = selectedTransformGizmo.translation;
	}
	else if (selected == LISTENER_SELECTED_INDEX)
	{
		listener.head_collider.centre = selectedTransformGizmo.translation;
		listener.rotation = QuaternionToEuler( selectedTransformGizmo.rotation );
		//listener.head_collider.radius = selectedTransformGizmo.scale;
		
		if (IsKeyPressed(KEY_R))
		{
			currentGizmoMode = GIZMO_ROTATE;
		}
	}


	if (IsKeyPressed(KEY_G))
	{
		currentGizmoMode = GIZMO_TRANSLATE;
	}

	if (IsKeyPressed(KEY_S))
	{
		currentGizmoMode = GIZMO_SCALE;
	}


	if (rendered == false && renderButtonPressed)
	{
		rendered = true;

		// Emit rays from speaker produce raw IR
		emit_rays(RAY_MAX, &room, &speakers[0]);
		printf("Rays Emitted\n");
		// Normalise raw IR
		normalise_ir(&speakers[0]);
		printf("Normalised\n");
		// Convert raw IR to an audio buffer (input / outputtable)
		impulse_response_to_audio_buffer(&speakers[0]);
		printf("Converted\n");


		// Import Source Audio
		import_audio_buffer(speakers[0].source_filepath, speakers[0].source_audio);
		printf("Imported");
		
		// Convolute Source Audio
		convolute_buffer(speakers[0].source_audio, speakers[0].ir_buffer, speakers[0].convoluted);
		printf("convoluted");
		
		// Output Impulse Response and Convoluted Audio
		output_audio_buffer("ir.wav", speakers[0].ir_buffer); // output wav stereo
		output_audio_buffer("convoluted.wav", speakers[0].convoluted);
		printf("outputted");

	}

}

void on_draw() 
{
	ClearBackground(RAYWHITE);
	Vector2 mouse = GetMousePosition();

	DrawCircleV(mouse, 5, RED);
	
	BeginMode3D(camera.camera);
	DrawGrid(20, 1.0f);
	draw_room(&room);
	

	for (int i = 0; i < speakers_head; ++i)
	{
		draw_speaker(i);
	}


	draw_listener(&listener);
	SetMouseScale(
		(float)viewportTex.texture.width / GetScreenWidth(),
		(float)viewportTex.texture.height / GetScreenHeight()
	);

	if (selected != NONE_SELECTED_INDEX) DrawGizmo3D(currentGizmoMode, &selectedTransformGizmo);
	
	SetMouseScale(1, 1);
	EndMode3D();
}

void on_draw_gui() 
{
	draw_selected_info_panel();

	addAudioSourceButtonPressed = GuiButton((Rectangle) { 12.5f, 12.5f, 25.0f, 25.0f }, "+");
	

	Vector2 renderButtonPos = (Vector2){ GetScreenWidth() - GetScreenWidth() * 0.0625f, GetScreenHeight() - GetScreenHeight() * 0.0625f };
	Rectangle renderButtonRect = (Rectangle){ renderButtonPos.x, renderButtonPos.y, GetScreenWidth() * 0.06125f, GetScreenHeight() * 0.06125f };

	renderButtonPressed = GuiButton(renderButtonRect, "Render");
	

}

void on_cleanup()
{
	free(listener.l_ir);
	free(listener.r_ir);

	for (int i = 0; i < speakers_head; ++i)
	{
		free_audio_buffer(speakers[i].source_audio);
		free_audio_buffer(speakers[i].ir_buffer);
		free_audio_buffer(speakers[i].convoluted);
	}

}
// cleanup
void free_audio_buffer(AudioBuffer* buffer)
{
	free(buffer->left);
	free(buffer->right);
	free(buffer);
}


// --
void draw_sphere(Sphere sphere, Color c)
{
	DrawSphere(sphere.centre, sphere.radius, c);
}

void draw_sphere_selected(Sphere sphere, Color c) 
{
	DrawSphereWires(sphere.centre, sphere.radius+0.005f, 10, 10, c);
}

void draw_speaker(int i) 
{
	draw_sphere(speakers[i].collider, SPEAKER_COLOUR);
	
	if (selected == i)
	{
		draw_sphere_selected(speakers[i].collider, OUTLINE_COLOUR);
	}
}

void draw_listener(const Listener* listener) 
{
	draw_sphere(listener->head_collider, LISTENER_COLOUR);
	draw_sphere(listener->l_collider, R_COLOUR);
	draw_sphere(listener->r_collider, L_COLOUR);

	if (selected == LISTENER_SELECTED_INDEX)
	{
		draw_sphere_selected(listener->head_collider, OUTLINE_COLOUR);
		draw_sphere_selected(listener->l_collider, OUTLINE_COLOUR);
		draw_sphere_selected(listener->r_collider, OUTLINE_COLOUR);
	}
}

void draw_selected_info_panel()
{
	GuiPanel(selected_info_panel.panel, selected_info_panel.name);

	static bool editPosX = false;
	static bool editPosY = false;
	static bool editPosZ = false;
	static bool editScale = false;
	static bool editSourceFilepath = false;

	static bool editRotX = false;
	static bool editRotY = false;
	static bool editRotZ = false;

	static char posXBuffer[64] = { 0 };
	static char posYBuffer[64] = { 0 };
	static char posZBuffer[64] = { 0 };
	static char scaleBuffer[64] = { 0 };
	static char sourceFilepathBuffer[64] = { 0 };

	static char rotXBuffer[64] = { 0 };
	static char rotYBuffer[64] = { 0 };
	static char rotZBuffer[64] = { 0 };


	if (selected == NONE_SELECTED_INDEX || selected == ROOM_SELECTED_INDEX)
		return;

	float padding = selected_info_panel.panel.width * 0.025f;

	Rectangle posLabelBounds = {
		selected_info_panel.panel.x + padding,
		selected_info_panel.panel.y + selected_info_panel.panel.height * 0.25f,
		selected_info_panel.panel.width,
		selected_info_panel.panel.height * 0.1f
	};

	Rectangle scaleLabelBounds = {
		selected_info_panel.panel.x + padding,
		selected_info_panel.panel.y + selected_info_panel.panel.height * 0.5f,
		selected_info_panel.panel.width,
		selected_info_panel.panel.height * 0.1f
	};

	Rectangle sourceFilepathLabelBounds = {
	selected_info_panel.panel.x + padding,
	selected_info_panel.panel.y + selected_info_panel.panel.height * 0.75f,
	selected_info_panel.panel.width,
	selected_info_panel.panel.height * 0.1f
	};

	Rectangle rotationLabelBounds = {
	selected_info_panel.panel.x + padding,
	selected_info_panel.panel.y + selected_info_panel.panel.height * 0.75f,
	selected_info_panel.panel.width,
	selected_info_panel.panel.height * 0.1f
	};

	Rectangle posXBounds = {
		selected_info_panel.panel.x + selected_info_panel.panel.width * 0.125f,
		selected_info_panel.panel.y + selected_info_panel.panel.height * 0.25f,
		selected_info_panel.panel.width * 0.125f,
		selected_info_panel.panel.height * 0.1f
	};

	Rectangle posYBounds = {
		selected_info_panel.panel.x + selected_info_panel.panel.width * 0.325f,
		selected_info_panel.panel.y + selected_info_panel.panel.height * 0.25f,
		selected_info_panel.panel.width * 0.125f,
		selected_info_panel.panel.height * 0.1f
	};

	Rectangle posZBounds = {
		selected_info_panel.panel.x + selected_info_panel.panel.width * 0.525f,
		selected_info_panel.panel.y + selected_info_panel.panel.height * 0.25f,
		selected_info_panel.panel.width * 0.125f,
		selected_info_panel.panel.height * 0.1f
	};

	Rectangle scaleBoxBounds = {
		selected_info_panel.panel.x + selected_info_panel.panel.width * 0.125f,
		selected_info_panel.panel.y + selected_info_panel.panel.height * 0.5f,
		selected_info_panel.panel.width * 0.25f,
		selected_info_panel.panel.height * 0.1f
	};

	Rectangle sourceFilepathBoxBounds = {
	selected_info_panel.panel.x + selected_info_panel.panel.width * 0.125f,
	selected_info_panel.panel.y + selected_info_panel.panel.height * 0.75f,
	selected_info_panel.panel.width * 0.4f,
	selected_info_panel.panel.height * 0.1f
	};

	Rectangle sourceFilepathButtonBounds = {
	selected_info_panel.panel.x + selected_info_panel.panel.width * 0.525f,
	selected_info_panel.panel.y + selected_info_panel.panel.height * 0.75f,
	selected_info_panel.panel.width * 0.1f,
	selected_info_panel.panel.height * 0.1f
	};

	Rectangle rotXBounds = {
	selected_info_panel.panel.x + selected_info_panel.panel.width * 0.125f,
	selected_info_panel.panel.y + selected_info_panel.panel.height * 0.75f,
	selected_info_panel.panel.width * 0.125f,
	selected_info_panel.panel.height * 0.1f
	};

	Rectangle rotYBounds = {
		selected_info_panel.panel.x + selected_info_panel.panel.width * 0.325f,
		selected_info_panel.panel.y + selected_info_panel.panel.height * 0.75f,
		selected_info_panel.panel.width * 0.125f,
		selected_info_panel.panel.height * 0.1f
	};

	Rectangle rotZBounds = {
		selected_info_panel.panel.x + selected_info_panel.panel.width * 0.525f,
		selected_info_panel.panel.y + selected_info_panel.panel.height * 0.75f,
		selected_info_panel.panel.width * 0.125f,
		selected_info_panel.panel.height * 0.1f
	};

	

	Vector3* position = NULL;
	Vector3* rotation = NULL;
	float* scale = NULL;
	char* source_filepath = NULL;

	if (selected == LISTENER_SELECTED_INDEX)
	{
		position = &listener.head_collider.centre;
		scale = &listener.head_collider.radius;
		rotation = &listener.rotation;

	}
	else if (selected < MAX_SOURCES)
	{
		position = &speakers[selected].collider.centre;
		scale = &speakers[selected].collider.radius;
		source_filepath = speakers[selected].source_filepath;

		
	}

	if (!position || !scale)
		return;

	// Keep buffers synced while not editing
	if (!editPosX)
		snprintf(posXBuffer, sizeof(posXBuffer), "%.2f", position->x);

	if (!editPosY)
		snprintf(posYBuffer, sizeof(posYBuffer), "%.2f", position->y);

	if (!editPosZ)
		snprintf(posZBuffer, sizeof(posZBuffer), "%.2f", position->z);

	if (!editScale)
		snprintf(scaleBuffer, sizeof(scaleBuffer), "%.2f", *scale);

	GuiLabel(posLabelBounds, "Pos:");

	if (GuiTextBox(posXBounds, posXBuffer, sizeof(posXBuffer), editPosX))
	{
		editPosX = !editPosX;

		if (!editPosX)
			position->x = (float)atof(posXBuffer);
	}

	if (GuiTextBox(posYBounds, posYBuffer, sizeof(posYBuffer), editPosY))
	{
		editPosY = !editPosY;

		if (!editPosY)
			position->y = (float)atof(posYBuffer);
	}

	if (GuiTextBox(posZBounds, posZBuffer, sizeof(posZBuffer), editPosZ))
	{
		editPosZ = !editPosZ;

		if (!editPosZ)
			position->z = (float)atof(posZBuffer);
	}

	GuiLabel(scaleLabelBounds, "Scale:");

	if (GuiTextBox(scaleBoxBounds, scaleBuffer, sizeof(scaleBuffer), editScale))
	{
		editScale = !editScale;

		if (!editScale)
			*scale = (float)atof(scaleBuffer);
	}

	// Selection specific 
	if (selected < MAX_SOURCES)
	{
		if (!editSourceFilepath)
		{
			if (source_filepath)
			{
				strncpy(sourceFilepathBuffer, source_filepath,sizeof(sourceFilepathBuffer) - 1);
				sourceFilepathBuffer[sizeof(sourceFilepathBuffer) - 1] = '\0';
			}
			else
			{
				sourceFilepathBuffer[0] = '\0';
			}
		}

		GuiLabel(sourceFilepathLabelBounds, "Source:");

		GuiTextBox(sourceFilepathBoxBounds, sourceFilepathBuffer, sizeof(sourceFilepathBuffer), false);

		browseSourceFilesButtonPressed = GuiButton(sourceFilepathButtonBounds, "#01#");

	}
	else if (selected == LISTENER_SELECTED_INDEX)
	{
		if (!rotation)
			return;

		// Keep buffers synced while not editing
		if (!editRotX)
			snprintf(rotXBuffer, sizeof(rotXBuffer), "%.2f", rotation->x);

		if (!editRotY)
			snprintf(rotYBuffer, sizeof(rotYBuffer), "%.2f", rotation->y);

		if (!editRotZ)
			snprintf(rotZBuffer, sizeof(rotZBuffer), "%.2f", rotation->z);

		GuiLabel(rotationLabelBounds, "Rot: ");

		if (GuiTextBox(rotXBounds, rotXBuffer, sizeof(rotXBuffer), editRotX))
		{
			editRotX = !editRotX;

			if (!editRotX)
				rotation->x = (float)atof(rotXBuffer);
		}

		if (GuiTextBox(rotYBounds, rotYBuffer, sizeof(rotYBuffer), editRotY))
		{
			editRotY = !editRotY;

			if (!editRotY)
				rotation->y = (float)atof(rotYBuffer);
		}

		if (GuiTextBox(rotZBounds, rotZBuffer, sizeof(rotZBuffer), editRotZ))
		{
			editRotZ = !editRotZ;

			if (!editRotZ)
				rotation->z = (float)atof(rotZBuffer);
		}

	}
}

//
bool is_sphere_clicked(const Sphere* sphere)
{
	if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		return false;


	Vector2 mouse = GetMousePosition();

	Ray ray = GetScreenToWorldRayEx(mouse, camera.camera, viewportTex.texture.width, viewportTex.texture.height);
	

	RayCollision hit = GetRayCollisionSphere(ray, sphere->centre, sphere->radius);

	return hit.hit;
}

void update_selection()
{

	for (int i = 0; i < speakers_head; ++i)
	{
		if (is_sphere_clicked(&speakers[i].collider))
		{

			selected = i;



			selectedTransformGizmo = GizmoIdentity();
			selectedTransformGizmo.translation = speakers[selected].collider.centre;
		}
	}

	if (is_sphere_clicked(&listener.head_collider))
	{

		selected = LISTENER_SELECTED_INDEX;


		selectedTransformGizmo = GizmoIdentity();
		selectedTransformGizmo.translation = listener.head_collider.centre;

	}
	


	if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
	{
		selected = NONE_SELECTED_INDEX;
	}




	 



}

void init_speaker(Vector3 pos, const char* source_audio_filepath)
{
	if (speakers_head >= MAX_SOURCES)
	{
		printf("Sources list is full\n");
		return;
	}
	selected = speakers_head;
	speakers[speakers_head].pos = pos;
	speakers[speakers_head].collider = (Sphere){ .centre = pos, .radius = 0.1f };
	
	strncpy(speakers[speakers_head].source_filepath, source_audio_filepath, sizeof(speakers[speakers_head].source_filepath) - 1);
	speakers[speakers_head].source_filepath[sizeof(speakers[speakers_head].source_filepath) - 1] = '\0';

	speakers[speakers_head].source_audio = malloc(sizeof(AudioBuffer));
	speakers[speakers_head].ir_buffer = malloc(sizeof(AudioBuffer));
	speakers[speakers_head].convoluted = malloc(sizeof(AudioBuffer));

	speakers[speakers_head].raw_l_ir = calloc(NUM_FREQ_BANDS, sizeof * speakers[speakers_head].raw_l_ir);
	speakers[speakers_head].raw_r_ir = calloc(NUM_FREQ_BANDS, sizeof * speakers[speakers_head].raw_r_ir);

	speakers_head++;
}

void init_selected_info_panel()
{
	selected_info_panel.pos = (Vector2){GetScreenWidth() - GetScreenWidth()*0.25f, 0.0f};
	selected_info_panel.panel = (Rectangle){ selected_info_panel.pos.x, selected_info_panel.pos.y, GetScreenWidth()*0.365f, GetScreenHeight()*0.365f };
	selected_info_panel.name = "Selection";
}


// Listener

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

	camera->position = (Vector3){ 10.0f, 10.0f, 10.0f }; // Camera position
	camera->target = (Vector3){ 0.0f, 0.0f, 0.0f };      // Camera looking at point
	camera->up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
	camera->fovy = 45.0f;                                // Camera field-of-view Y
	camera->projection = CAMERA_PERSPECTIVE;             // Camera projection type
}
void init_room(Room* room, float width, float height, float depth)
{
	room->listener = &listener;
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

			DrawCubeWiresV(
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

			DrawCubeWiresV(
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
			
			DrawCubeWiresV(
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
	listener->l_collider.centre = (Vector3){ listener->head_collider.centre.x - (EAR_DISTANCE) / 2, listener->head_collider.centre.y, listener->head_collider.centre.z };
	listener->r_collider.centre = (Vector3){ listener->head_collider.centre.x + (EAR_DISTANCE) / 2, listener->head_collider.centre.y, listener->head_collider.centre.z };

}

// Raycasting Generation

void emit_rays(int n, const Room* room, AudioSource* speaker)
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

void impulse_response_to_audio_buffer(AudioSource* speaker)
{

	speaker->ir_buffer->left = malloc(MAX_IR_SAMPLES * sizeof(float));
	speaker->ir_buffer->right = malloc(MAX_IR_SAMPLES * sizeof(float));
	speaker->ir_buffer->sampleCount = MAX_IR_SAMPLES;
	speaker->ir_buffer->sampleRate = SAMPLE_RATE;

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

		speaker->ir_buffer->left[i] = l;
		speaker->ir_buffer->right[i] = r;
	}

}


void import_audio_buffer(const char* filepath, AudioBuffer* dest)
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
}



void convolute_stereo(AudioBuffer* audio, AudioBuffer* ir, AudioBuffer* dest)
{
	
	

}