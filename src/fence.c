
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "chunks.h"
#include "cube.h"
#include "door.h"
#include "fence.h"
#include "item.h"
#include "matrix.h"
#include "pw.h"
#include "util.h"

#define POST {{P06, P00, P06}, {P10, P16, P10}}
#define SLAT1 {{P07, P04, P10}, {P09, P06, P16}}
#define SLAT2 {{P07, P08, P10}, {P09, P10, P16}}
#define SLAT3 {{P07, P12, P10}, {P09, P14, P16}}
#define SLAT4 {{P07, P04, P00}, {P09, P06, P06}}
#define SLAT5 {{P07, P08, P00}, {P09, P10, P06}}
#define SLAT6 {{P07, P12, P00}, {P09, P14, P06}}
#define SLATX1 {{P00, P04, P07}, {P07, P06, P09}}
#define SLATX2 {{P00, P08, P07}, {P07, P10, P09}}
#define SLATX3 {{P00, P12, P07}, {P07, P14, P09}}
#define SLATX4 {{P10, P04, P07}, {P16, P06, P09}}
#define SLATX5 {{P10, P08, P07}, {P16, P10, P09}}
#define SLATX6 {{P10, P12, P07}, {P16, P14, P09}}
#define GATEPOST1 {{P07, P00, P00}, {P09, P16, P02}}
#define GATEPOST2 {{P07, P00, P14}, {P09, P16, P16}}
#define GATESLAT1 {{P07, P04, P10}, {P09, P06, P14}}
#define GATESLAT2 {{P07, P08, P10}, {P09, P10, P14}}
#define GATESLAT3 {{P07, P12, P10}, {P09, P14, P14}}
#define GATESLAT4 {{P07, P04, P02}, {P09, P06, P06}}
#define GATESLAT5 {{P07, P08, P02}, {P09, P10, P06}}
#define GATESLAT6 {{P07, P12, P02}, {P09, P14, P06}}
#define GATEMID1 {{P07, P05, P09}, {P09, P13, P10}}
#define GATEMID2 {{P07, P05, P06}, {P09, P13, P07}}
#define GATEPOSTX1 {{P02, P00, P09}, {P00, P16, P07}}
#define GATEPOSTX2 {{P14, P00, P07}, {P16, P16, P09}}
#define GATESLATX1 {{P02, P04, P07}, {P07, P06, P09}}
#define GATESLATX2 {{P02, P08, P07}, {P07, P10, P09}}
#define GATESLATX3 {{P02, P12, P07}, {P07, P14, P09}}
#define GATESLATX4 {{P10, P04, P07}, {P14, P06, P09}}
#define GATESLATX5 {{P10, P08, P07}, {P14, P10, P09}}
#define GATESLATX6 {{P10, P12, P07}, {P14, P14, P09}}
#define GATEMIDX1 {{P09, P05, P07}, {P10, P13, P09}}
#define GATEMIDX2 {{P07, P05, P07}, {P08, P13, P09}}

#define GATESLAT1OPEN {{P02, P04, P00}, {P07, P06, P02}}
#define GATESLAT2OPEN {{P02, P08, P00}, {P07, P10, P02}}
#define GATESLAT3OPEN {{P02, P12, P00}, {P07, P14, P02}}
#define GATESLAT4OPEN {{P02, P04, P14}, {P07, P06, P16}}
#define GATESLAT5OPEN {{P02, P08, P14}, {P07, P10, P16}}
#define GATESLAT6OPEN {{P02, P12, P14}, {P07, P14, P16}}
#define GATEMID1OPEN {{P01, P05, P00}, {P02, P13, P02}}
#define GATEMID2OPEN {{P01, P05, P14}, {P02, P13, P16}}

