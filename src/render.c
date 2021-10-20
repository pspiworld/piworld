#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "chunk.h"
#include "chunks.h"
#include "clients.h"
#include "cube.h"
#include "item.h"
#include "matrix.h"
#include "render.h"

const float RED[4] = {1.0, 0.0, 0.0, 1.0};
const float GREEN[4] = {0.0, 1.0, 0.0, 1.0};
const float BLACK[4] = {0.0, 0.0, 0.0, 1.0};

float hud_text_background[4] = {0.4, 0.4, 0.4, 0.4};
float hud_text_color[4] = {0.85, 0.85, 0.85, 1.0};

typedef struct {
    GLuint program;
    GLuint position;
    GLuint normal;
    GLuint uv;
    GLuint color;
    GLuint matrix;
    GLuint sampler;
    GLuint camera;
    GLuint timer;
    GLuint extra1;
    GLuint extra2;
    GLuint extra3;
    GLuint extra4;
    GLuint extra5;
    GLuint extra6;
    GLuint map;
} Attrib;

Attrib block_attrib = {0};
Attrib line_attrib = {0};
Attrib text_attrib = {0};
Attrib sky_attrib = {0};
Attrib mouse_attrib = {0};

GLuint sky_buffer;

typedef struct {
    State *player_state;
    int width;
    int height;
    int render_radius;
    int sign_radius;
    int ortho;
    float fov;
    float scale;
    int gl_float_type;
    size_t float_size;
} RenderState;

RenderState rs;

GLuint gen_sky_buffer(void);

void render_init(void)
{
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0, 0, 0, 1);

    // LOAD TEXTURES //
    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    load_texture("texture");

    GLuint sky;
    glGenTextures(1, &sky);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sky);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    load_texture("sky");

    GLuint sign;
    glGenTextures(1, &sign);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, sign);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    load_texture("sign");

    // LOAD SHADERS //
    GLuint program;

    program = load_program("block");
    block_attrib.program = program;
    block_attrib.position = glGetAttribLocation(program, "position");
    block_attrib.normal = glGetAttribLocation(program, "normal");
    block_attrib.uv = glGetAttribLocation(program, "uv");
    block_attrib.matrix = glGetUniformLocation(program, "matrix");
    block_attrib.sampler = glGetUniformLocation(program, "sampler");
    block_attrib.extra1 = glGetUniformLocation(program, "sky_sampler");
    block_attrib.extra2 = glGetUniformLocation(program, "daylight");
    block_attrib.extra3 = glGetUniformLocation(program, "fog_distance");
    block_attrib.extra4 = glGetUniformLocation(program, "ortho");
    block_attrib.camera = glGetUniformLocation(program, "camera");
    block_attrib.timer = glGetUniformLocation(program, "timer");
    block_attrib.map = glGetUniformLocation(program, "map");

    program = load_program("line");
    line_attrib.program = program;
    line_attrib.position = glGetAttribLocation(program, "position");
    line_attrib.matrix = glGetUniformLocation(program, "matrix");
    line_attrib.extra1 = glGetUniformLocation(program, "color");

    program = load_program("text");
    text_attrib.program = program;
    text_attrib.position = glGetAttribLocation(program, "position");
    text_attrib.uv = glGetAttribLocation(program, "uv");
    text_attrib.color = glGetAttribLocation(program, "color");
    text_attrib.matrix = glGetUniformLocation(program, "matrix");
    text_attrib.sampler = glGetUniformLocation(program, "sampler");
    text_attrib.extra1 = glGetUniformLocation(program, "is_sign");
    text_attrib.extra2 = glGetUniformLocation(program, "sky_sampler");
    text_attrib.extra3 = glGetUniformLocation(program, "fog_distance");
    text_attrib.extra4 = glGetUniformLocation(program, "ortho");
    text_attrib.extra5 = glGetUniformLocation(program, "hud_text_background");
    text_attrib.extra6 = glGetUniformLocation(program, "hud_text_color");
    text_attrib.timer = glGetUniformLocation(program, "timer");
    text_attrib.camera = glGetUniformLocation(program, "camera");

    program = load_program("sky");
    sky_attrib.program = program;
    sky_attrib.position = glGetAttribLocation(program, "position");
    sky_attrib.uv = glGetAttribLocation(program, "uv");
    sky_attrib.matrix = glGetUniformLocation(program, "matrix");
    sky_attrib.sampler = glGetUniformLocation(program, "sampler");
    sky_attrib.timer = glGetUniformLocation(program, "timer");

    program = load_program("mouse");
    mouse_attrib.program = program;
    mouse_attrib.position = glGetAttribLocation(program, "position");
    mouse_attrib.uv = glGetAttribLocation(program, "uv");
    mouse_attrib.matrix = glGetUniformLocation(program, "matrix");
    mouse_attrib.sampler = glGetUniformLocation(program, "sampler");

    sky_buffer = gen_sky_buffer();
}

