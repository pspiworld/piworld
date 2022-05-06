#pragma once
#include "player.h"
#include "pwlua.h"
#include "sign.h"
#include "ui.h"
#include "vt.h"

#define MAX_HISTORY_SIZE 20

#define CHAT_HISTORY 0
#define COMMAND_HISTORY 1
#define SIGN_HISTORY 2
#define LUA_HISTORY 3
#define MENU_LINE_EDIT_HISTORY 4
#define NUM_HISTORIES 5
#define NOT_IN_HISTORY -1

#define MAX_MOUSE_BUTTONS 10
#define MAX_GAMEPAD_BUTTONS 16
#define MAX_GAMEPAD_AXES 6

typedef struct {
    char lines[MAX_HISTORY_SIZE][MAX_TEXT_LENGTH];
    int size;
    int end;
    size_t line_start;
} TextLineHistory;

typedef struct {
    int x;
    int y;
    int z;
    int w;
} Block;

typedef struct {
    int x;
    int y;
    int z;
    int texture;
    int extra;
    int light;
    int shape;
    int transform;
    SignList signs;
    int has_sign;
} UndoBlock;

typedef enum {
    MOUSE_BUTTON,
    MOUSE_MOTION,
    KEYBOARD,
    JOYSTICK_BUTTON,
    JOYSTICK_AXIS
} EventType;

typedef struct {
    EventType type;
    int button;  // mouse button, joystick button, axis or keysym
    int state;   // button/key up/down state
    float x;     // axis value or mouse x
    float y;     // mouse y
    int mods;    // keyboard mods at time of event
} Event;

struct LocalPlayer;
typedef void (*action_t)(struct LocalPlayer *, Event *);

typedef struct { int key; action_t value; } KeyBinding;

enum Focus { GameFocus, CommandLineFocus, VTFocus };

typedef struct LocalPlayer {
    Player *player;
    int item_index;
    int flying;
    float dy;
    char messages[MAX_MESSAGES][MAX_TEXT_LENGTH];
    int message_index;

    enum Focus typing;
    char typing_buffer[MAX_TEXT_LENGTH];
    TextLineHistory typing_history[NUM_HISTORIES];
    int history_position;
    size_t text_cursor;
    size_t typing_start;

    LuaThreadState* lua_shell;

    int mouse_x;
    int mouse_y;
    Menu menu;
    int menu_id_resume;
    int menu_id_options;
    int menu_id_new;
    int menu_id_load;
    int menu_id_exit;
    Menu menu_options;
    int menu_id_script;
    int menu_id_crosshairs;
    int menu_id_fullscreen;
    int menu_id_verbose;
    int menu_id_wireframe;
    int menu_id_worldgen;
    int menu_id_options_command_lines;
    int menu_id_options_edit_block;
    int menu_id_options_item_in_hand;
    int menu_id_options_resume;
    Menu menu_new;
    int menu_id_new_game_name;
    int menu_id_new_ok;
    int menu_id_new_cancel;
    Menu menu_load;
    int menu_id_load_cancel;
    Menu menu_item_in_hand;
    int menu_id_item_in_hand_cancel;
    Menu menu_block_edit;
    int menu_id_block_edit_resume;
    int menu_id_texture;
    int menu_id_sign_text;
    int menu_id_light;
    int menu_id_control_bit;
    int menu_id_open_bit;
    int menu_id_shape;
    int menu_id_transform;
    int edit_x, edit_y, edit_z, edit_face;
    Menu menu_texture;
    int menu_id_texture_cancel;
    Menu menu_shape;
    int menu_id_shape_cancel;
    Menu menu_transform;
    int menu_id_transform_cancel;
    Menu menu_script;
    int menu_id_script_cancel;
    int menu_id_script_run;
    Menu menu_script_run;
    char menu_script_run_dir[MAX_DIR_LENGTH];
    int menu_id_script_run_cancel;
    Menu menu_worldgen;
    int menu_id_worldgen_cancel;
    int menu_id_worldgen_select;
    Menu menu_worldgen_select;
    char menu_worldgen_dir[MAX_DIR_LENGTH];
    int menu_id_worldgen_select_cancel;
    Menu menu_command_lines;
    int menu_id_action;
    int menu_id_chat;
    int menu_id_lua;
    int menu_id_sign;
    int menu_id_vt;
    Menu osk;
    int osk_id_ok;
    int osk_id_cancel;
    int osk_id_backspace;
    int osk_id_delete;
    int osk_id_space;
    int osk_id_left;
    int osk_id_right;
    int osk_id_up;
    int osk_id_down;
    int osk_id_home;
    int osk_id_end;
    Menu *active_menu;

    Menu *osk_open_for_menu;
    int osk_open_for_menu_line_edit_id;

    int view_x;
    int view_y;
    int view_width;
    int view_height;

    int forward_is_pressed;
    int back_is_pressed;
    int left_is_pressed;
    int right_is_pressed;
    int jump_is_pressed;
    int crouch_is_pressed;
    int view_left_is_pressed;
    int view_right_is_pressed;
    int view_up_is_pressed;
    int view_down_is_pressed;
    int ortho_is_pressed;
    int zoom_is_pressed;
    float view_speed_left_right;
    float view_speed_up_down;
    float movement_speed_left_right;
    float movement_speed_forward_back;
    int shoulder_button_mode;

    Block block0;
    Block block1;
    Block copy0;
    Block copy1;

    UndoBlock undo_block;
    int has_undo_block;

    int observe1;
    int observe1_client_id;
    int observe2;
    int observe2_client_id;

    int mouse_id;
    int keyboard_id;
    int joystick_id;

    action_t mouse_bindings[MAX_MOUSE_BUTTONS];
    action_t gamepad_bindings[MAX_GAMEPAD_BUTTONS];
    action_t gamepad_axes_bindings[MAX_GAMEPAD_AXES];
    KeyBinding* key_bindings;

    int vt_open;
    float vt_scale;
    PiWorldTerm *pwt;

    int show_world;
} LocalPlayer;

