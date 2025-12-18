#ifndef ADS_SEARCH_H
#define ADS_SEARCH_H

#include "ads_defines.h"

MV MY_xDiamondSearchOpt(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init);
MV MY_xDiamondSearchADS(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init);
MV MY_full_search_motion_estimation(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init);

unsigned int sad_block(const Frame* ref, const Frame* cur, int rx, int ry, int bx, int by, int bw, int bh);
#endif