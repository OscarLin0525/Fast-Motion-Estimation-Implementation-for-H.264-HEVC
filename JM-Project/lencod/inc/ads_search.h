#pragma once

#include "defines.h"

MV xDiamondSearchOpt(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init);
MV xDiamondSearchADS(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init);
MV full_search_motion_estimation(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init);

unsigned int sad_block(const Frame* ref, const Frame* cur, int rx, int ry, int bx, int by, int bw, int bh);
