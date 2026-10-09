// joba288

#include "cglm/cglm.h"
#include <stdio.h>
#include <time.h>

#include <nfd.h>

#include <stdlib.h>

#include "raytracer.h"
#include "ui.h"
#include "audio.h"
#include "roomverb.h"

#include "rcamera_blender.h"

void on_init(Application* app);
void on_update(Application* app);
void on_draw(Application* app);
void on_draw_gui(Application* app);
void on_cleanup(Application* app);


void init_application(Application* app);


int main()
{

	Application app;
	init_application(&app);
	const int screenWidth = 1080; const int screenHeight = 768;
	const float vpwScale = 1; const float vphScale = 1;
	
	InitWindow(screenWidth, screenHeight, "Roomverb Sim");
	app.viewportTex = LoadRenderTexture(screenWidth * vpwScale, screenHeight * vphScale);

	SetTargetFPS(60);
	//
	on_init(&app);
	while (!WindowShouldClose())
	{

		on_update(&app);

		BeginTextureMode(app.viewportTex);

			on_draw(&app);

		EndTextureMode();

		BeginDrawing();

		DrawTextureRec(app.viewportTex.texture, (Rectangle) {0,0, app.viewportTex.texture.width,-app.viewportTex.texture.height},(Vector2) {0, 0},WHITE);

			on_draw_gui(&app);

		EndDrawing();
	}
	on_cleanup(&app);

	UnloadRenderTexture(app.viewportTex);
	CloseWindow();

	return 0;
}

void on_init(Application* app) 
{
	
	app->camera = CreateBlenderCamera();
	init_listener(&app->listener, (Vector3) {2.0f, 0.0f, 0.0f});
	init_speaker(app, (Vector3) {0.0f, 0.0f, 0.0f}, "resources/sound1.wav");
	init_selected_info_panel(app);
	init_room(&app->room, &app->listener, 2.0f, 2.f, 5.0f);
}

void on_update(Application* app)
{
	update_listener(&app->listener);
	update_selection(app);
	BlenderCameraUpdate(&app->camera);



	// Check GUI Input

	if (app->addAudioSourceButtonPressed)
	{
		app->addAudioSourceButtonPressed = false;

		nfdchar_t* outPath = NULL;
		nfdresult_t result = NFD_OpenDialog("wav", NULL, &outPath);

		if (result == NFD_OKAY && outPath != NULL)
		{
			printf("Selected file: %s\n", outPath);

			init_speaker(app,(Vector3) {0.0f, 0.0f, 0.0f},outPath);

			free(outPath);
		}
		else if (result == NFD_ERROR)
		{
			printf("NFD error: %s\n", NFD_GetError());
		}
	}

	if (app->browseSourceFilesButtonPressed)
	{
		app->browseSourceFilesButtonPressed = false;

		nfdchar_t* outPath = NULL;
		nfdresult_t result = NFD_OpenDialog("wav", NULL, &outPath);

		if (result == NFD_OKAY && outPath != NULL)
		{
			printf("Selected file: %s\n", outPath);

			strncpy(app->speakers[app->selected].source_filepath,outPath,sizeof(app->speakers[app->selected].source_filepath) - 1);

			app->speakers[app->selected].source_filepath[sizeof(app->speakers[app->selected].source_filepath) - 1] = '\0';

			free(outPath);
		}
		else if (result == NFD_ERROR)
		{
			printf("NFD error: %s\n", NFD_GetError());
		}
	}
	
	// Draw Gizmos on selected
	if (app->selected < MAX_SOURCES)
	{
		if (app->currentGizmoMode == GIZMO_ROTATE) app->currentGizmoMode = GIZMO_TRANSLATE;

		app->speakers[app->selected].pos = app->selectedTransformGizmo.translation;
		app->speakers[app->selected].collider.centre = app->selectedTransformGizmo.translation;
	}
	else if (app->selected == LISTENER_SELECTED_INDEX)
	{
		app->listener.head_collider.centre = app->selectedTransformGizmo.translation;
		app->listener.rotation = QuaternionToEuler(app->selectedTransformGizmo.rotation );
		
		// Can only rotate a listener
		if (IsKeyPressed(KEY_R))
		{
			app->currentGizmoMode = GIZMO_ROTATE;
		}
	}

	if (IsKeyPressed(KEY_G))
	{
		app->currentGizmoMode = GIZMO_TRANSLATE;
	}

	if (IsKeyPressed(KEY_S))
	{
		app->currentGizmoMode = GIZMO_SCALE;
	}


	if (app->rendered == false && app->renderButtonPressed)
	{
		app->rendered = true;

		for (int i = 0; i < app->speakers_head; ++i)
		{
			AudioSource* source = &app->speakers[i];

			printf("[%d] Starting raycasting\n", i);
			fflush(stdout);

			emit_rays(RAY_MAX, &app->room, source);

			printf("[%d] Raycasting complete\n", i);
			fflush(stdout);

			normalise_ir(source);

			printf("[%d] Normalisation complete\n", i);
			fflush(stdout);

			impulse_response_to_audio_buffer(source);

			printf("[%d] IR conversion complete\n", i);
			fflush(stdout);

			if (!import_audio_buffer(source->source_filepath,source->source_audio))
			{
				fprintf(stderr,"Failed to import: %s\n", source->source_filepath);

				continue;
			}

			printf("[%d] Import complete: %d samples\n", i, source->source_audio->sampleCount);
			fflush(stdout);

			convolute_buffer(source->source_audio,source->ir_buffer,source->convoluted);

			printf("[%d] Convolution complete\n", i);
			fflush(stdout);
		}

		printf("Starting mixing\n");
		fflush(stdout);

		AudioBuffer* buffers[MAX_SOURCES];

		for (int i = 0; i < app->speakers_head; i++)
			buffers[i] = app->speakers[i].convoluted;

		mix_audio_buffers(
			buffers,
			app->speakers_head,
			app->mixed_audio
		);
		output_audio_buffer("mixed_output.wav", app->mixed_audio);

		printf("Mixing complete\n");
		fflush(stdout);
	}

	app->rendered = false;

}

