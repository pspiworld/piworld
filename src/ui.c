#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "pw.h"
#include "util.h"
#include "x11_event_handler.h"

#define INITIAL_MENU_SIZE 8

void menu_clear_items(Menu *menu)
{
    if (menu->allocated_item_count > 0) {
        free(menu->items);
    }
    menu->item_count = 0;
    menu->allocated_item_count = 0;
    menu->first_item_shown = 0;
}

void menu_set_title(Menu *menu, char *title)
{
    snprintf(menu->title, MAX_TEXT_LENGTH, title);
}

static int cmp_menu_name(const void *p1, const void *p2)
{
    return strcasecmp(((MenuItem *)p1)->name, ((MenuItem *)p2)->name);
}

void menu_sort(Menu *menu)
{
    qsort(menu->items, menu->item_count, sizeof(MenuItem), cmp_menu_name);
}

MenuItem* menu_add_item(Menu *menu, char *name)
{
    if (menu->item_count == 0) {
        menu->items = malloc(sizeof(MenuItem) * INITIAL_MENU_SIZE);
        menu->allocated_item_count = INITIAL_MENU_SIZE;
    }
    if (menu->item_count + 1 > menu->allocated_item_count) {
        menu->items = realloc(menu->items, sizeof(MenuItem) *
                              (menu->item_count + 1));
        menu->allocated_item_count = menu->item_count + 1;
    }
    MenuItem *item = &menu->items[menu->item_count];
    snprintf(item->name, MAX_TEXT_LENGTH, name);
    item->type = MENU_BUTTON;
    item->text[0] = '\0';
    item->text_cursor = 0;
    menu->item_count++;
    return item;
}

int menu_add(Menu *menu, char *name)
{
    menu_add_item(menu, name);
    return menu->item_count;
}

int menu_add_option(Menu *menu, char *name)
{
    MenuItem *item = menu_add_item(menu, name);
    if (item) {
        item->type = MENU_OPTION;
        item->data = 0;
    }
    return menu->item_count;
}

void menu_set_option(Menu *menu, int i, int value)
{
    menu->items[i - 1].data = value;
}

int menu_get_option(Menu *menu, int i)
{
    return menu->items[i - 1].data;
}

int menu_add_line_edit(Menu *menu, char *label)
{
    MenuItem *item = menu_add_item(menu, label);
    if (item) {
        item->type = MENU_LINE_EDIT;
    }
    return menu->item_count;
}

char *menu_get_line_edit(Menu *menu, int i)
{
    return menu->items[i - 1].text;
}

void menu_set_text(Menu *menu, int i, char *text)
{
    snprintf(menu->items[i - 1].text, MAX_TEXT_LENGTH, "%s", text);
}

char *menu_get_name(Menu *menu, int i)
{
    return menu->items[i - 1].name;
}

void menu_set_highlighted_item(Menu *menu, int i)
{
    menu->highlighted_item = i;
}

void menu_scroll_up(Menu *menu)
{
    if (menu->first_item_shown > 1) {
        menu->first_item_shown--;
    }
}

void menu_scroll_down(Menu *menu)
{
    if (menu->item_count >=
        (menu->max_menu_row_count + menu->first_item_shown)) {
        menu->first_item_shown++;
    }
}

void menu_highlight_next(Menu *menu)
{
    menu->highlighted_item = (menu->highlighted_item + 1) %
                             (menu->item_count + 1);
    if (menu->highlighted_item == 0) {
        menu->highlighted_item = 1;
    }
    if (menu->item_count >= (menu->max_menu_row_count +
        menu->first_item_shown)) {
        int last_item_in_view = menu->first_item_shown +
                                menu->max_menu_row_count;
        if (last_item_in_view == menu->highlighted_item) {
            menu->first_item_shown++;
        }
    }
    if (menu->highlighted_item == 1) {
        menu->first_item_shown = 1;
    }
}

void menu_highlight_previous(Menu *menu)
{
    menu->highlighted_item -= 1;
    if (menu->highlighted_item <= 0) {
        menu->highlighted_item = menu->item_count;
    }
    if (menu->first_item_shown > 1) {
        if (menu->first_item_shown > menu->highlighted_item) {
            menu->first_item_shown--;
        }
    }
    if (menu->highlighted_item == menu->item_count) {
        menu->first_item_shown = menu->item_count -
                                 menu->max_menu_row_count + 1;
    }
}

