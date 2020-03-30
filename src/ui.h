#pragma once

#include "config.h"
#include "pw.h"

#define MENU_CANCELLED -1
#define MENU_REMAINS_OPEN 0

// Type of menu item
#define MENU_BUTTON 0
#define MENU_OPTION 1
#define MENU_LINE_EDIT 2

#define MAX_MENU_ITEMS 16
#define MAX_TEXT_LENGTH 256

#define MOUSE_CURSOR_SIZE 16

typedef struct {
    char name[MAX_TEXT_LENGTH];
    int type;
    int data;
    char text[MAX_TEXT_LENGTH];
    size_t text_cursor;
    float x;
    float y;
    float w;
    float h;
} MenuItem;

typedef struct {
    char title[MAX_TEXT_LENGTH];
    MenuItem *items;
    int item_count;
    int allocated_item_count;
    int highlighted_item;
    int hover_item;
    float font_size;
    int first_item_shown;
    int max_menu_row_count;
} Menu;

void menu_clear_items(Menu *menu);
void menu_set_title(Menu *menu, char *title);
void menu_sort(Menu *menu);
int menu_add(Menu *menu, char *item);
int menu_add_option(Menu *menu, char *item);
void menu_set_option(Menu *menu, int i, int value);
int menu_get_option(Menu *menu, int i);
int menu_add_line_edit(Menu *menu, char *label);
char *menu_get_line_edit(Menu *menu, int i);
char *menu_get_name(Menu *menu, int i);
void menu_set_highlighted_item(Menu *menu, int i);

int menu_handle_mouse_release(Menu *menu, int x, int y, int b);
void menu_handle_mouse(Menu *menu, int x, int y);
int menu_handle_key_press(Menu *menu, int mods, int keysym);
void menu_handle_joystick_axis(Menu *menu, int axis, float value);
int menu_handle_joystick_button(Menu *menu, int button, int state);

void menu_render(Menu *menu, Attrib *text_attrib, Attrib *line_attrib,
                 int view_width, int view_height);
