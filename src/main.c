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

bool addAudioSourceButtonPressed = false;
bool renderButtonPressed = false;
bool browseSourceFilesButtonPressed = false;

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