void render_deinit(void)
{
    del_buffer(sky_buffer);
}

// Setup for the next set of render calls.
void render_set_state(State *player_state, int width, int height,
    int render_radius, int sign_radius, int ortho, float fov,
    float scale, int gl_float_type, size_t float_size)
{
    rs.player_state = player_state;
    rs.width = width;
    rs.height = height;
    rs.render_radius = render_radius;
    rs.sign_radius = sign_radius;
    rs.ortho = ortho;
    rs.fov = fov;
    rs.scale = scale;
    rs.gl_float_type = gl_float_type;
    rs.float_size = float_size;
}

float get_daylight(void)
{
    float timer = time_of_day();
    if (timer < 0.5) {
        float t = (timer - 0.25) * 100;
        return 1 / (1 + powf(2, -t));
    }
    else {
        float t = (timer - 0.85) * 100;
        return 1 - 1 / (1 + powf(2, -t));
    }
}

GLuint gen_sky_buffer(void) {
    float data[3072];
    GLint buffer;
    make_sphere(data, 1, 2);
    if (config->use_hfloat) {
        hfloat hdata[3072];
        for (size_t i=0; i<3072; i++) {
            hdata[i] = float_to_hfloat(data + i);
        }
        buffer = gen_buffer(sizeof(hdata), hdata);
    } else {
        buffer = gen_buffer(sizeof(data), data);
    }
    return buffer;
}

GLuint gen_cube_buffer(float x, float y, float z, float n, int w)
{
    GLfloat *data = malloc_faces(10, 6, sizeof(GLfloat));
    float ao[6][4] = {0};
    float light[6][4] = {
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5}
    };
    GLint buffer;
    make_cube(data, ao, light, 1, 1, 1, 1, 1, 1, x, y, z, n, w);
    if (config->use_hfloat) {
        hfloat *hdata = malloc_faces(10, 6, sizeof(hfloat));
        for (size_t i=0; i<(6 * 10 * 6); i++) {
            hdata[i] = float_to_hfloat(data + i);
        }
        buffer = gen_faces(10, 6, hdata, sizeof(hfloat));
        free(data);
    } else {
        buffer = gen_faces(10, 6, data, sizeof(GLfloat));
    }
    return buffer;
}

GLuint gen_plant_buffer(float x, float y, float z, float n, int w)
{
    GLfloat *data = malloc_faces(10, 4, sizeof(GLfloat));
    float ao = 0;
    float light = 1;
    GLint buffer;
    make_plant(data, ao, light, x, y, z, n, w, 45);
    if (config->use_hfloat) {
        hfloat *hdata = malloc_faces(10, 4, sizeof(hfloat));
        for (size_t i=0; i<(6 * 10 * 4); i++) {
            hdata[i] = float_to_hfloat(data + i);
        }
        buffer = gen_faces(10, 4, hdata, sizeof(hfloat));
        free(data);
    } else {
        buffer = gen_faces(10, 4, data, sizeof(GLfloat));
    }
    return buffer;
}

