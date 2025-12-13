#pragma once

#include "defines.h"

// Adaptive Diamond Search entry
// ref: reference frame; cur: current frame
// bx, by: block top-left in current frame
// init: initial MV prediction
MV xDiamondSearch(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init);

// Full Search (exhaustive) for baseline comparison
MV xFullSearch(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init);

// Utility: compute SAD of a block at (rx,ry) in ref vs (bx,by) in cur
unsigned int sad_block(const Frame* ref, const Frame* cur, int rx, int ry, int bx, int by, int bw, int bh);
