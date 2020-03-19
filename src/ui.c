#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "pw.h"
#include "util.h"
#include "x11_event_handler.h"

void menu_set_title(Menu *d, char *title)
{
    snprintf(d->title, MAX_TITLE_LENGTH, title);
}

MenuItem* menu_add_item(Menu *d, char *name)
{
    if (d->item_count + 1 > MAX_MENU_ITEMS) {
        printf("Too many items on menu - ignored: %s\n", name);
        return NULL;
    }
    MenuItem *item = &d->items[d->item_count];
    snprintf(item->name, MAX_TITLE_LENGTH, name);
    item->type = MENU_BUTTON;
    d->item_count++;
    return item;
}

int menu_add(Menu *d, char *name)
{
    menu_add_item(d, name);
    return d->item_count;
}

int menu_add_option(Menu *d, char *name)
{
    MenuItem *item = menu_add_item(d, name);
    if (item) {
        item->type = MENU_OPTION;
        item->data = 0;
    }
    return d->item_count;
}

void menu_set_option(Menu *d, int i, int value)
{
    d->items[i - 1].data = value;
}

int menu_get_option(Menu *d, int i)
{
    return d->items[i - 1].data;
}

void menu_highlight_next(Menu *d)
{
    d->highlighted_item = (d->highlighted_item + 1) % (d->item_count + 1);
    if (d->highlighted_item == 0) {
        d->highlighted_item = 1;
    }
}

void menu_highlight_previous(Menu *d)
{
    d->highlighted_item -= 1;
    if (d->highlighted_item <= 0) {
        d->highlighted_item = d->item_count;
    }
}

void menu_activate_item(Menu *d, int i)
{
    MenuItem *item = &d->items[i - 1];
    if (item->type == MENU_OPTION) {
        item->data = item->data ? 0 : 1;
    }
}

int menu_handle_key_press(Menu *d, int keysym)
{
    int result = MENU_REMAINS_OPEN;
    switch (keysym) {
    case XK_Escape:
        result = MENU_CANCELLED;
        break;
    case XK_Up:
        menu_highlight_previous(d);
        break;
    case XK_Down:
        menu_highlight_next(d);
        break;
    case XK_Return:
        result = d->highlighted_item;
        menu_activate_item(d, d->highlighted_item);
        break;
    }
    return result;
}

int menu_handle_mouse_release(Menu *menu, int x, int y)
{
    int result = 0;
    y += MOUSE_CURSOR_SIZE;  // account for cursor hotspot
    for (int i=0; i<menu->item_count; i++) {
        MenuItem *item = &menu->items[i];
        if (y >= (item->y - item->h) && y <= item->y &&
            x >= item->x && x <= (item->x + item->w)) {
            result = i + 1;
            menu_activate_item(menu, menu->hover_item);
            break;
        }
    }
    return result;
}

void menu_handle_mouse(Menu *menu, int x, int y)
{
    y += MOUSE_CURSOR_SIZE;  // account for cursor hotspot
    menu->hover_item = 0;
    for (int i=0; i<menu->item_count; i++) {
        MenuItem *item = &menu->items[i];
        if (y >= (item->y - item->h) && y <= item->y &&
            x >= item->x && x <= (item->x + item->w)) {
            menu->hover_item = i + 1;
            break;
        }
    }
}

void menu_handle_joystick_axis(Menu *d, int axis, float value)
{
    if (axis == 1 || axis == 5) {
        if (value < 0) {
            menu_highlight_previous(d);
        } else if (value > 0) {
            menu_highlight_next(d);
        }
    }
}

int menu_handle_joystick_button(Menu *d, int button, int state)
{
    int result = MENU_REMAINS_OPEN;
    if (button == 0) {
        if (state) {
            result = d->highlighted_item;
            menu_activate_item(d, d->highlighted_item);
        }
    } else if (button == 9) {
        if (state) {
            result = MENU_CANCELLED;
        }
    }
    return result;
}

int longest_str_len(Menu *menu)
{
    size_t longest = strlen(menu->title) + 2;;
    for (int i=0; i<menu->item_count; i++) {
        MenuItem *item = &menu->items[i];
        longest = MAX(longest, strlen(item->name));
        if (item->type == MENU_OPTION) {
            longest = MAX(longest, strlen(item->name) + 2);
        }
    }
    return longest;
}

void menu_render(Menu *menu, Attrib *text_attrib, int view_width,
                 int view_height)
{
    float ts = 8;  // font size
    char text_buffer[1024];
    int menu_line_count = menu->item_count + 1;  // items + title
    int menu_line_len = longest_str_len(menu);
    float menu_text_size = ts * 2;
    float menu_width = menu_line_len * menu_text_size + 2;
    float menu_height = menu_line_count * menu_text_size * 2;
    if (menu_height > view_height || menu_width > view_width) {
        menu_text_size = ts;
        menu_width = menu_line_len * menu_text_size + 2;
        menu_height = menu_line_count * menu_text_size * 2;
    }
    float menu_title_bg_color[4] = {0.8, 0.2, 0.2, 1.0};
    float menu_title_text_color[4] = {0.15, 0.15, 0.15, 1.0};
    float menu_bg_color[4] = {0.4, 0.4, 0.4, 0.8};
    float menu_text_color[4] = {0.85, 0.85, 0.85, 1.0};
    float highlighted_color[4] = {0.85, 0.0, 0.0, 1.0};
    float hover_color[4] = {0.85, 0.85, 0.0, 1.0};
    float *text_color = menu_text_color;

    // Title
    int pad_len = menu_line_len - (strlen(menu->title) + 2);
    pad_len = MAX(0, pad_len);
    snprintf(text_buffer, 1024, " %s%*.*s ", menu->title, pad_len, pad_len, "");
    render_text_rgba(text_attrib, ALIGN_CENTER, view_width/2,
                view_height/2 + menu_height/2 - menu_text_size,
                menu_text_size, text_buffer, menu_title_bg_color,
                menu_title_text_color);

    // Items
    for (int i=0; i<menu->item_count; i++) {
        MenuItem *item = &menu->items[i];
        char item_text[MAX_TITLE_LENGTH + 2];
        if (item->type == MENU_OPTION) {
            snprintf(item_text, MAX_TITLE_LENGTH + 2, "%s %s",
                     (item->data == 0) ? "-" : "+",
                     item->name);
        } else {
            snprintf(item_text, MAX_TITLE_LENGTH + 2, "%s", item->name);
        }
        pad_len = menu_line_len - strlen(item_text);
        pad_len = MAX(0, pad_len);
        if (menu->hover_item == i+1) {
            snprintf(text_buffer, 1024, ">%s%*.*s<", item_text, pad_len,
                     pad_len, "");
            text_color = hover_color;
        } else if (menu->highlighted_item == i+1) {
            snprintf(text_buffer, 1024, ">%s%*.*s<", item_text, pad_len,
                     pad_len, "");
            text_color = highlighted_color;
        } else {
            snprintf(text_buffer, 1024, "%s%*.*s", item_text, pad_len,
                     pad_len, "");
            text_color = menu_text_color;
        }
        item->x = view_width/2 - (menu_text_size *
                  (strlen(text_buffer) - 1) / 2) - (menu_text_size/2);
        item->y = view_height/2 - (menu_text_size * (2*(i+1))) + menu_height/2;
        item->w = menu_text_size * strlen(text_buffer);
        item->h = menu_text_size * 2;
        render_text_rgba(text_attrib, ALIGN_CENTER, view_width/2,
            item->y - menu_text_size, menu_text_size, text_buffer,
            menu_bg_color, text_color);
    }
}
