#pragma once

#include <GLES2/gl2.h>
#include "chunk.h"
#include "player.h"

#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2

extern const float RED[4];
extern const float GREEN[4];
extern const float BLACK[4];

void render_init(void);
void render_deinit(void);
void render_set_state(State *player_state, int width, int height,
    int render_radius, int sign_radius, int ortho, float fov,
    float scale, int gl_float_type, size_t float_size);
int gen_sign_buffer(
    GLfloat *data, float x, float y, float z, int face, const char *text,
    float y_face_height);
int render_chunks(void);
void render_signs(void);
void render_sign(char *typing_buffer, int x, int y, int z, int face, float y_face_height);
void render_players(Player *player);
void render_sky(void);
void render_wireframe(int hx, int hy, int hz, const float color[4], float item_height);
void render_crosshairs(const float color[4]);
void render_item(int w);
void render_text_rgba(
    int justify, float x, float y, float n, char *text,
    const float *background, const float *text_color);
void render_text(
    int justify, float x, float y, float n, char *text);
void render_text_cursor(float x, float y);
void render_mouse_cursor(float x, float y, int p);

