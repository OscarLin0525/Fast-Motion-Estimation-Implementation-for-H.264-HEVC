#pragma once

#include <stdint.h>

#define CLIP3(min_v, max_v, v) ((v) < (min_v) ? (min_v) : ((v) > (max_v) ? (max_v) : (v)))

typedef struct {
    int width;
    int height;
    int stride;
    uint8_t* data; // Y-only for simplicity
} Frame;


typedef struct {
    int x;
    int y;
} MV;

typedef struct {
    int block_w;
    int block_h;
    int search_range; // +/- range
    int max_iters;     // safety cap
} MEParams;
