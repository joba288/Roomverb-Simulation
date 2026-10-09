
#define RAYGUI_IMPLEMENTATION
#include "ui.h"


void draw_sphere(Sphere sphere, Color c)
{
	DrawSphere(sphere.centre, sphere.radius, c);
}

void draw_sphere_selected(Sphere sphere, Color c)
{
	DrawSphereWires(sphere.centre, sphere.radius + 0.005f, 10, 10, c);
}

void draw_speaker(Application* app, int i)
{
	draw_sphere(app->speakers[i].collider, SPEAKER_COLOUR);

	if (app->selected == i)
	{
		draw_sphere_selected(app->speakers[i].collider, OUTLINE_COLOUR);
	}
}

void draw_listener(Application* app)
{
	Listener* listener = &app->listener;
	draw_sphere(listener->head_collider, LISTENER_COLOUR);
	draw_sphere(listener->l_collider, R_COLOUR);
	draw_sphere(listener->r_collider, L_COLOUR);

	if (app->selected == LISTENER_SELECTED_INDEX)
	{
		draw_sphere_selected(listener->head_collider, OUTLINE_COLOUR);
		draw_sphere_selected(listener->l_collider, OUTLINE_COLOUR);
		draw_sphere_selected(listener->r_collider, OUTLINE_COLOUR);
	}
}

void draw_selected_info_panel(Application* app)
{
	SelectedInfoPanel selected_info_panel = app->selected_info_panel;
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


	if (app->selected == NONE_SELECTED_INDEX || app->selected == ROOM_SELECTED_INDEX)
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

	if (app->selected == LISTENER_SELECTED_INDEX)
	{
		position = &app->listener.head_collider.centre;
		scale = &app->listener.head_collider.radius;
		rotation = &app->listener.rotation;

	}
	else if (app->selected < MAX_SOURCES)
	{
		position = &app->speakers[app->selected].collider.centre;
		scale = &app->speakers[app->selected].collider.radius;
		source_filepath = app->speakers[app->selected].source_filepath;


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
	if (app->selected < MAX_SOURCES)
	{
		if (!editSourceFilepath)
		{
			if (source_filepath)
			{
				strncpy(sourceFilepathBuffer, source_filepath, sizeof(sourceFilepathBuffer) - 1);
				sourceFilepathBuffer[sizeof(sourceFilepathBuffer) - 1] = '\0';
			}
			else
			{
				sourceFilepathBuffer[0] = '\0';
			}
		}

		GuiLabel(sourceFilepathLabelBounds, "Source:");

		GuiTextBox(sourceFilepathBoxBounds, sourceFilepathBuffer, sizeof(sourceFilepathBuffer), false);

		app->browseSourceFilesButtonPressed = GuiButton(sourceFilepathButtonBounds, "#01#");

	}
	else if (app->selected == LISTENER_SELECTED_INDEX)
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



bool is_sphere_clicked(Application* app, const Sphere* sphere)
{
	if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		return false;


	Vector2 mouse = GetMousePosition();

	Ray ray = GetScreenToWorldRayEx(mouse, app->camera.camera, app->viewportTex.texture.width, app->viewportTex.texture.height);


	RayCollision hit = GetRayCollisionSphere(ray, sphere->centre, sphere->radius);

	return hit.hit;
}

void update_selection(Application* app)
{

	for (int i = 0; i < app->speakers_head; ++i)
	{
		if (is_sphere_clicked(app, &app->speakers[i].collider))
		{

			app->selected = i;
			app->selectedTransformGizmo = GizmoIdentity();
			app->selectedTransformGizmo.translation = app->speakers[app->selected].collider.centre;
		}
	}

	if (is_sphere_clicked(app, &app->listener.head_collider))
	{

		app->selected = LISTENER_SELECTED_INDEX;


		app->selectedTransformGizmo = GizmoIdentity();
		app->selectedTransformGizmo.translation = app->listener.head_collider.centre;

	}



	if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
	{
		app->selected = NONE_SELECTED_INDEX;
	}

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

			DrawCubeWiresV(p->centre,(Vector3) {0.02f, height, depth},color);


		}
		// floor / ceiling
		else if (fabsf(p->normal.y) > 0.9f)
		{
			float width = fabsf(room->planes[1].centre.x - room->planes[0].centre.x);
			float depth = fabsf(room->planes[3].centre.z - room->planes[2].centre.z);

			DrawCubeWiresV(p->centre,(Vector3) {width, 0.02f, depth},color);
		}
		// front / back wall
		else if (fabsf(p->normal.z) > 0.9f)
		{
			float width = fabsf(room->planes[1].centre.x - room->planes[0].centre.x);
			float height = fabsf(room->planes[5].centre.y - room->planes[4].centre.y);

			DrawCubeWiresV(p->centre,(Vector3) {width, height, 0.02f},color);

		}
	}
}

void update_listener(Listener* listener)
{
	listener->l_collider.centre = (Vector3){ listener->head_collider.centre.x - (EAR_DISTANCE) / 2, listener->head_collider.centre.y, listener->head_collider.centre.z };
	listener->r_collider.centre = (Vector3){ listener->head_collider.centre.x + (EAR_DISTANCE) / 2, listener->head_collider.centre.y, listener->head_collider.centre.z };
}

void init_selected_info_panel(Application* app)
{
	app->selected_info_panel.pos = (Vector2){ GetScreenWidth() - GetScreenWidth() * 0.25f, 0.0f };
	app->selected_info_panel.panel = (Rectangle){ app->selected_info_panel.pos.x, app->selected_info_panel.pos.y, GetScreenWidth() * 0.365f, GetScreenHeight() * 0.365f };
	app->selected_info_panel.name = "Selection";
}

