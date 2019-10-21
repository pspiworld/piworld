
#pragma once

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

void pg_print_info(void);
void pg_start(char *title, int x, int y, int width, int height);
void pg_end(void);
void pg_swap_buffers(void);
void pg_swap_interval(int interval);
int pg_get_gpu_mem_size(void);
double pg_get_time(void);
void pg_set_time(double time);
void pg_get_window_size(int *width, int *height);
void pg_set_window_geometry(int x, int y, uint32_t width, uint32_t height);