void local_player_init(LocalPlayer *local, Player *player, int player_id);
void local_player_reset(LocalPlayer *local);
void local_player_clear(LocalPlayer *local);
void history_add(TextLineHistory *history, char *line);
void history_previous(TextLineHistory *history, char *line, int *position);
void history_next(TextLineHistory *history, char *line, int *position);
TextLineHistory* current_history(LocalPlayer *local);
void reset_history(LocalPlayer *local);
void history_load(LocalPlayer *local);
void history_save(LocalPlayer *local);
void handle_menu_event(LocalPlayer *local, Menu *menu, int event);
void open_menu(LocalPlayer *local, Menu *menu);
void close_menu(LocalPlayer *local);
void cancel_player_inputs(LocalPlayer *p);
void populate_block_edit_menu(LocalPlayer *local, int w, char *sign_text,
    int light, int extra, int shape, int transform);
void set_item_in_hand_to_item_under_crosshair(LocalPlayer *player);
void set_block_under_crosshair(LocalPlayer *player);
void clear_block_under_crosshair(LocalPlayer *player);
void open_menu_for_item_under_crosshair(LocalPlayer *local);
void cycle_item_in_hand_down(LocalPlayer *player);
void cycle_item_in_hand_up(LocalPlayer *player);
void on_light(LocalPlayer *local);
void handle_movement(double dt, LocalPlayer *local);
void open_chat_command_line(LocalPlayer* local);
void open_action_command_line(LocalPlayer* local);
void open_lua_command_line(LocalPlayer* local);
void open_sign_command_line(LocalPlayer* local);
void open_menu_line_edit_command_line(LocalPlayer* local);
void open_osk_for_menu_line_edit(LocalPlayer *local, int item);
void local_player_set_vt_size(LocalPlayer* local);
void open_vt(LocalPlayer* local);
void close_vt(LocalPlayer* local);
void destroy_vt(LocalPlayer* local);
int use_osk(LocalPlayer *local);
