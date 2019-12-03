#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "lodepng.h"
#include "pg.h"
#include "matrix.h"
#include "util.h"

int rand_int(int n) {
    int result;
    while (n <= (result = rand() / (RAND_MAX / n)));
    return result;
}

double rand_double() {
    return (double)rand() / (double)RAND_MAX;
}

void update_fps(FPS *fps) {
    fps->frames++;
    double now = pg_get_time();
    double elapsed = now - fps->since;
    if (elapsed >= 1) {
        fps->fps = round(fps->frames / elapsed);
        fps->frames = 0;
        fps->since = now;
    }
}

char *load_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "fopen %s failed: %d %s\n", path, errno, strerror(errno));
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);
    char *data = calloc(length + 1, sizeof(char));
    if (fread(data, 1, length, file) != (size_t)length) {
        fprintf(stderr, "fread failed: %s\n", path);
        exit(-1);
    }
    fclose(file);
    return data;
}

GLuint gen_buffer(GLsizei size, GLfloat *data) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return buffer;
}

void del_buffer(GLuint buffer) {
    glDeleteBuffers(1, &buffer);
}

GLfloat *malloc_faces(int components, int faces) {
    return malloc(sizeof(GLfloat) * 6 * components * faces);
}

GLfloat *malloc_faces_with_rgba(int components, int faces) {
    return malloc(sizeof(GLfloat) * 6 * components * faces * 4);
}

GLuint gen_faces(int components, int faces, GLfloat *data) {
    GLuint buffer = gen_buffer(
        sizeof(GLfloat) * 6 * components * faces, data);
    free(data);
    return buffer;
}

GLuint gen_faces_with_rgba(int components, int faces, GLfloat *data) {
    GLuint buffer = gen_buffer(
        sizeof(GLfloat) * 6 * components * faces * 4, data);
    free(data);
    return buffer;
}

GLuint make_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        GLchar *info = calloc(length, sizeof(GLchar));
        glGetShaderInfoLog(shader, length, NULL, info);
        fprintf(stderr, "glCompileShader failed:\n%s\n", info);
        free(info);
    }
    return shader;
}

GLuint load_shader(GLenum type, const char *path) {
    char *data = load_file(path);
    GLuint result = make_shader(type, data);
    free(data);
    return result;
}

GLuint make_program(GLuint shader1, GLuint shader2) {
    GLuint program = glCreateProgram();
    glAttachShader(program, shader1);
    glAttachShader(program, shader2);
    glLinkProgram(program);
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        GLchar *info = calloc(length, sizeof(GLchar));
        glGetProgramInfoLog(program, length, NULL, info);
        fprintf(stderr, "glLinkProgram failed: %s\n", info);
        free(info);
    }
    glDetachShader(program, shader1);
    glDetachShader(program, shader2);
    glDeleteShader(shader1);
    glDeleteShader(shader2);
    return program;
}

GLuint load_program(const char *path1, const char *path2) {
    GLuint shader1 = load_shader(GL_VERTEX_SHADER, path1);
    GLuint shader2 = load_shader(GL_FRAGMENT_SHADER, path2);
    GLuint program = make_program(shader1, shader2);
    return program;
}

void flip_image_vertical(
    unsigned char *data, unsigned int width, unsigned int height)
{
    unsigned int size = width * height * 4;
    unsigned int stride = sizeof(char) * width * 4;
    unsigned char *new_data = malloc(sizeof(unsigned char) * size);
    for (unsigned int i = 0; i < height; i++) {
        unsigned int j = height - i - 1;
        memcpy(new_data + j * stride, data + i * stride, stride);
    }
    memcpy(data, new_data, size);
    free(new_data);
}

void load_png_texture(const char *file_name) {
    unsigned int error;
    unsigned char *data;
    unsigned int width, height;
    error = lodepng_decode32_file(&data, &width, &height, file_name);
    if (error) {
        fprintf(stderr, "load_png_texture %s failed, error %u: %s\n", file_name, error, lodepng_error_text(error));
        exit(1);
    }
    flip_image_vertical(data, width, height);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, data);
    free(data);
}

