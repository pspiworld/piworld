#pragma once

#include <GLES2/gl2.h>
#include "util.h"

typedef struct {
    float x;
    float y;
    float z;
    float rx;
    float ry;
    float t;
} State;

typedef struct Player {
    char name[MAX_NAME_LENGTH];
    int id;  // 1...MAX_LOCAL_PLAYERS
    State state;
    State state1;
    State state2;
    GLuint buffer;
    int texture_index;
    int is_active;
} Player;

typedef struct {
    int id;
    Player players[MAX_LOCAL_PLAYERS];
} Client;

