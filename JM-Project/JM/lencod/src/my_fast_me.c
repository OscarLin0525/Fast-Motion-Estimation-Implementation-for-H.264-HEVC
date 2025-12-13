#include <limits.h>
#include <stdlib.h>
#include "my_fast_me.h"

static unsigned int sad_point_ads(imgpel** ref, imgpel** cur, int width, int height, int cx, int cy, int bx, int by, int bw, int bh) {
    return sad_block_ads(ref, cur, width, height, cx, cy, bx, by, bw, bh);
}

unsigned int sad_block_ads(imgpel** ref, imgpel** cur, int width, int height, int rx, int ry, int bx, int by, int bw, int bh) {
    rx = iClip3(0, width  - bw, rx);
    ry = iClip3(0, height - bh, ry);

    unsigned int sad = 0;
    for (int y = 0; y < bh; ++y) {
        int ref_y = ry + y;
        int cur_y = by + y;
        for (int x = 0; x < bw; ++x) {
            int ref_x = rx + x;
            int cur_x = bx + x;
            sad += (unsigned int)abs((int)ref[ref_y][ref_x] - (int)cur[cur_y][cur_x]);
        }
    }
    return sad;
}

ADSMV xDiamondSearchADS(imgpel** ref, imgpel** cur, int width, int height, int bx, int by, ADSParams params, ADSMV init) {
    int bw = params.block_w;
    int bh = params.block_h;
    int range = params.search_range;
    int max_iters = params.max_iters > 0 ? params.max_iters : 64;

    int cx = bx + init.x;
    int cy = by + init.y;
    cx = iClip3(0, width  - bw, cx);
    cy = iClip3(0, height - bh, cy);

    unsigned int best_sad = sad_point_ads(ref, cur, width, height, cx, cy, bx, by, bw, bh);
    ADSMV best_mv = (ADSMV){ cx - bx, cy - by };

    const int ldsp_offsets[8][2] = {
        {  0, -2 }, {  2,  0 }, {  0,  2 }, { -2,  0 },
        {  2, -2 }, {  2,  2 }, { -2,  2 }, { -2, -2 }
    };
    const int sdsp_offsets[4][2] = {
        {  0, -1 }, {  1,  0 }, {  0,  1 }, { -1,  0 }
    };

    int effective_range = range;
    if (bw <= 8 || bh <= 8) {
        effective_range = range / 2;
        if (effective_range < 8) effective_range = 8;
        if (max_iters > 32) max_iters = 32;
    }

    int iters = 0;
    int converged_ldsp = 0;
    while (iters < max_iters) {
        ++iters;
        unsigned int center_sad = sad_point_ads(ref, cur, width, height, cx, cy, bx, by, bw, bh);
        unsigned int local_best = center_sad;
        int best_dx = 0, best_dy = 0;

        for (int i = 0; i < 8; ++i) {
            int nx = cx + ldsp_offsets[i][0];
            int ny = cy + ldsp_offsets[i][1];
            int mvx = nx - bx;
            int mvy = ny - by;
            if (mvx < -effective_range || mvx > effective_range || mvy < -effective_range || mvy > effective_range)
                continue;
            unsigned int s = sad_point_ads(ref, cur, width, height, nx, ny, bx, by, bw, bh);
            if (s < local_best) {
                local_best = s;
                best_dx = ldsp_offsets[i][0];
                best_dy = ldsp_offsets[i][1];
            }
        }

        if (local_best < center_sad) {
            cx += best_dx;
            cy += best_dy;
            cx = iClip3(0, width  - bw, cx);
            cy = iClip3(0, height - bh, cy);
            best_sad = local_best;
            best_mv.x = cx - bx;
            best_mv.y = cy - by;
            continue;
        } else {
            converged_ldsp = 1;
        }

        if (converged_ldsp) {
            unsigned int center2 = center_sad;
            unsigned int best2 = center2;
            int bdx2 = 0, bdy2 = 0;
            for (int i = 0; i < 4; ++i) {
                int nx = cx + sdsp_offsets[i][0];
                int ny = cy + sdsp_offsets[i][1];
                int mvx = nx - bx;
                int mvy = ny - by;
                if (mvx < -effective_range || mvx > effective_range || mvy < -effective_range || mvy > effective_range)
                    continue;
                unsigned int s = sad_point_ads(ref, cur, width, height, nx, ny, bx, by, bw, bh);
                if (s < best2) {
                    best2 = s;
                    bdx2 = sdsp_offsets[i][0];
                    bdy2 = sdsp_offsets[i][1];
                }
            }
            if (best2 < center2) {
                cx += bdx2;
                cy += bdy2;
                cx = iClip3(0, width  - bw, cx);
                cy = iClip3(0, height - bh, cy);
                best_sad = best2;
                best_mv.x = cx - bx;
                best_mv.y = cy - by;
            }
            break;
        }
    }

    (void)best_sad; // suppress unused warning if not consumed by caller
    return best_mv;
}