#define DDS_HEADER_SIZE 128
int load_etc1_in_dds_texture(const char *file_name) {
    unsigned int width, height;
    unsigned int texture_length;
    char *data = load_file(file_name);
    if (strncmp(data, "DDS ", 4) != 0) {
        printf("Not a DDS file: %s\n", file_name);
        return -1;
    }
    if (strncmp(&data[84], "ETC ", 4) != 0) {
        printf("Not an ETC file: %s\n", file_name);
        return -2;
    }
    height = *(unsigned int*)&data[12];
    width = *(unsigned int*)&data[16];
    texture_length = *(unsigned int*)&data[20];
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_ETC1_RGB8_OES, width, height, 0,
                           texture_length, &data[DDS_HEADER_SIZE]);
    free(data);
    return 0;
}

void load_texture(const char *name) {
    // Try loading an ETC1 version of the texture at bin/name.dds, if it is not
    // present or is older than the original PNG file use the PNG file instead.
    struct stat st, st2;
    char dds_file_name[MAX_PATH_LENGTH];
    char png_file_name[MAX_PATH_LENGTH];
    snprintf(dds_file_name, MAX_PATH_LENGTH, "./bin/%s.dds", name);
    snprintf(png_file_name, MAX_PATH_LENGTH, "./textures/%s.png", name);
    if (stat(dds_file_name, &st) == 0 && stat(png_file_name, &st2) == 0 &&
        st.st_mtime > st2.st_mtime &&
        load_etc1_in_dds_texture(dds_file_name) == 0) {
        return;  // ETC1 texture will be used
    }
    load_png_texture(png_file_name);
}

char *tokenize(char *str, const char *delim, char **key) {
    char *result;
    if (str == NULL) {
        str = *key;
    }
    str += strspn(str, delim);
    if (*str == '\0') {
        return NULL;
    }
    result = str;
    str += strcspn(str, delim);
    if (*str) {
        *str++ = '\0';
    }
    *key = str;
    return result;
}

int char_width(unsigned char input) {
    static const int lookup[128] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        4, 2, 4, 7, 6, 9, 7, 2, 3, 3, 4, 6, 3, 5, 2, 7,
        6, 3, 6, 6, 6, 6, 6, 6, 6, 6, 2, 3, 5, 6, 5, 7,
        8, 6, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 5, 8, 8, 6,
        6, 7, 6, 6, 6, 6, 8, 8, 8, 6, 6, 3, 6, 3, 6, 6,
        4, 7, 6, 6, 6, 6, 5, 6, 6, 2, 5, 5, 2, 9, 6, 6,
        6, 6, 6, 6, 5, 6, 6, 6, 6, 6, 6, 4, 2, 5, 7, 0
    };
    return lookup[input];
}

int string_width(const char *input) {
    int result = 0;
    int length = strlen(input);
    for (int i = 0; i < length; i++) {
        if (input[i] == '\\' && input[i+1] != '\0' && input[i+1] != ' ') {
            // ignore markup characters in string width
            // run until next space or end.
            while (input[i] != ' ' && i < length) {
                i += 1;
            }
            continue;
        }
        result += char_width(input[i]);
    }
    return result;
}

int wrap(const char *input, int max_width, char *output, int max_length) {
    *output = '\0';
    char *text = malloc(sizeof(char) * (strlen(input) + 1));
    strcpy(text, input);
    int space_width = char_width(' ');
    int line_number = 0;
    char *key1, *key2;
    char *line = tokenize(text, "\r\n", &key1);
    while (line) {
        int line_width = 0;
        char *token = tokenize(line, " ", &key2);
        while (token) {
            int token_width = string_width(token);
            if (line_width && token[0] != '\\') {
                if (line_width + token_width > max_width) {
                    line_width = 0;
                    line_number++;
                    strncat(output, "\n", max_length - strlen(output) - 1);
                }
                else {
                    strncat(output, " ", max_length - strlen(output) - 1);
                }
            }
            strncat(output, token, max_length - strlen(output) - 1);
            if (token[0] != '\\') {
                line_width += token_width + space_width;
            } else {
                // preserve space around markup
                strncat(output, " ", max_length - strlen(output) - 1);
            }
            token = tokenize(NULL, " ", &key2);
        }
        line_number++;
        strncat(output, "\n", max_length - strlen(output) - 1);
        line = tokenize(NULL, "\r\n", &key1);
    }
    free(text);
    return line_number;
}