int gen_sign_buffer(
    GLfloat *data, float x, float y, float z, int face, const char *text,
    float y_face_height)
{
    static const int glyph_dx[8] = {0, 0, -1, 1, 1, 0, -1, 0};
    static const int glyph_dz[8] = {1, -1, 0, 0, 0, -1, 0, 1};
    static const int line_dx[8] = {0, 0, 0, 0, 0, 1, 0, -1};
    static const int line_dy[8] = {-1, -1, -1, -1, 0, 0, 0, 0};
    static const int line_dz[8] = {0, 0, 0, 0, 1, 0, -1, 0};
    if (face < 0 || face >= 8) {
        return 0;
    }

    // Align sign to the item shape
    float face_height_offset = 0;
    if (face >= 0 && face <= 3) { // side faces
        face_height_offset = 0.5 - (y_face_height / 2);
    } else if (face >= 4 && face <= 7) { // top faces
        face_height_offset = 1 - y_face_height;
    }

    float font_scaling = 1.0;
    float r = 0.0, g = 0.0, b = 0.0;
    if (strlen(text) > 2 && text[0] == '\\' &&
        (isdigit(text[1]) || text[1] == '.')) {
        font_scaling = atof(&text[1]);
    }
    int count = 0;
    float max_width = 64 / font_scaling;
    float line_height = 1.25;
    char lines[1024];
    int rows = wrap(text, max_width, lines, 1024);
    rows = MIN(rows, 5);
    int dx = glyph_dx[face];
    int dz = glyph_dz[face];
    int ldx = line_dx[face];
    int ldy = line_dy[face];
    int ldz = line_dz[face];
    float n = 1.0 / (max_width / 10);
    float sx = x - n * (rows - 1) * (line_height / 2) * ldx;
    float sy = y - n * (rows - 1) * (line_height / 2) * ldy -
               face_height_offset;
    float sz = z - n * (rows - 1) * (line_height / 2) * ldz;
    char *key;
    char *line = tokenize(lines, "\n", &key);
    while (line) {
        int length = strlen(line);
        int line_start = 0;
        int line_width = string_width(line + line_start);
        line_width = MIN(line_width, max_width);
        float rx = sx - dx * line_width / max_width / 2;
        float ry = sy;
        float rz = sz - dz * line_width / max_width / 2;
        for (int i = line_start; i < length; i++) {
            if (line[i] == '\\' && i+1 < length) {
                // process markup
                char color_text[MAX_COLOR_STRING_LENGTH];
                if (i+7 < length && line[i+1] == '#' && isxdigit(line[i+2]) &&
                    isxdigit(line[i+3]) && isxdigit(line[i+4]) &&
                    isxdigit(line[i+5]) && isxdigit(line[i+6]) &&
                    isxdigit(line[i+7])) {
                    strncpy(color_text, line + i + 1, 7);
                    color_text[MAX_COLOR_STRING_LENGTH - 1] = '\0';
                    color_text[7] = '\0';
                    color_from_text(color_text, &r, &g, &b);
                } else if (i+4 < length && line[i+1] == '#' &&
                           isxdigit(line[i+2]) && isxdigit(line[i+3]) &&
                           isxdigit(line[i+4])) {
                    strncpy(color_text, line + i + 1, 4);
                    color_text[MAX_COLOR_STRING_LENGTH - 1] = '\0';
                    color_text[4] = '\0';
                    color_from_text(color_text, &r, &g, &b);
                } else if (isalpha(line[i+1])) {
                    strncpy(color_text, line + i + 1, 1);
                    color_text[MAX_COLOR_STRING_LENGTH - 1] = '\0';
                    color_text[1] = '\0';
                    color_from_text(color_text, &r, &g, &b);
                }
                // eat all remaining markup text
                while (line[i] != ' ' && i < length) {
                    i++;
                }
                continue;  // do not process markup as displayable text
            }
            int width = char_width(line[i]);
            line_width -= width;
            if (line_width < 0) {
                break;
            }
            rx += dx * width / max_width / 2;
            rz += dz * width / max_width / 2;
            if (line[i] != ' ') {
                make_character_3d(
                    data + count * 54, rx, ry, rz, n / 2, face, line[i],
                    r, g, b);
                count++;
            }
            rx += dx * width / max_width / 2;
            rz += dz * width / max_width / 2;
        }
        sx += n * line_height * ldx;
        sy += n * line_height * ldy;
        sz += n * line_height * ldz;
        line = tokenize(NULL, "\n", &key);
        rows--;
        if (rows <= 0) {
            break;
        }
    }
    return count;
}

