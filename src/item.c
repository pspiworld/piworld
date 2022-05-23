#include "item.h"
#include "util.h"

const int items[] = {
    // items the user can build
    GRASS,
    SAND,
    STONE,
    BRICK,
    WOOD,
    CEMENT,
    DIRT,
    PLANK,
    SNOW,
    GLASS,
    COBBLE,
    LIGHT_STONE,
    DARK_STONE,
    CHEST,
    LEAVES,
    TALL_GRASS,
    YELLOW_FLOWER,
    RED_FLOWER,
    PURPLE_FLOWER,
    SUN_FLOWER,
    WHITE_FLOWER,
    BLUE_FLOWER,
    COLOR_00,
    COLOR_01,
    COLOR_02,
    COLOR_03,
    COLOR_04,
    COLOR_05,
    COLOR_06,
    COLOR_07,
    COLOR_08,
    COLOR_09,
    COLOR_10,
    COLOR_11,
    COLOR_12,
    COLOR_13,
    COLOR_14,
    COLOR_15,
    COLOR_16,
    COLOR_17,
    COLOR_18,
    COLOR_19,
    COLOR_20,
    COLOR_21,
    COLOR_22,
    COLOR_23,
    COLOR_24,
    COLOR_25,
    COLOR_26,
    COLOR_27,
    COLOR_28,
    COLOR_29,
    COLOR_30,
    COLOR_31
};

const int item_count = sizeof(items) / sizeof(int);

const char *item_names[] = {
    "Grass",
    "Sand",
    "Stone",
    "Brick",
    "Wood",
    "Cement",
    "Dirt",
    "Plank",
    "Snow",
    "Glass",
    "Cobble",
    "Light Stone",
    "Dark Stone",
    "Chest",
    "Leaves",
    "Tall Grass",
    "Yellow Flower",
    "Red Flower",
    "Purple Flower",
    "Sun Flower",
    "White Flower",
    "Blue Flower",
    "Yellow",
    "Light Green",
    "Green",
    "Mid Green",
    "Dark Green",
    "Dark Brown",
    "Dark Grey",
    "Dark Violet",
    "Grey",
    "Mid Grey",
    "Violet",
    "Red",
    "Pink",
    "Magenta",
    "Green 2",
    "Brown 2",
    "Black",
    "Purple 1",
    "Purple 2",
    "Brown 3",
    "Brown 4",
    "Orange",
    "Sand 1",
    "Sand 2",
    "Blue 1",
    "Blue 2",
    "Blue 3",
    "Blue 4",
    "Blue 5",
    "White",
    "Grey 2",
    "Grey 3",
};

const int blocks[256][6] = {
    // w => (left, right, top, bottom, front, back) tiles
    {0, 0, 0, 0, 0, 0}, // 0 - empty
    {16, 16, 32, 0, 16, 16}, // 1 - grass
    {1, 1, 1, 1, 1, 1}, // 2 - sand
    {2, 2, 2, 2, 2, 2}, // 3 - stone
    {3, 3, 3, 3, 3, 3}, // 4 - brick
    {20, 20, 36, 4, 20, 20}, // 5 - wood
    {5, 5, 5, 5, 5, 5}, // 6 - cement
    {6, 6, 6, 6, 6, 6}, // 7 - dirt
    {7, 7, 7, 7, 7, 7}, // 8 - plank
    {24, 24, 40, 8, 24, 24}, // 9 - snow
    {9, 9, 9, 9, 9, 9}, // 10 - glass
    {10, 10, 10, 10, 10, 10}, // 11 - cobble
    {11, 11, 11, 11, 11, 11}, // 12 - light stone
    {12, 12, 12, 12, 12, 12}, // 13 - dark stone
    {13, 13, 13, 13, 13, 13}, // 14 - chest
    {14, 14, 14, 14, 14, 14}, // 15 - leaves
    {15, 15, 15, 15, 15, 15}, // 16 - cloud
    {0, 0, 0, 0, 0, 0}, // 17
    {0, 0, 0, 0, 0, 0}, // 18
    {0, 0, 0, 0, 0, 0}, // 19
    {0, 0, 0, 0, 0, 0}, // 20
    {0, 0, 0, 0, 0, 0}, // 21
    {0, 0, 0, 0, 0, 0}, // 22
    {0, 0, 0, 0, 0, 0}, // 23
    {0, 0, 0, 0, 0, 0}, // 24
    {0, 0, 0, 0, 0, 0}, // 25
    {0, 0, 0, 0, 0, 0}, // 26
    {0, 0, 0, 0, 0, 0}, // 27
    {0, 0, 0, 0, 0, 0}, // 28
    {0, 0, 0, 0, 0, 0}, // 29
    {0, 0, 0, 0, 0, 0}, // 30
    {0, 0, 0, 0, 0, 0}, // 31
    {176, 176, 176, 176, 176, 176}, // 32
    {177, 177, 177, 177, 177, 177}, // 33
    {178, 178, 178, 178, 178, 178}, // 34
    {179, 179, 179, 179, 179, 179}, // 35
    {180, 180, 180, 180, 180, 180}, // 36
    {181, 181, 181, 181, 181, 181}, // 37
    {182, 182, 182, 182, 182, 182}, // 38
    {183, 183, 183, 183, 183, 183}, // 39
    {184, 184, 184, 184, 184, 184}, // 40
    {185, 185, 185, 185, 185, 185}, // 41
    {186, 186, 186, 186, 186, 186}, // 42
    {187, 187, 187, 187, 187, 187}, // 43
    {188, 188, 188, 188, 188, 188}, // 44
    {189, 189, 189, 189, 189, 189}, // 45
    {190, 190, 190, 190, 190, 190}, // 46
    {191, 191, 191, 191, 191, 191}, // 47
    {192, 192, 192, 192, 192, 192}, // 48
    {193, 193, 193, 193, 193, 193}, // 49
    {194, 194, 194, 194, 194, 194}, // 50
    {195, 195, 195, 195, 195, 195}, // 51
    {196, 196, 196, 196, 196, 196}, // 52
    {197, 197, 197, 197, 197, 197}, // 53
    {198, 198, 198, 198, 198, 198}, // 54
    {199, 199, 199, 199, 199, 199}, // 55
    {200, 200, 200, 200, 200, 200}, // 56
    {201, 201, 201, 201, 201, 201}, // 57
    {202, 202, 202, 202, 202, 202}, // 58
    {203, 203, 203, 203, 203, 203}, // 59
    {204, 204, 204, 204, 204, 204}, // 60
    {205, 205, 205, 205, 205, 205}, // 61
    {206, 206, 206, 206, 206, 206}, // 62
    {207, 207, 207, 207, 207, 207}, // 63
};

