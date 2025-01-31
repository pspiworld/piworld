#pragma once

#include "config.h"
#include "util.h"

#define MENU_CANCELLED -1
#define MENU_REMAINS_OPEN 0

// Type of menu item
#define MENU_BUTTON 0
#define MENU_OPTION 1
#define MENU_LINE_EDIT 2
#define MENU_HBOX_START 3
#define MENU_HBOX_END 4

#define MOUSE_CURSOR_SIZE 16

typedef struct MenuItem {
    char name[MAX_TEXT_LENGTH];
    int type;
    int data;
    char text[MAX_TEXT_LENGTH];
    size_t text_cursor;
    struct MenuItem* parent;
    float x;
    float y;
    float w;
    float h;
} MenuItem;

typedef struct {
    char title[MAX_TEXT_LENGTH];
    int id;
    MenuItem *items;
    int item_count;
    int allocated_item_count;
    int highlighted_item;
    int hover_item;
    float font_size;
    int first_item_shown;
    int max_menu_row_count;
    MenuItem *open_box;
    float title_bg_color[4];
    float bg_color[4];
} Menu;

Menu *menu_create(void);
void menu_destroy(Menu *menu);
void menu_clear_items(Menu *menu);
void menu_set_title(Menu *menu, char *title);
void menu_set_title_bg(Menu *menu, float r, float g, float b, float a);
void menu_set_bg(Menu *menu, float r, float g, float b, float a);
void menu_sort(Menu *menu);
int menu_add(Menu *menu, char *item);
int menu_add_option(Menu *menu, char *item);
void menu_set_option(Menu *menu, int i, int value);
int menu_get_option(Menu *menu, int i);
void menu_set_text(Menu *menu, int i, char *text);
int menu_add_line_edit(Menu *menu, char *label);
char *menu_get_line_edit(Menu *menu, int i);
char *menu_get_name(Menu *menu, int i);
int menu_start_hbox(Menu *menu);
int menu_end_hbox(Menu *menu);
void menu_set_highlighted_item(Menu *menu, int i);

int menu_handle_mouse_release(Menu *menu, int x, int y, int b);
void menu_handle_mouse(Menu *menu, int x, int y);
int menu_handle_key_press(Menu *menu, int mods, int keysym);
void menu_handle_joystick_axis(Menu *menu, int axis, float value);
int menu_handle_joystick_button(Menu *menu, int button, int state);

void menu_highlight_left(Menu *menu);
void menu_highlight_right(Menu *menu);
void menu_highlight_up(Menu *menu);
void menu_highlight_down(Menu *menu);

void menu_render(Menu *menu, int view_width, int view_height, float scale);