GLuint gen_crosshair_buffer(void)
{
    int x = rs.width / 2;
    int y = rs.height / 2;
    int p = 10 * rs.scale;
    float data[] = {
        x, y - p, x, y + p,
        x - p, y, x + p, y
    };
    return gen_buffer(sizeof(data), data);
}

GLuint gen_wireframe_buffer(float x, float y, float z, float n, float height)
{
    float data[72];
    make_cube_wireframe(data, x, y, z, n, height);
    return gen_buffer(sizeof(data), data);
}

GLuint gen_text_buffer(float x, float y, float n, char *text)
{
    int length = strlen(text);
    GLfloat *data = malloc_faces(4, length, sizeof(GLfloat));
    for (int i = 0; i < length; i++) {
        make_character(data + i * 24, x, y, n / 2, n, text[i]);
        x += n;
    }
    return gen_faces(4, length, data, sizeof(GLfloat));
}

GLuint gen_mouse_cursor_buffer(float x, float y, int p)
{
    GLfloat *data = malloc_faces(4, 1, sizeof(GLfloat));
    make_mouse_cursor(data, x, y, p);
    return gen_faces(4, 1, data, sizeof(GLfloat));
}

void draw_triangles_3d_ao(Attrib *attrib, GLuint buffer, int count,
                          size_t type_size, int gl_type)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->normal);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, gl_type, GL_FALSE,
        type_size * 10, 0);
    glVertexAttribPointer(attrib->normal, 3, gl_type, GL_FALSE,
        type_size * 10, (GLvoid *)(type_size * 3));
    glVertexAttribPointer(attrib->uv, 4, gl_type, GL_FALSE,
        type_size * 10, (GLvoid *)(type_size * 6));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->normal);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_triangles_3d_text(Attrib *attrib, GLuint buffer, int count)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->uv);
    glEnableVertexAttribArray(attrib->color);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 9, 0);
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 9, (GLvoid *)(sizeof(GLfloat) * 3));
    glVertexAttribPointer(attrib->color, 4, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 9, (GLvoid *)(sizeof(GLfloat) * 5));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->uv);
    glDisableVertexAttribArray(attrib->color);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_triangles_3d_sky(Attrib *attrib, GLuint buffer, int count, int gl_float_type, size_t float_size)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, gl_float_type, GL_FALSE,
        float_size * 8, 0);
    glVertexAttribPointer(attrib->uv, 2, gl_float_type, GL_FALSE,
        float_size * 8, (GLvoid *)(float_size * 6));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_triangles_2d(Attrib *attrib, GLuint buffer, int count)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 2, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 4, 0);
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 4, (GLvoid *)(sizeof(GLfloat) * 2));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_lines(Attrib *attrib, GLuint buffer, int components, int count)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glVertexAttribPointer(
        attrib->position, components, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_LINES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_chunk(Attrib *attrib, Chunk *chunk, int gl_float_type, size_t float_size)
{
    draw_triangles_3d_ao(attrib, chunk->buffer, chunk->faces * 6,
                         float_size, gl_float_type);
}

void draw_item(Attrib *attrib, GLuint buffer, int count, size_t type_size,
               int gl_type)
{
    draw_triangles_3d_ao(attrib, buffer, count, type_size, gl_type);
}

void draw_text(Attrib *attrib, GLuint buffer, int length) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_triangles_2d(attrib, buffer, length * 6);
    glDisable(GL_BLEND);
}

