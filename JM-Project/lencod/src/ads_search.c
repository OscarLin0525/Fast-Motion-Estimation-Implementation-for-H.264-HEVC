#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include "ads_search.h"

// From ads_harness.c for SAD accounting (demo harness only)
extern int g_count_mode; // 0: none, 1: FS, 2: DS
extern unsigned long long g_sad_count_fs;
extern unsigned long long g_sad_count_ds;

static unsigned int sad_point(const Frame* ref, const Frame* cur, int cx, int cy, int bx, int by, int bw, int bh) {
    return sad_block(ref, cur, cx, cy, bx, by, bw, bh);
}

unsigned int sad_block(const Frame* ref, const Frame* cur, int rx, int ry, int bx, int by, int bw, int bh) {
    unsigned int sad = 0;
    const uint8_t* r = ref->data + ry * ref->stride + rx;
    const uint8_t* c = cur->data + by * cur->stride + bx;
    for (int y = 0; y < bh; ++y) {
        for (int x = 0; x < bw; ++x) {
            sad += (unsigned int)abs((int)r[x] - (int)c[x]);
        }
        r += ref->stride;
        c += cur->stride;
    }
    if (g_count_mode == 1) {
        ++g_sad_count_fs;
    } else if (g_count_mode == 2) {
        ++g_sad_count_ds;
    }
    return sad;
}

static const int ldsp_offsets[8][2] = {
    {  0, -2 }, {  2,  0 }, {  0,  2 }, { -2,  0 },
    {  2, -2 }, {  2,  2 }, { -2,  2 }, { -2, -2 }
};
static const int sdsp_offsets[4][2] = {
    {  0, -1 }, {  1,  0 }, {  0,  1 }, { -1,  0 }
};

MV xDiamondSearchOpt(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init) {
    int bw = params.block_w;
    int bh = params.block_h;
    int range = params.search_range;
    int max_iters = params.max_iters > 0 ? params.max_iters : 64;

    unsigned int et_threshold = (unsigned int)(bw * bh / 4); // tighter early stop

    int cx = CLIP3(0, ref->width - bw, bx + init.x);
    int cy = CLIP3(0, ref->height - bh, by + init.y);
    unsigned int current_sad = sad_point(ref, cur, cx, cy, bx, by, bw, bh);
    MV best_mv = (MV){ cx - bx, cy - by };

    if (init.x != 0 || init.y != 0) {
        int zx = CLIP3(0, ref->width - bw, bx);
        int zy = CLIP3(0, ref->height - bh, by);
        unsigned int zero_sad = sad_point(ref, cur, zx, zy, bx, by, bw, bh);
        if (zero_sad < current_sad) {
            current_sad = zero_sad;
            cx = zx;
            cy = zy;
            best_mv = (MV){ 0, 0 };
        }
    }

    int effective_range = range;
    if (current_sad < et_threshold * 3) {
        effective_range = (range / 4) < 8 ? 8 : (range / 4);
        if (max_iters > 16) max_iters = 16;
    } else if (bw <= 8 || bh <= 8) {
        effective_range = (range / 2) < 8 ? 8 : (range / 2);
        if (max_iters > 32) max_iters = 32;
    }

    int min_x = CLIP3(0, ref->width - bw, bx - effective_range);
    int max_x = CLIP3(0, ref->width - bw, bx + effective_range);
    int min_y = CLIP3(0, ref->height - bh, by - effective_range);
    int max_y = CLIP3(0, ref->height - bh, by + effective_range);

    cx = CLIP3(min_x, max_x, cx);
    cy = CLIP3(min_y, max_y, cy);

    int iters = 0;
    bool use_ldsp = true;
    bool moved = false;

    while (iters < max_iters) {
        ++iters;

        unsigned int next_sad = current_sad;
        int best_dx = 0;
        int best_dy = 0;
        bool found_better = false;

        const int (*pattern)[2] = use_ldsp ? ldsp_offsets : sdsp_offsets;
        int pattern_len = use_ldsp ? 8 : 4;

        for (int i = 0; i < pattern_len; ++i) {
            int dx = pattern[i][0];
            int dy = pattern[i][1];
            int nx = cx + dx;
            int ny = cy + dy;

            if (nx < min_x || nx > max_x || ny < min_y || ny > max_y)
                continue;

            unsigned int s = sad_point(ref, cur, nx, ny, bx, by, bw, bh);
            if (s < next_sad) {
                next_sad = s;
                best_dx = dx;
                best_dy = dy;
                found_better = true;
            }
        }

        if (found_better) {
            cx += best_dx;
            cy += best_dy;
            current_sad = next_sad;
            best_mv.x = cx - bx;
            best_mv.y = cy - by;
            moved = true;
            if (moved && current_sad < et_threshold) break;
            continue;
        }

        if (use_ldsp) {
            use_ldsp = false;
            continue;
        }
        break;
    }

    return best_mv;
}

