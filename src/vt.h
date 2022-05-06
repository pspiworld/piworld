#pragma once

#include "render.h"
#include "vterm.h"

typedef struct {
    VTerm *vt;
    VTermScreen *vts;
    int rows;
    int cols;
    int master;
} PiWorldTerm;

void vt_init(PiWorldTerm *pwt, int width, int height);
void vt_deinit(PiWorldTerm *pwt);
void vt_set_size(PiWorldTerm *pwt, int cols, int rows);
void vt_draw(PiWorldTerm *pwt, float x, float y, float scale);
int vt_process(PiWorldTerm *pwt);
void vt_handle_key_press(PiWorldTerm *pwt, int mods, int keysym);