void menu_activate_item(Menu *menu, int i)
{
    MenuItem *item = &menu->items[i - 1];
    if (item->type == MENU_OPTION) {
        item->data = item->data ? 0 : 1;
    }
}

void menu_insert_into_typing_buffer(MenuItem *item, unsigned char c) {
    size_t n = strlen(item->text);
    if (n < MAX_TEXT_LENGTH - 1) {
        if (item->text_cursor != n) {
            // Shift text after the text cursor to the right
            memmove(item->text + item->text_cursor + 1,
                    item->text + item->text_cursor,
                    n - item->text_cursor);
        }
        item->text[item->text_cursor] = c;
        item->text[n + 1] = '\0';
        item->text_cursor += 1;
    }
}

int menu_handle_key_press_typing(MenuItem *item, int i,
    int mods __attribute__ ((unused)), int keysym)
{
    int result = MENU_REMAINS_OPEN;
    switch (keysym) {
    case XK_BackSpace:
        {
        size_t n = strlen(item->text);
        if (n > 0 && item->text_cursor > 0) {
            if (item->text_cursor < n) {
                memmove(item->text + item->text_cursor - 1,
                        item->text + item->text_cursor,
                        n - item->text_cursor);
            }
            item->text[n - 1] = '\0';
            item->text_cursor -= 1;
        }
        break;
        }
    case XK_Return:
        result = i;
        break;
    case XK_Delete:
        {
        size_t n = strlen(item->text);
        if (n > 0 && item->text_cursor < n) {
            memmove(item->text + item->text_cursor,
                    item->text + item->text_cursor + 1,
                    n - item->text_cursor);
            item->text[n - 1] = '\0';
        }
        break;
        }
    case XK_Left:
        if (item->text_cursor > 0) {
            item->text_cursor -= 1;
        }
        break;
    case XK_Right:
        if (item->text_cursor < strlen(item->text)) {
            item->text_cursor += 1;
        }
        break;
    case XK_Home:
        item->text_cursor = 0;
        break;
    case XK_End:
        item->text_cursor = strlen(item->text);
        break;
    default:
        if (keysym >= XK_space && keysym <= XK_asciitilde) {
            menu_insert_into_typing_buffer(item, keysym);
        }
        break;
    }
    return result;
}

int menu_handle_key_press(Menu *menu, int mods, int keysym)
{
    int result = MENU_REMAINS_OPEN;
    switch (keysym) {
    case XK_Escape:
        result = MENU_CANCELLED;
        break;
    case XK_Up:
        menu_highlight_previous(menu);
        break;
    case XK_Down:
        menu_highlight_next(menu);
        break;
    case XK_Return:
        result = menu->highlighted_item;
        menu_activate_item(menu, menu->highlighted_item);
        break;
    default:
        {
        MenuItem *item = &menu->items[menu->highlighted_item - 1];
        if (item->type == MENU_LINE_EDIT) {
            result = menu_handle_key_press_typing(item,
                menu->highlighted_item, mods, keysym);
        }
        }
        break;
    }
    return result;
}

