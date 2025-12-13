#pragma once

#include "global.h"

// Simple ADS motion structures (integer-pel)
typedef struct {
    int x;
    int y;
} ADSMV;

typedef struct {
    int block_w;
    int block_h;
    int search_range;
    int max_iters;
} ADSParams;

// Adaptive Diamond Search on imgpel** buffers (integer-pel domain)
ADSMV xDiamondSearchADS(imgpel** ref, imgpel** cur, int width, int height, int bx, int by, ADSParams params, ADSMV init);

// SAD utility on imgpel** buffers
unsigned int sad_block_ads(imgpel** ref, imgpel** cur, int width, int height, int rx, int ry, int bx, int by, int bw, int bh);
