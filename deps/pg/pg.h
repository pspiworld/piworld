
#pragma once

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

void pg_start();
void pg_end();
uint32_t pg_get_screen_width();
uint32_t pg_get_screen_height();
void pg_swap_buffers();
int pg_get_gpu_mem_size();
double pg_get_time(void);
void pg_set_time(double time);