#define GATESLATX1OPEN {{P00, P04, P02}, {P02, P06, P07}}
#define GATESLATX2OPEN {{P00, P08, P02}, {P02, P10, P07}}
#define GATESLATX3OPEN {{P00, P12, P02}, {P02, P14, P07}}
#define GATESLATX4OPEN {{P14, P04, P02}, {P16, P06, P07}}
#define GATESLATX5OPEN {{P14, P08, P02}, {P16, P10, P07}}
#define GATESLATX6OPEN {{P14, P12, P02}, {P16, P14, P07}}
#define GATEMIDX1OPEN {{P00, P05, P01}, {P02, P13, P02}}
#define GATEMIDX2OPEN {{P14, P05, P01}, {P16, P13, P02}}

// cuboids:
// [transformations][cuboids][corners][xyz]
float fence_post_cubiods[1][1][2][3] = {
    {POST},
};
// faces:
// [transformations][cuboids][faces][corners][xyz]
float fence_post_cuboid_faces[1][4][6][4][3] = { };

// 4 transformations of a fence half
// 4 cuboids:
//   1 post
//   3 fence slats
float fence_half_cubiods[4][4][2][3] = {
    {POST, SLAT1, SLAT2, SLAT3},
    {POST, SLAT4, SLAT5, SLAT6},
    {POST, SLATX1, SLATX2, SLATX3},
    {POST, SLATX4, SLATX5, SLATX6},
};
float fence_half_cuboid_faces[4][4][6][4][3] = { };

// 2 transformations of a fence
// 7 cuboids:
//   1 post
//   6 fence slats
float fence_cubiods[2][7][2][3] = {
    {POST, SLAT1, SLAT2, SLAT3, SLAT4, SLAT5, SLAT6},
    {POST, SLATX1, SLATX2, SLATX3, SLATX4, SLATX5, SLATX6},
};
float fence_cuboid_faces[2][7][6][4][3] = { };

// 4 transformations of fence L
// 7 cuboids:
//   1 post
//   6 fence slats
float fence_L_cubiods[4][7][2][3] = {
    {POST, SLAT1, SLAT2, SLAT3, SLATX1, SLATX2, SLATX3},
    {POST, SLATX1, SLATX2, SLATX3, SLAT4, SLAT5, SLAT6},
    {POST, SLAT1, SLAT2, SLAT3, SLATX4, SLATX5, SLATX6},
    {POST, SLAT4, SLAT5, SLAT6, SLATX4, SLATX5, SLATX6},
};
float fence_L_cuboid_faces[4][7][6][4][3] = { };

// 4 transformations of fence T
// 10 cuboids:
//   1 post
//   9 fence slats
float fence_T_cubiods[4][10][2][3] = {
    {POST, SLAT1, SLAT2, SLAT3, SLATX1, SLATX2, SLATX3, SLAT4, SLAT5, SLAT6},
    {POST, SLATX1, SLATX2, SLATX3, SLAT1, SLAT2, SLAT3, SLATX4, SLATX5, SLATX6},
    {POST, SLAT1, SLAT2, SLAT3, SLAT4, SLAT5, SLAT6, SLATX4, SLATX5, SLATX6},
    {POST, SLAT4, SLAT5, SLAT6, SLATX4, SLATX5, SLATX6, SLATX1, SLATX2, SLATX3},
};
float fence_T_cuboid_faces[4][10][6][4][3] = { };

// 1 transformation of fence X
// 13 cuboids:
//   1 post
//   12 fence slats
float fence_X_cubiods[1][13][2][3] = {
    {POST, SLAT1, SLAT2, SLAT3, SLAT4, SLAT5, SLAT6,
     SLATX1, SLATX2, SLATX3, SLATX4, SLATX5, SLATX6},
};
float fence_X_cuboid_faces[1][13][6][4][3] = { };

