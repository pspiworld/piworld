#pragma once

#define BEDROCK COLOR_11  // A Raspberry base

typedef void (*world_func)(int, int, int, int, void *);

void create_world(int p, int q, world_func func, void *arg);