void draw_signs(Attrib *attrib, Chunk *chunk)
{
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0, -2.0);
    draw_triangles_3d_text(attrib, chunk->sign_buffer, chunk->sign_faces * 6);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void draw_sign(Attrib *attrib, GLuint buffer, int length)
{
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-8, -1024);
    draw_triangles_3d_text(attrib, buffer, length * 6);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void draw_cube(Attrib *attrib, GLuint buffer, size_t float_size, int gl_float_type)
{
    draw_item(attrib, buffer, 36, float_size, gl_float_type);
}

void draw_plant(Attrib *attrib, GLuint buffer, size_t float_size, int gl_float_type)
{
    draw_item(attrib, buffer, 24, float_size, gl_float_type);
}

void draw_player(Attrib *attrib, Player *player)
{
    draw_cube(attrib, player->buffer, sizeof(GLfloat), GL_FLOAT);
}

void draw_mouse(Attrib *attrib, GLuint buffer)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_triangles_2d(attrib, buffer, 6);
    glDisable(GL_BLEND);
}

int render_chunks(void)
{
    int result = 0;
    State *s = rs.player_state;
    int p = chunked(s->x);
    int q = chunked(s->z);
    float light = get_daylight();
    float matrix[16];
    set_matrix_3d(
        matrix, rs.width, rs.height,
        s->x, s->y, s->z, s->rx, s->ry, rs.fov, rs.ortho, rs.render_radius);
    float planes[6][4];
    frustum_planes(planes, rs.render_radius, matrix);
    glUseProgram(block_attrib.program);
    glUniformMatrix4fv(block_attrib.matrix, 1, GL_FALSE, matrix);
    glUniform3f(block_attrib.camera, s->x, s->y, s->z);
    glUniform1i(block_attrib.sampler, 0);
    glUniform1i(block_attrib.extra1, 2);
    glUniform1f(block_attrib.extra2, light);
    glUniform1f(block_attrib.extra3, rs.render_radius * CHUNK_SIZE);
    glUniform1i(block_attrib.extra4, rs.ortho);
    glUniform1f(block_attrib.timer, time_of_day());
    for (int i = 0; i < chunk_count; i++) {
        Chunk *chunk = chunks + i;
        if (chunk_distance(chunk, p, q) > rs.render_radius) {
            continue;
        }
        if (!chunk_visible(
            planes, chunk->p, chunk->q, chunk->miny, chunk->maxy, rs.ortho))
        {
            continue;
        }
        glUniform4f(block_attrib.map, chunk->map.dx, chunk->map.dy, chunk->map.dz, 0);
        draw_chunk(&block_attrib, chunk, rs.gl_float_type, rs.float_size);
        result += chunk->faces;
    }
    return result;
}

void render_signs(void)
{
    State *s = rs.player_state;
    int p = chunked(s->x);
    int q = chunked(s->z);
    float matrix[16];
    set_matrix_3d(
        matrix, rs.width, rs.height,
        s->x, s->y, s->z, s->rx, s->ry, rs.fov, rs.ortho, rs.render_radius);
    float planes[6][4];
    frustum_planes(planes, rs.render_radius, matrix);
    glUseProgram(text_attrib.program);
    glUniformMatrix4fv(text_attrib.matrix, 1, GL_FALSE, matrix);
    glUniform3f(text_attrib.camera, s->x, s->y, s->z);
    glUniform1i(text_attrib.sampler, 3);
    glUniform1i(text_attrib.extra1, 1);  // is_sign
    glUniform1i(text_attrib.extra2, 2);  // sky_sampler
    glUniform1f(text_attrib.extra3, rs.render_radius * CHUNK_SIZE); // fog_distance
    glUniform1i(text_attrib.extra4, rs.ortho);  // ortho
    glUniform1f(text_attrib.timer, time_of_day());
    for (int i = 0; i < chunk_count; i++) {
        Chunk *chunk = chunks + i;
        if (chunk_distance(chunk, p, q) > rs.sign_radius) {
            continue;
        }
        if (!chunk_visible(
            planes, chunk->p, chunk->q, chunk->miny, chunk->maxy, rs.ortho))
        {
            continue;
        }
        draw_signs(&text_attrib, chunk);
    }
}