// 4 tranformations of gate
// 10 cuboids:
//   2 posts
//   6 fence slats
//   2 vertical slats
float gate_cubiods[4][10][2][3] = {
    {GATEPOST1, GATEPOST2, GATESLAT1, GATESLAT2, GATESLAT3,
     GATESLAT4, GATESLAT5, GATESLAT6, GATEMID1, GATEMID2},
    {GATEPOSTX1, GATEPOSTX2, GATESLATX1, GATESLATX2, GATESLATX3,
     GATESLATX4, GATESLATX5, GATESLATX6, GATEMIDX1, GATEMIDX2},
    {GATEPOST1, GATEPOST2, GATESLAT1OPEN, GATESLAT2OPEN, GATESLAT3OPEN,
     GATESLAT4OPEN, GATESLAT5OPEN, GATESLAT6OPEN, GATEMID1OPEN, GATEMID2OPEN},
    {GATEPOSTX1, GATEPOSTX2, GATESLATX1OPEN, GATESLATX2OPEN, GATESLATX3OPEN,
     GATESLATX4OPEN, GATESLATX5OPEN, GATESLATX6OPEN, GATEMIDX1OPEN, GATEMIDX2OPEN},
};
float gate_cuboid_faces[4][10][6][4][3] = { };

#define FENCE_SHAPE_COUNT 7
struct FenceShape {
    int transform_count;
    int cuboid_count;
    float *cuboids;
    float *faces;
    int face_count;
};
struct FenceShape fence_shapes[FENCE_SHAPE_COUNT];

int ShapeMap[] = {
    FENCE_POST, FENCE_HALF, FENCE, FENCE_L, FENCE_T, FENCE_X, GATE
};

void cuboid_corners_to_faces(float *corners, float *faces)
{
    // Input: an array of 2*3 floats describing a cube in 2 corners
    // Output: an array of 6*4*3 floats describing a cube in 6 faces
    static const int map_cuboid_to_face_corners[6*4*3*2] = {
        0,0, 0,1, 0,2, 0,0, 0,1, 1,2, 0,0, 1,1, 0,2, 0,0, 1,1, 1,2,
        1,0, 0,1, 0,2, 1,0, 0,1, 1,2, 1,0, 1,1, 0,2, 1,0, 1,1, 1,2,
        0,0, 1,1, 0,2, 0,0, 1,1, 1,2, 1,0, 1,1, 0,2, 1,0, 1,1, 1,2,
        0,0, 0,1, 0,2, 0,0, 0,1, 1,2, 1,0, 0,1, 0,2, 1,0, 0,1, 1,2,
        0,0, 0,1, 0,2, 0,0, 1,1, 0,2, 1,0, 0,1, 0,2, 1,0, 1,1, 0,2,
        0,0, 0,1, 1,2, 0,0, 1,1, 1,2, 1,0, 0,1, 1,2, 1,0, 1,1, 1,2,
    };
    int cube_corner = 0;
    for (int face=0; face<6; face++) {
        for (int face_corner=0; face_corner<4; face_corner++) {
            *(faces + (2*face*6 + face_corner*3)) = 
                *(corners + map_cuboid_to_face_corners[cube_corner]*3 +
                            map_cuboid_to_face_corners[cube_corner+1]);
            *(faces + (2*face*6 + face_corner*3) + 1) = 
                *(corners + map_cuboid_to_face_corners[cube_corner+2]*3 +
                            map_cuboid_to_face_corners[cube_corner+3]);
            *(faces + (2*face*6 + face_corner*3) + 2) = 
                *(corners + map_cuboid_to_face_corners[cube_corner+4]*3 +
                            map_cuboid_to_face_corners[cube_corner+5]);
            cube_corner += 6;
        }
    }
}

static struct FenceShape *get_fence_shape(int shape)
{
    struct FenceShape *s = NULL;
    for (int i=0; i<FENCE_SHAPE_COUNT; i++) {
        if (ShapeMap[i] == shape) {
            s = &fence_shapes[i];
            break;
        }
    }
    return s;
}

