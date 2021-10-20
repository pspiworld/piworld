#pragma once

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stddef.h>
#include "config.h"

#define PI 3.14159265359
#define DEGREES(radians) ((radians) * 180 / PI)
#define RADIANS(degrees) ((degrees) * PI / 180)
#define ABS(x) ((x) < 0 ? (-(x)) : (x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SIGN(x) (((x) > 0) - ((x) < 0))

#define MAX_COLOR_STRING_LENGTH 11

#define MAX_NAME_LENGTH 32
#define MAX_TEXT_LENGTH 256

#if DEBUG
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif

typedef struct {
    unsigned int fps;
    unsigned int frames;
    double since;
} FPS;

typedef unsigned short hfloat;

void init_data_dir(void);
char *get_data_dir(void);

int rand_int(int n);
double rand_double(void);
void update_fps(FPS *fps);
float time_of_day(void);

GLuint gen_buffer(GLsizei size, const void *data);
void del_buffer(GLuint buffer);
void *malloc_faces(int components, int faces, size_t type_size);
GLfloat *malloc_faces_with_rgba(int components, int faces);
GLuint gen_faces(int components, int faces, void *data, size_t type_size);
GLuint gen_faces_with_rgba(int components, int faces, GLfloat *data);
GLuint make_shader(GLenum type, const char *source);
GLuint load_shader(GLenum type, const char *path);
GLuint make_program(GLuint shader1, GLuint shader2);
GLuint load_program(const char *name);
void load_png_texture(const char *file_name);
void load_texture(const char *file_name);
char *tokenize(char *str, const char *delim, char **key);
int char_width(unsigned char input);
int string_width(const char *input);
int wrap(const char *input, int max_width, char *output, int max_length);
void color_from_text(const char *text, float *r, float *g, float *b);
hfloat float_to_hfloat(float *f);

#ifdef DEBUG
void _check_gl_error(const char *file, int line);

///
/// Usage
/// [... some opengl calls]
/// check_gl_error();
///
#define check_gl_error() _check_gl_error(__FILE__,__LINE__)
#endif