void on_draw(Application* app)
{
	ClearBackground(RAYWHITE);
	Vector2 mouse = GetMousePosition();

	DrawCircleV(mouse, 5, RED);
	
	BeginMode3D(app->camera.camera);
	DrawGrid(20, 1.0f);
	draw_room(&app->room);
	

	for (int i = 0; i < app->speakers_head; ++i)
	{
		draw_speaker(app, i);
	}

	draw_listener(app);
	SetMouseScale((float)app->viewportTex.texture.width / GetScreenWidth(),(float)app->viewportTex.texture.height / GetScreenHeight());

	if (app->selected != NONE_SELECTED_INDEX) DrawGizmo3D(app->currentGizmoMode, &app->selectedTransformGizmo);
	
	SetMouseScale(1, 1);
	EndMode3D();
}

void on_draw_gui(Application* app)
{
	draw_selected_info_panel(app);

	app->addAudioSourceButtonPressed = GuiButton((Rectangle) { 12.5f, 12.5f, 25.0f, 25.0f }, "+");
	

	Vector2 renderButtonPos = (Vector2){ GetScreenWidth() - GetScreenWidth() * 0.0625f, GetScreenHeight() - GetScreenHeight() * 0.0625f };
	Rectangle renderButtonRect = (Rectangle){ renderButtonPos.x, renderButtonPos.y, GetScreenWidth() * 0.06125f, GetScreenHeight() * 0.06125f };

	app->renderButtonPressed = GuiButton(renderButtonRect, "Render");
	

}

void on_cleanup(Application* app)
{
	free(app->listener.l_ir);
	free(app->listener.r_ir);

	app->listener.l_ir = NULL;
	app->listener.r_ir = NULL;

	for (int i = 0; i < app->speakers_head; ++i)
	{
		AudioSource* source = &app->speakers[i];

		free_audio_buffer(source->source_audio);
		free_audio_buffer(source->ir_buffer);
		free_audio_buffer(source->convoluted);

		source->source_audio = NULL;
		source->ir_buffer = NULL;
		source->convoluted = NULL;

		free(source->raw_l_ir);
		free(source->raw_r_ir);

		source->raw_l_ir = NULL;
		source->raw_r_ir = NULL;
	}

	free_audio_buffer(app->mixed_audio);
	app->mixed_audio = NULL;
}


void init_application(Application* app)
{
	*app = (Application){ 0 };
	app->addAudioSourceButtonPressed = false;
	app->renderButtonPressed = false;
	app->browseSourceFilesButtonPressed = false;
	app->rendered = false;
	app->currentGizmoMode = GIZMO_TRANSLATE;

	app->mixed_audio = malloc(sizeof(AudioBuffer));

	if (app->mixed_audio)
	{
		*app->mixed_audio = (AudioBuffer){ 0 };
	}
}