//void fence_shape_init(int shape_id, float *cuboids, float *faces,
void fence_shape_init(int shape_id, void *vcuboids, void *vfaces,
    int transform_count, int cuboid_count)
{
    struct FenceShape *shape;
    float *cuboids = vcuboids;
    float *faces = vfaces;
    shape = get_fence_shape(shape_id);
    shape->transform_count = transform_count;
    shape->cuboid_count = cuboid_count;
    shape->cuboids = cuboids;
    shape->faces = faces;
    for (int i=0; i<transform_count; i++) {
        for (int j=0; j<cuboid_count; j++) {
            cuboid_corners_to_faces(
                (cuboids + ((i * cuboid_count * 2 * 3) + (j * 2 * 3))),
                (faces + ((i * cuboid_count *6 * 4 * 3) + (j * 6 * 4 * 3))));
        }
    }
    shape->face_count = cuboid_count * 6;
}

void fence_init(void)
{
    fence_shape_init(
        FENCE_POST, fence_post_cubiods, fence_post_cuboid_faces, 1, 1);
    fence_shape_init(
        FENCE_HALF, fence_half_cubiods, fence_half_cuboid_faces, 4, 4);
    fence_shape_init(
        FENCE, fence_cubiods, fence_cuboid_faces, 2, 7);
    fence_shape_init(
        FENCE_L, fence_L_cubiods, fence_L_cuboid_faces, 4, 7);
    fence_shape_init(
        FENCE_T, fence_T_cubiods, fence_T_cuboid_faces, 4, 10);
    fence_shape_init(
        FENCE_X, fence_X_cubiods, fence_X_cuboid_faces, 1, 13);
    fence_shape_init(
        GATE, gate_cubiods, gate_cuboid_faces, 4, 10);
}

int fence_face_count(int shape_id)
{
    struct FenceShape *shape;
    shape = get_fence_shape(shape_id);
    return shape->face_count;
}

void make_cuboid_faces(
    float *data, float ao[6][4], float light[6][4],
    int left, int right, int top, int bottom, int front, int back,
    int wleft, int wright, int wtop, int wbottom, int wfront, int wback,
    float x, float y, float z, float n, float cuboid_positions[6][4][3])
{
    float height1 = 0.0625;

    static const float normals[6][3] = {
        {-1, 0, 0},
        {+1, 0, 0},
        {0, +1, 0},
        {0, -1, 0},
        {0, 0, -1},
        {0, 0, +1}
    };
    static const float uvs[6][4][2] =
    {
        {{0, 0}, {1, 0}, {0, 0.5}, {1, 0.5}},
        {{1, 0}, {0, 0}, {1, 0.5}, {0, 0.5}},
        {{0, 1}, {0, 0}, {1, 1}, {1, 0}},
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
        {{0, 0}, {0, 0.5}, {1, 0}, {1, 0.5}},
        {{1, 0}, {1, 0.5}, {0, 0}, {0, 0.5}}
    };

    static const float indices[6][6] = {
        {0, 3, 2, 0, 1, 3},
        {0, 3, 1, 0, 2, 3},
        {0, 3, 2, 0, 1, 3},
        {0, 3, 1, 0, 2, 3},
        {0, 3, 2, 0, 1, 3},
        {0, 3, 1, 0, 2, 3}
    };
    static const float flipped[6][6] = {
        {0, 1, 2, 1, 3, 2},
        {0, 2, 1, 2, 3, 1},
        {0, 1, 2, 1, 3, 2},
        {0, 2, 1, 2, 3, 1},
        {0, 1, 2, 1, 3, 2},
        {0, 2, 1, 2, 3, 1}
    };
    float *d = data;
    float s = 0.0625;
    float a = 0 + 1 / 2048.0;
    float b = s - 1 / 2048.0;
    int faces[6] = {left, right, top, bottom, front, back};
    int tiles[6] = {wleft, wright, wtop, wbottom, wfront, wback};
    for (int i = 0; i < 6; i++) {
        if (faces[i] == 0) {
            continue;
        }
        float du = (tiles[i] % 16) * s;
        float dv = (tiles[i] / 16) * s;
        int flip = ao[i][0] + ao[i][3] > ao[i][1] + ao[i][2];
        for (int v = 0; v < 6; v++) {
            int j = flip ? flipped[i][v] : indices[i][v];
            *(d++) = x + n * cuboid_positions[i][j][0];
            *(d++) = y + n * cuboid_positions[i][j][1];
            *(d++) = z + n * cuboid_positions[i][j][2];
            *(d++) = normals[i][0];
            *(d++) = normals[i][1];
            *(d++) = normals[i][2];
            *(d++) = du + (uvs[i][j][0] ? b : a);
            if (uvs[i][j][1] == 1) {
                *(d++) = dv + b;
            } else if (uvs[i][j][1] == 0) {
                *(d++) = dv + a;
            } else if (uvs[i][j][1] == 0.5) {
                *(d++) = dv + b - (a*(128 - (128*height1)));
            }
            *(d++) = ao[i][j];
            *(d++) = light[i][j];
        }
    }
}