void render_sign(char *typing_buffer, int x, int y, int z, int face, float y_face_height)
{
    State *s = rs.player_state;
    float matrix[16];
    set_matrix_3d(
        matrix, rs.width, rs.height,
        s->x, s->y, s->z, s->rx, s->ry, rs.fov, rs.ortho, rs.render_radius);
    glUseProgram(text_attrib.program);
    glUniformMatrix4fv(text_attrib.matrix, 1, GL_FALSE, matrix);
    glUniform1i(text_attrib.sampler, 3);
    glUniform1i(text_attrib.extra1, 1);
    char text[MAX_SIGN_LENGTH];
    strncpy(text, typing_buffer + 1, MAX_SIGN_LENGTH);
    text[MAX_SIGN_LENGTH - 1] = '\0';
    GLfloat *data = malloc_faces_with_rgba(5, strlen(text));
    int length = gen_sign_buffer(data, x, y, z, face, text, y_face_height);
    GLuint buffer = gen_faces_with_rgba(5, length, data);
    draw_sign(&text_attrib, buffer, length);
    del_buffer(buffer);
}

void render_players(Player *player)
{
    State *s = rs.player_state;
    float matrix[16];
    set_matrix_3d(
        matrix, rs.width, rs.height,
        s->x, s->y, s->z, s->rx, s->ry, rs.fov, rs.ortho, rs.render_radius);
    glUseProgram(block_attrib.program);
    glUniformMatrix4fv(block_attrib.matrix, 1, GL_FALSE, matrix);
    glUniform3f(block_attrib.camera, s->x, s->y, s->z);
    glUniform1i(block_attrib.sampler, 0);
    glUniform1f(block_attrib.timer, time_of_day());
    for (int i = 0; i < client_count; i++) {
        Client *client = clients + i;
        for (int j = 0; j < MAX_LOCAL_PLAYERS; j++) {
            Player *other = client->players + j;
            if (other != player && other->is_active) {
                glUniform4f(block_attrib.map, 0, 0, 0, 0);
                draw_player(&block_attrib, other);
            }
        }
    }
}

void render_sky(void)
{
    State *s = rs.player_state;
    float matrix[16];
    set_matrix_3d(
        matrix, rs.width, rs.height,
        0, 0, 0, s->rx, s->ry, rs.fov, 0, rs.render_radius);
    glUseProgram(sky_attrib.program);
    glUniformMatrix4fv(sky_attrib.matrix, 1, GL_FALSE, matrix);
    glUniform1i(sky_attrib.sampler, 2);
    glUniform1f(sky_attrib.timer, time_of_day());
    draw_triangles_3d_sky(&sky_attrib, sky_buffer, 128 * 3, rs.gl_float_type, rs.float_size);
}

void render_wireframe(int hx, int hy, int hz, const float color[4], float item_height)
{
    State *s = rs.player_state;
    float matrix[16];
    set_matrix_3d(
        matrix, rs.width, rs.height,
        s->x, s->y, s->z, s->rx, s->ry, rs.fov, rs.ortho, rs.render_radius);
    glUseProgram(line_attrib.program);
    glLineWidth(1);
    glUniformMatrix4fv(line_attrib.matrix, 1, GL_FALSE, matrix);
    glUniform4fv(line_attrib.extra1, 1, color);
    float h = item_height - 0.97;
    GLuint wireframe_buffer = gen_wireframe_buffer(hx, hy, hz, 0.53, h);
    draw_lines(&line_attrib, wireframe_buffer, 3, 24);
    del_buffer(wireframe_buffer);
}

