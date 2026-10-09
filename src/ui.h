#pragma once
#include "roomverb.h"
#include "raygui.h"

void draw_speaker(Application* app, int i);
void draw_listener(Application* app);
void draw_sphere(Sphere sphere, Color c);
void draw_sphere_selected(Sphere sphere, Color c);
void draw_selected_info_panel(Application* app);
void draw_room(Room* room);


void update_selection(Application* app);
bool is_sphere_clicked(Application* app, const Sphere* sphere);
void init_selected_info_panel(Application* app);