const int plants[256] = {
    // w => tile
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0 - 16
    48, // 17 - tall grass
    49, // 18 - yellow flower
    50, // 19 - red flower
    51, // 20 - purple flower
    52, // 21 - sun flower
    53, // 22 - white flower
    54, // 23 - blue flower
};

const int shapes[] = {
    CUBE,
    SLAB1,
    SLAB2,
    SLAB3,
    SLAB4,
    SLAB5,
    SLAB6,
    SLAB7,
    SLAB8,
    SLAB9,
    SLAB10,
    SLAB11,
    SLAB12,
    SLAB13,
    SLAB14,
    SLAB15,
    UPPER_DOOR,
    LOWER_DOOR,
    FENCE,
    FENCE_POST,
    FENCE_HALF,
    FENCE_L,
    FENCE_T,
    FENCE_X,
    GATE,
};

const int shape_count = sizeof(shapes) / sizeof(int);

const char *shape_names[] = {
    "Cube",
    "Slab 1",
    "Slab 2",
    "Slab 3",
    "Slab 4",
    "Slab 5",
    "Slab 6",
    "Slab 7",
    "Slab 8",
    "Slab 9",
    "Slab 10",
    "Slab 11",
    "Slab 12",
    "Slab 13",
    "Slab 14",
    "Slab 15",
    "Upper Door",
    "Lower Door",
    "Fence",
    "Fence Post",
    "Fence Half",
    "Fence L",
    "Fence T",
    "Fence X",
    "Gate",
};

int is_plant(int w) {
    switch (w) {
        case TALL_GRASS:
        case YELLOW_FLOWER:
        case RED_FLOWER:
        case PURPLE_FLOWER:
        case SUN_FLOWER:
        case WHITE_FLOWER:
        case BLUE_FLOWER:
            return 1;
        default:
            return 0;
    }
}

int is_obstacle(int w, int shape, int extra) {
    w = ABS(w);
    if (is_plant(w)) {
        return 0;
    }
    shape = ABS(shape);
    if ((shape == UPPER_DOOR || shape == LOWER_DOOR || shape == GATE) &&
        is_open(extra)) {
        return 0;
    }
    switch (w) {
        case EMPTY:
        case CLOUD:
            return 0;
        default:
            return 1;
    }
}

int is_transparent(int w) {
    if (w == EMPTY) {
        return 1;
    }
    w = ABS(w);
    if (is_plant(w)) {
        return 1;
    }
    switch (w) {
        case EMPTY:
        case GLASS:
        case LEAVES:
            return 1;
        default:
            return 0;
    }
}

int is_destructable(int w) {
    switch (w) {
        case EMPTY:
        case CLOUD:
            return 0;
        default:
            return 1;
    }
}

int is_door_material(int w)
{
    if ((w >= COLOR_00 && w<= COLOR_31) || w == PLANK || w == GLASS) {
        return 1;
    }
    return 0;
}

int is_control(int w) {
    w = ABS(w);
    return (w & EXTRA_BIT_CONTROL) ? 1 : 0;
}

int is_open(int w) {
    w = ABS(w);
    return (w & EXTRA_BIT_OPEN) ? 1 : 0;
}

float item_height(int shape) {
    float height = 1.0;
    shape = ABS(shape);
    if (shape >= SLAB1 && shape <= SLAB15) {
        height = 1.0 / (16.0 / shape);
    }
    return height;
}