void make_cuboid(
    float *data, float ao[6][4], float light[6][4],
    int left, int right, int top, int bottom, int front, int back,
    float x, float y, float z, float n, int w,
    float cuboid_positions[6][4][3])
{
    int texture_id = w;
    int wleft = blocks[texture_id][0];
    int wright = blocks[texture_id][1];
    int wtop = blocks[texture_id][2];
    int wbottom = blocks[texture_id][3];
    int wfront = blocks[texture_id][4];
    int wback = blocks[texture_id][5];
    make_cuboid_faces(
        data, ao, light,
        left, right, top, bottom, front, back,
        wleft, wright, wtop, wbottom, wfront, wback,
        x, y, z, n, cuboid_positions);
}

void make_fence(
    float *data, float ao[6][4], float light[6][4],
    int left, int right, int top, int bottom, int front, int back,
    float x, float y, float z, float n, int w, int shape, int extra,
    int rotate)
{
    struct FenceShape *s = get_fence_shape(shape);

    int transform = MIN(rotate, s->transform_count - 1);

    if (shape == GATE && is_open(extra)) {
        transform += 2;
    }
    
    for (int i=0; i<s->cuboid_count; i++) {
        make_cuboid(data + i*6*60, ao, light,
            left, right, top, bottom, front, back,
            x, y, z, n, w,
            (s->faces + ((transform * s->cuboid_count *6*4*3)) + (i *6*4*3)));
    }
}

void make_gate_in_buffer_sub_data(int buffer, int float_size,
    DoorMapEntry *gate)
{
    // This is an optimisation to change the shape of just one gate block.
    GLfloat gate_data[10*6*10*6];  // 10 * 6 * components * faces
    make_fence(
        gate_data, gate->ao, gate->light, gate->left, gate->right, gate->top,
        gate->bottom, gate->front, gate->back, gate->e.x, gate->e.y, gate->e.z,
        gate->n, gate->e.w, gate->shape, gate->extra, gate->transform);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    if (config->use_hfloat) {
        hfloat hdata[10*6*10*6];
        for (size_t i=0; i<(10 * 6 * 10 * 6); i++) {
            hdata[i] = float_to_hfloat(gate_data + i);
        }
        glBufferSubData(GL_ARRAY_BUFFER,
            gate->offset_into_gl_buffer * float_size,
            6*10*gate->face_count_in_gl_buffer*float_size, hdata);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER,
            gate->offset_into_gl_buffer * float_size,
            6*10*gate->face_count_in_gl_buffer*float_size, gate_data);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void _gate_toggle_open(DoorMapEntry *gate, int x, int y, int z, GLuint buffer)
{
    if (is_open(gate->extra)) {
        gate->extra &= ~EXTRA_BIT_OPEN;
    } else {
        gate->extra |= EXTRA_BIT_OPEN;
    }
    set_extra_non_dirty(x, y, z, gate->extra);
    make_gate_in_buffer_sub_data(buffer, get_float_size(), gate);
}

void gate_toggle_open(DoorMapEntry *gate, int x, int y,
    int z, GLuint buffer)
{
    _gate_toggle_open(gate, x, y, z, buffer);
}

