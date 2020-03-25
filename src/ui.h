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
    MenuItem items[MAX_MENU_ITEMS];
    int highlighted_item;
    int hover_item;
    int item_count;
    float font_size;
} Menu;

void menu_set_title(Menu *d, char *title);
int menu_add(Menu *menu, char *item);
int menu_add_option(Menu *menu, char *item);
void menu_set_option(Menu *d, int i, int value);
int menu_get_option(Menu *d, int i);
int menu_add_line_edit(Menu *menu, char *label);
char *menu_get_line_edit(Menu *menu, int i);
void menu_set_highlighted_item(Menu *menu, int i);

int menu_handle_mouse_release(Menu *menu, int x, int y);
void menu_handle_mouse(Menu *menu, int x, int y);
int menu_handle_key_press(Menu *d, int mods, int keysym);
void menu_handle_joystick_axis(Menu *d, int axis, float value);
int menu_handle_joystick_button(Menu *d, int button, int state);

void menu_render(Menu *menu, Attrib *text_attrib, Attrib *line_attrib,
                 int view_width, int view_height);
