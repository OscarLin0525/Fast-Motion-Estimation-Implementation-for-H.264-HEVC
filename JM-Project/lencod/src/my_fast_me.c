#include <limits.h>
#include <stdlib.h>
#include "my_fast_me.h"

// From mv-search.c for SAD accounting (demo harness only)
extern int g_count_mode; // 0: none, 1: FS, 2: DS
extern unsigned long long g_sad_count_fs;
extern unsigned long long g_sad_count_ds;

static unsigned int sad_point(const Frame* ref, const Frame* cur, int cx, int cy, int bx, int by, int bw, int bh) {
    return sad_block(ref, cur, cx, cy, bx, by, bw, bh);
}

unsigned int sad_block(const Frame* ref, const Frame* cur, int rx, int ry, int bx, int by, int bw, int bh) {
    // Bounds clipping for reference coords
    rx = CLIP3(0, ref->width - bw, rx);
    ry = CLIP3(0, ref->height - bh, ry);
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

MV xDiamondSearch(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init) {
    int bw = params.block_w;
    int bh = params.block_h;
    int range = params.search_range;
    int max_iters = params.max_iters > 0 ? params.max_iters : 64;

    // Adapt range/iters for small blocks
    int effective_range = range;
    if (bw <= 8 || bh <= 8) {
        effective_range = range / 2;
        if (effective_range < 8) effective_range = 8;
        if (max_iters > 32) max_iters = 32;
    }

    // Search window clipped to frame boundaries
    int min_x = CLIP3(0, ref->width - bw, bx - effective_range);
    int max_x = CLIP3(0, ref->width - bw, bx + effective_range);
    int min_y = CLIP3(0, ref->height - bh, by - effective_range);
    int max_y = CLIP3(0, ref->height - bh, by + effective_range);

    // Center position in reference (clamped to search window)
    int cx = CLIP3(min_x, max_x, bx + init.x);
    int cy = CLIP3(min_y, max_y, by + init.y);

    unsigned int best_sad = sad_point(ref, cur, cx, cy, bx, by, bw, bh);
    MV best_mv = (MV){ cx - bx, cy - by };

    // Large Diamond Search Pattern (step=2)
    const int ldsp_offsets[8][2] = {
        {  0, -2 }, {  2,  0 }, {  0,  2 }, { -2,  0 },
        {  2, -2 }, {  2,  2 }, { -2,  2 }, { -2, -2 }
    };

    // Small Diamond Search Pattern (step=1)
    const int sdsp_offsets[4][2] = {
        {  0, -1 }, {  1,  0 }, {  0,  1 }, { -1,  0 }
    };

    int iters = 0;
    int use_ldsp = 1; // start with coarse LDSP
    while (iters < max_iters) {
        ++iters;

        unsigned int center_sad = sad_point(ref, cur, cx, cy, bx, by, bw, bh);
        unsigned int local_best = center_sad;
        int step_x = 0, step_y = 0;

        const int (*pattern)[2] = use_ldsp ? ldsp_offsets : sdsp_offsets;
        int pattern_len = use_ldsp ? 8 : 4;

        for (int i = 0; i < pattern_len; ++i) {
            int nx = cx + pattern[i][0];
            int ny = cy + pattern[i][1];
            if (nx < min_x || nx > max_x || ny < min_y || ny > max_y)
                continue;
            unsigned int s = sad_point(ref, cur, nx, ny, bx, by, bw, bh);
            if (s < local_best) {
                local_best = s;
                step_x = pattern[i][0];
                step_y = pattern[i][1];
            }
        }

        if (local_best < center_sad) {
            cx = CLIP3(min_x, max_x, cx + step_x);
            cy = CLIP3(min_y, max_y, cy + step_y);
            best_sad = local_best;
            best_mv.x = cx - bx;
            best_mv.y = cy - by;
            continue; // stay in current pattern
        }

        // If LDSP converged, switch to SDSP; if SDSP converged, stop
        if (use_ldsp) {
            use_ldsp = 0;
            continue;
        }
        break; // SDSP center is best
    }

    return best_mv;
}

MV xFullSearch(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init) {
    int bw = params.block_w;
    int bh = params.block_h;
    int range = params.search_range;

    // Adapt range similar to ADS for fair comparison
    int effective_range = range;
    if (bw <= 8 || bh <= 8) {
        effective_range = range / 2;
        if (effective_range < 8) effective_range = 8;
    }

    // Search window clipped to frame boundaries
    int min_x = CLIP3(0, ref->width - bw, bx - effective_range);
    int max_x = CLIP3(0, ref->width - bw, bx + effective_range);
    int min_y = CLIP3(0, ref->height - bh, by - effective_range);
    int max_y = CLIP3(0, ref->height - bh, by + effective_range);

    // Clamp initial MV to the window
    int cx = CLIP3(min_x, max_x, bx + init.x);
    int cy = CLIP3(min_y, max_y, by + init.y);

    unsigned int best_sad = sad_point(ref, cur, cx, cy, bx, by, bw, bh);
    MV best_mv = (MV){ cx - bx, cy - by };

    // Exhaustive search within range
    for (int test_y = min_y; test_y <= max_y; ++test_y) {
        for (int test_x = min_x; test_x <= max_x; ++test_x) {
            unsigned int sad = sad_block(ref, cur, test_x, test_y, bx, by, bw, bh);
            if (sad < best_sad) {
                best_sad = sad;
                best_mv.x = test_x - bx;
                best_mv.y = test_y - by;
            }
        }
    }

    return best_mv;
}