// Baseline ADS (ported from JM-style xDiamondSearchADS, minimal tweaks to fit Frame/MV)
MV xDiamondSearchADS(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init) {
    int bw = params.block_w;
    int bh = params.block_h;
    int range = params.search_range;
    int max_iters = params.max_iters > 0 ? params.max_iters : 64;

    int cx = bx + init.x;
    int cy = by + init.y;
    cx = CLIP3(0, ref->width - bw, cx);
    cy = CLIP3(0, ref->height - bh, cy);

    unsigned int best_sad = sad_point(ref, cur, cx, cy, bx, by, bw, bh);
    MV best_mv = (MV){ cx - bx, cy - by };

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
        unsigned int center_sad = sad_point(ref, cur, cx, cy, bx, by, bw, bh);
        unsigned int local_best = center_sad;
        int best_dx = 0, best_dy = 0;

        for (int i = 0; i < 8; ++i) {
            int nx = cx + ldsp_offsets[i][0];
            int ny = cy + ldsp_offsets[i][1];
            int mvx = nx - bx;
            int mvy = ny - by;
            if (mvx < -effective_range || mvx > effective_range || mvy < -effective_range || mvy > effective_range)
                continue;
            unsigned int s = sad_point(ref, cur, nx, ny, bx, by, bw, bh);
            if (s < local_best) {
                local_best = s;
                best_dx = ldsp_offsets[i][0];
                best_dy = ldsp_offsets[i][1];
            }
        }

        if (local_best < center_sad) {
            cx += best_dx;
            cy += best_dy;
            cx = CLIP3(0, ref->width - bw, cx);
            cy = CLIP3(0, ref->height - bh, cy);
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
                unsigned int s = sad_point(ref, cur, nx, ny, bx, by, bw, bh);
                if (s < best2) {
                    best2 = s;
                    bdx2 = sdsp_offsets[i][0];
                    bdy2 = sdsp_offsets[i][1];
                }
            }
            if (best2 < center2) {
                cx += bdx2;
                cy += bdy2;
                cx = CLIP3(0, ref->width - bw, cx);
                cy = CLIP3(0, ref->height - bh, cy);
                best_sad = best2;
                best_mv.x = cx - bx;
                best_mv.y = cy - by;
            }
            break;
        }
    }

    return best_mv;
}

// Baseline full search (JM-style naming for comparison)
MV full_search_motion_estimation(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init) {
    int bw = params.block_w;
    int bh = params.block_h;
    int range = params.search_range;

    int min_x = CLIP3(0, ref->width - bw, bx - range);
    int max_x = CLIP3(0, ref->width - bw, bx + range);
    int min_y = CLIP3(0, ref->height - bh, by - range);
    int max_y = CLIP3(0, ref->height - bh, by + range);

    int cx = CLIP3(min_x, max_x, bx + init.x);
    int cy = CLIP3(min_y, max_y, by + init.y);

    unsigned int best_sad = sad_point(ref, cur, cx, cy, bx, by, bw, bh);
    MV best_mv = (MV){ cx - bx, cy - by };

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            unsigned int sad = sad_block(ref, cur, x, y, bx, by, bw, bh);
            if (sad < best_sad) {
                best_sad = sad;
                best_mv.x = x - bx;
                best_mv.y = y - by;
            }
        }
    }
    return best_mv;
}