void color_from_text(const char *text, float *r, float *g, float *b) {
    if (strlen(text) == 1) {
        // Single letter colour codes
        if (text[0] == 'r') {         // red
            *r = 1.0;
            *g = 0.0;
            *b = 0.0;
        } else if (text[0] == 'g') {  // green
            *r = 0.0;
            *g = 1.0;
            *b = 0.0;
        } else if (text[0] == 'b') {  // blue
            *r = 0.0;
            *g = 0.0;
            *b = 1.0;
        } else if (text[0] == 'o') {  // orange
            *r = 1.0;
            *g = 0.5;
            *b = 0.0;
        } else if (text[0] == 'p') {  // purple
            *r = 0.5;
            *g = 0.0;
            *b = 0.5;
        } else if (text[0] == 'y') {  // yellow
            *r = 1.0;
            *g = 1.0;
            *b = 0.0;
        } else if (text[0] == 'c') {  // cyan
            *r = 0.0;
            *g = 1.0;
            *b = 1.0;
        } else if (text[0] == 'm') {  // magenta
            *r = 1.0;
            *g = 0.0;
            *b = 1.0;
        } else if (text[0] == 'l') {  // black
            *r = 0.0;
            *g = 0.0;
            *b = 0.0;
        } else if (text[0] == 'w') {  // white
            *r = 1.0;
            *g = 1.0;
            *b = 1.0;
        } else if (text[0] == 's') {  // silver
            *r = 0.75;
            *g = 0.75;
            *b = 0.75;
        } else if (text[0] == 'e') {  // grey
            *r = 0.5;
            *g = 0.5;
            *b = 0.5;
        }
    } else if (strlen(text) == 4) {
        // #RGB
        char col[2];
        col[1] = '\0';
        col[0] = text[1];
        *r = (float)strtol(col, NULL, 16) / 15.0;
        col[0] = text[2];
        *g = (float)strtol(col, NULL, 16) / 15.0;
        col[0] = text[3];
        *b = (float)strtol(col, NULL, 16) / 15.0;
    } else if (strlen(text) == 7) {
        // #RRGGBB
        int num = (int)strtol(text + 1, NULL, 16);
        *r = (float)((unsigned char)(num >> (8 * 2))) / 255.0;
        *g = (float)((unsigned char)(num >> 8)) / 255.0;
        *b = (float)((unsigned char)(num)) / 255.0;
    }
}

#ifdef DEBUG
void _check_gl_error(const char *file, int line);

///
/// Usage
/// [... some opengl calls]
/// check_gl_error();
///
#define check_gl_error() _check_gl_error(__FILE__,__LINE__)

void _check_gl_error(const char *file, int line) {
    GLenum err = glGetError();

    while (err != GL_NO_ERROR) {
        #define MAX_ERROR_TEXT_LENGTH 64
        char error[MAX_ERROR_TEXT_LENGTH];

        switch(err) {
            case GL_INVALID_OPERATION:
                snprintf(error, MAX_ERROR_TEXT_LENGTH, "INVALID_OPERATION");
                break;
            case GL_INVALID_ENUM:
                snprintf(error, MAX_ERROR_TEXT_LENGTH, "INVALID_ENUM");
                break;
            case GL_INVALID_VALUE:
                snprintf(error, MAX_ERROR_TEXT_LENGTH, "INVALID_VALUE");
                break;
            case GL_OUT_OF_MEMORY:
                snprintf(error, MAX_ERROR_TEXT_LENGTH, "OUT_OF_MEMORY");
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                snprintf(error, MAX_ERROR_TEXT_LENGTH,
                         "INVALID_FRAMEBUFFER_OPERATION");
                break;
            default:
                snprintf(error, MAX_ERROR_TEXT_LENGTH, "[UNKNOWN ERROR - %d]",
                         err);
        }

        printf("GL_%s - %s: %d\n", error, file, line);
        err = glGetError();
    }
}
#endif