int menu_handle_mouse_release(Menu *menu, int x, int y, int b)
{
    int result = 0;
    y += MOUSE_CURSOR_SIZE;  // account for cursor hotspot
    for (int i=0; i<menu->item_count; i++) {
        MenuItem *item = &menu->items[i];
        if (y >= (item->y - item->h) && y <= item->y &&
            x >= item->x && x <= (item->x + item->w)) {
            if (b == 4) {
                menu_scroll_up(menu);
            } else if (b == 5) {
                menu_scroll_down(menu);
            } else {
                if (item->type == MENU_LINE_EDIT) {
                    // Position the text cursor
                    size_t tx = (x - item->x) / menu->font_size - 1;
                    tx = MIN(strlen(item->text), tx);
                    item->text_cursor = tx;
                    menu->highlighted_item = i + 1;
                } else {
                    result = i + 1;
                    menu_activate_item(menu, menu->hover_item);
                }
            }
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

void menu_handle_joystick_axis(Menu *menu, int axis, float value)
{
    if (axis == 1 || axis == 5) {
        if (value < 0) {
            menu_highlight_previous(menu);
        } else if (value > 0) {
            menu_highlight_next(menu);
        }
    }
}

int menu_handle_joystick_button(Menu *menu, int button, int state)
{
    int result = MENU_REMAINS_OPEN;
    if (button == 0) {
        if (state) {
            result = menu->highlighted_item;
            menu_activate_item(menu, menu->highlighted_item);
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

void menu_render(Menu *menu, Attrib *text_attrib, Attrib *line_attrib,
                 int view_width, int view_height)
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
    menu->max_menu_row_count = view_height / (menu_text_size * 2) - 1;
    if (menu->first_item_shown <= 0) {
        menu->first_item_shown = 1;
    }
    menu_height = MIN(menu_height,
                     (menu->max_menu_row_count + 1) * menu_text_size * 2);
    float menu_title_bg_color[4] = {0.8, 0.2, 0.2, 1.0};
    float menu_title_text_color[4] = {0.15, 0.15, 0.15, 1.0};
    float menu_bg_color[4] = {0.4, 0.4, 0.4, 0.8};
    float menu_text_color[4] = {0.85, 0.85, 0.85, 1.0};
    float highlighted_color[4] = {0.85, 0.0, 0.0, 1.0};
    float hover_color[4] = {0.85, 0.85, 0.0, 1.0};
    float line_edit_color[4] = {0.15, 0.85, 0.15, 1.0};
    float line_edit_label_color[4] = {0.2, 0.2, 0.2, 0.8};
    float *text_color = menu_text_color;
    menu->font_size = menu_text_size;

    // Title
    int pad_len = menu_line_len - (strlen(menu->title) + 2);
    pad_len = MAX(0, pad_len);
    snprintf(text_buffer, 1024, " %s%*.*s ", menu->title, pad_len, pad_len, "");
    render_text_rgba(text_attrib, ALIGN_CENTER, view_width/2,
                view_height/2 + menu_height/2 - menu_text_size,
                menu_text_size, text_buffer, menu_title_bg_color,
                menu_title_text_color);

    // Items
    for (int i=menu->first_item_shown-1; i<menu->item_count; i++) {
        if (i-menu->first_item_shown+1 == menu->max_menu_row_count) {
            break;
        }
        MenuItem *item = &menu->items[i];
        char item_text[MAX_TEXT_LENGTH + 2];
        if (item->type == MENU_OPTION) {
            snprintf(item_text, MAX_TEXT_LENGTH + 2, "%s %s",
                     (item->data == 0) ? "-" : "+",
                     item->name);
        } else if (item->type == MENU_LINE_EDIT) {
            snprintf(item_text, MAX_TEXT_LENGTH, "%s", item->text);
        } else {
            snprintf(item_text, MAX_TEXT_LENGTH, "%s", item->name);
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
        item->y = view_height/2 - (menu_text_size * (2*(i+1))) +
                  menu_height/2 + ((menu->first_item_shown-1)*menu_text_size*2);
        item->w = menu_text_size * strlen(text_buffer);
        item->h = menu_text_size * 2;
        if (item->type == MENU_LINE_EDIT) {
            float cursor_x = item->x + menu_text_size * item->text_cursor;
            float cursor_y = item->y - menu_text_size;
            if (menu->hover_item == i+1 || menu->highlighted_item == i+1) {
                cursor_x += menu_text_size;
            }
            // Line edit text
            text_color = line_edit_color;
            render_text_rgba(text_attrib, ALIGN_CENTER, view_width/2,
                item->y - menu_text_size, menu_text_size, text_buffer,
                menu_bg_color, text_color);
            // Label
            if (strlen(item->text) == 0) {
                glClear(GL_DEPTH_BUFFER_BIT);
                render_text_rgba(text_attrib, ALIGN_CENTER, view_width/2,
                    item->y - menu_text_size, menu_text_size, item->name,
                    menu_bg_color, line_edit_label_color);
            }
            // Text cursor
            glClear(GL_DEPTH_BUFFER_BIT);
            render_text_cursor(line_attrib, cursor_x, cursor_y);
        } else {
            render_text_rgba(text_attrib, ALIGN_CENTER, view_width/2,
                item->y - menu_text_size, menu_text_size, text_buffer,
                menu_bg_color, text_color);
        }
    }
}