void render_crosshairs(const float color[4])
{
    float matrix[16];
    set_matrix_2d(matrix, rs.width, rs.height);
    glUseProgram(line_attrib.program);
    glLineWidth(4 * rs.scale);
    glUniformMatrix4fv(line_attrib.matrix, 1, GL_FALSE, matrix);
    glUniform4fv(line_attrib.extra1, 1, color);
    GLuint crosshair_buffer = gen_crosshair_buffer();
    draw_lines(&line_attrib, crosshair_buffer, 2, 4);
    del_buffer(crosshair_buffer);
}

void render_item(int w)
{
    float matrix[16];
    set_matrix_item(matrix, rs.width, rs.height, rs.scale);
    glUseProgram(block_attrib.program);
    glUniformMatrix4fv(block_attrib.matrix, 1, GL_FALSE, matrix);
    glUniform3f(block_attrib.camera, 0, 0, 5);
    glUniform1i(block_attrib.sampler, 0);
    glUniform1f(block_attrib.timer, time_of_day());
    glUniform4f(block_attrib.map, 0, 0, 0, 0);
    if (is_plant(w)) {
        GLuint buffer = gen_plant_buffer(0, 0, 0, 0.5, w);
        draw_plant(&block_attrib, buffer, rs.float_size, rs.gl_float_type);
        del_buffer(buffer);
    }
    else {
        GLuint buffer = gen_cube_buffer(0, 0, 0, 0.5, w);
        draw_cube(&block_attrib, buffer, rs.float_size, rs.gl_float_type);
        del_buffer(buffer);
    }
}

void render_text_rgba(
    int justify, float x, float y, float n, char *text,
    const float *background, const float *text_color)
{
    float matrix[16];
    set_matrix_2d(matrix, rs.width, rs.height);
    glUseProgram(text_attrib.program);
    glUniformMatrix4fv(text_attrib.matrix, 1, GL_FALSE, matrix);
    glUniform1i(text_attrib.sampler, 3);
    glUniform1i(text_attrib.extra1, 0);
    glUniform4fv(text_attrib.extra5, 1, background);
    glUniform4fv(text_attrib.extra6, 1, text_color);
    int length = strlen(text);
    x -= n * justify * (length - 1) / 2;
    GLuint buffer = gen_text_buffer(x, y, n, text);
    draw_text(&text_attrib, buffer, length);
    del_buffer(buffer);
}

void render_text(
    int justify, float x, float y, float n, char *text)
{
    render_text_rgba(justify, x, y, n, text, hud_text_background,
                     hud_text_color);
}

GLuint gen_text_cursor_buffer(float x, float y, float scale)
{
    int p = 10 * scale;
    float data[] = {
        x, y - p, x, y + p,
    };
    return gen_buffer(sizeof(data), data);
}

void render_text_cursor(float x, float y)
{
    float matrix[16];
    set_matrix_2d(matrix, rs.width, rs.height);
    glUseProgram(line_attrib.program);
    glLineWidth(2 * rs.scale);
    glUniformMatrix4fv(line_attrib.matrix, 1, GL_FALSE, matrix);
    glUniform4fv(line_attrib.extra1, 1, GREEN);
    GLuint text_cursor_buffer = gen_text_cursor_buffer(x, y, rs.scale);
    draw_lines(&line_attrib, text_cursor_buffer, 2, 2);
    del_buffer(text_cursor_buffer);
}

void render_mouse_cursor(float x, float y, int p)
{
    float matrix[16];
    set_matrix_2d(matrix, rs.width, rs.height);
    glUseProgram(mouse_attrib.program);
    glUniformMatrix4fv(mouse_attrib.matrix, 1, GL_FALSE, matrix);
    glUniform1i(mouse_attrib.sampler, 0);
    GLuint mouse_cursor_buffer = gen_mouse_cursor_buffer(x, y, p);
    draw_mouse(&mouse_attrib, mouse_cursor_buffer);
    del_buffer(mouse_cursor_buffer);
}

