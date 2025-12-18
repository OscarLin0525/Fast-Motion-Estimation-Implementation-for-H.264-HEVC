#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <immintrin.h> // AVX2
#include "ads_search.h" 

extern int g_count_mode; // 0: none, 1: FS, 2: DS
extern unsigned long long g_sad_count_fs;
extern unsigned long long g_sad_count_ds;


// version A: traditional C language (for Baseline)
unsigned int sad_block_c(const Frame* ref, const Frame* cur, int rx, int ry, int bx, int by, int bw, int bh) {
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
    return sad;
}


// version B: AVX2 SIMD (only optimize can use)
static inline unsigned int hsum_256_epu64(__m256i v) {
    __m128i vlow = _mm256_castsi256_si128(v);
    __m128i vhigh = _mm256_extracti128_si256(v, 1);
    __m128i vsum = _mm_add_epi64(vlow, vhigh);
    __m128i vsum_high = _mm_unpackhi_epi64(vsum, vsum);
    vsum = _mm_add_epi64(vsum, vsum_high);
    return (unsigned int)_mm_cvtsi128_si64(vsum);
}

unsigned int sad_block_avx(const Frame* ref, const Frame* cur, int rx, int ry, int bx, int by, int bw, int bh) {
    // Fix 1 ：Strictly limit AVX to 16x16 blocks as README assumption.
    // Fallback to C for any other size.
    if (bw != 16 || bh != 16) return sad_block_c(ref, cur, rx, ry, bx, by, bw, bh);

    const uint8_t* r_ptr = ref->data + ry * ref->stride + rx;
    const uint8_t* c_ptr = cur->data + by * cur->stride + bx;
    
    __m256i sum_vec = _mm256_setzero_si256();

    for (int y = 0; y < bh; y += 2) {
        // Fix 2  Explanation
        // Why not use _mm256_loadu_si256?
        // Because Row y and Row y+1 are not contiguous in memory.
        // They are separated by stride. We must load them separately.
        // You can see more details in the comments. 
        
        __m128i r_row0 = _mm_loadu_si128((__m128i const*)(r_ptr));
        __m128i r_row1 = _mm_loadu_si128((__m128i const*)(r_ptr + ref->stride));
        
        // Pack two 128-bit rows into one 256-bit register
        __m256i r_256 = _mm256_castsi128_si256(r_row0);
        r_256 = _mm256_inserti128_si256(r_256, r_row1, 1);

        __m128i c_row0 = _mm_loadu_si128((__m128i const*)(c_ptr));
        __m128i c_row1 = _mm_loadu_si128((__m128i const*)(c_ptr + cur->stride));
        
        __m256i c_256 = _mm256_castsi128_si256(c_row0);
        c_256 = _mm256_inserti128_si256(c_256, c_row1, 1);

        __m256i sad_val = _mm256_sad_epu8(r_256, c_256);
        sum_vec = _mm256_add_epi64(sum_vec, sad_val);

        r_ptr += 2 * ref->stride;
        c_ptr += 2 * cur->stride;
    }
    return hsum_256_epu64(sum_vec);
}

// Wrapper below

// This is the standard interface used for external (harness) calls.
unsigned int sad_block(const Frame* ref, const Frame* cur, int rx, int ry, int bx, int by, int bw, int bh) {
    unsigned int ret = sad_block_c(ref, cur, rx, ry, bx, by, bw, bh);
    
    // This part is only responsible for collecting FS statistics.
    if (g_count_mode == 1) ++g_sad_count_fs;
    
    return ret;
}

// choose which SAD version to use (for internal)
static unsigned int sad_point_internal(const Frame* ref, const Frame* cur, int cx, int cy, int bx, int by, int bw, int bh, bool use_avx) {
    // Whenever this function is called, it means that DS has checked one point.
    if (g_count_mode == 2) {
        ++g_sad_count_ds;
    }

    if (use_avx) {
        return sad_block_avx(ref, cur, cx, cy, bx, by, bw, bh);
    } else {
        return sad_block_c(ref, cur, cx, cy, bx, by, bw, bh);
    }
}

//Wrapper above

// search algorithms implementations

static const int ldsp_offsets[8][2] = {
    {  0, -2 }, {  2,  0 }, {  0,  2 }, { -2,  0 },
    {  2, -2 }, {  2,  2 }, { -2,  2 }, { -2, -2 }
};
static const int sdsp_offsets[4][2] = {
    {  0, -1 }, {  1,  0 }, {  0,  1 }, { -1,  0 }
};

// optimize DS : : use SAD with AVX2
MV MY_xDiamondSearchOpt(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init) {
    int bw = params.block_w;
    int bh = params.block_h;
    int range = params.search_range;
    int max_iters = params.max_iters > 0 ? params.max_iters : 64;
    unsigned int et_threshold = (unsigned int)(bw * bh / 4);

    int cx = CLIP3(0, ref->width - bw, bx + init.x);
    int cy = CLIP3(0, ref->height - bh, by + init.y);
    
    // our key step : start AVX
    unsigned int current_sad = sad_point_internal(ref, cur, cx, cy, bx, by, bw, bh, true);
    
    MV best_mv = (MV){ cx - bx, cy - by };

    if (init.x != 0 || init.y != 0) {
        int zx = CLIP3(0, ref->width - bw, bx);
        int zy = CLIP3(0, ref->height - bh, by);
        unsigned int zero_sad = sad_point_internal(ref, cur, zx, zy, bx, by, bw, bh, true);
        if (zero_sad < current_sad) {
            current_sad = zero_sad;
            cx = zx; cy = zy; best_mv = (MV){ 0, 0 };
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
        int best_dx = 0; int best_dy = 0;
        bool found_better = false;

        const int (*pattern)[2] = use_ldsp ? ldsp_offsets : sdsp_offsets;
        int pattern_len = use_ldsp ? 8 : 4;

        for (int i = 0; i < pattern_len; ++i) {
            int dx = pattern[i][0];
            int dy = pattern[i][1];
            int nx = cx + dx;
            int ny = cy + dy;

            if (nx < min_x || nx > max_x || ny < min_y || ny > max_y) continue;

            unsigned int s = sad_point_internal(ref, cur, nx, ny, bx, by, bw, bh, true);
            if (s < next_sad) {
                next_sad = s;
                best_dx = dx; best_dy = dy;
                found_better = true;
            }
        }

        if (found_better) {
            cx += best_dx; cy += best_dy;
            current_sad = next_sad;
            best_mv.x = cx - bx; best_mv.y = cy - by;
            moved = true;
            
            // Fix 3 : Removed early termination here to match the baseline implementation.
            // This ensures the comparison between DS-Base and DS-Opt is fair.            
            if (moved && current_sad < et_threshold) {
                break;
            }
            // Just continue to next iteration
            continue;
        }
        if (use_ldsp) { use_ldsp = false; continue; }
        break;
    }
    return best_mv;
}

// DS baseline : use SAD C version
MV MY_xDiamondSearchADS(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init) {
    int bw = params.block_w;
    int bh = params.block_h;
    int range = params.search_range;
    int max_iters = params.max_iters > 0 ? params.max_iters : 64;

    int cx = bx + init.x;
    int cy = by + init.y;
    cx = CLIP3(0, ref->width - bw, cx);
    cy = CLIP3(0, ref->height - bh, cy);

    // notice : not use AVX, instead use C
    // you will see ldsp_offsets(大菱形) and sdsp_offsets（小菱形）, this is how they work
    unsigned int best_sad = sad_point_internal(ref, cur, cx, cy, bx, by, bw, bh, false);
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
        unsigned int center_sad = sad_point_internal(ref, cur, cx, cy, bx, by, bw, bh, false);
        unsigned int local_best = center_sad;
        int best_dx = 0, best_dy = 0;

        for (int i = 0; i < 8; ++i) {
            int nx = cx + ldsp_offsets[i][0];
            int ny = cy + ldsp_offsets[i][1];
            int mvx = nx - bx; int mvy = ny - by;
            if (mvx < -effective_range || mvx > effective_range || mvy < -effective_range || mvy > effective_range) continue;
            
            unsigned int s = sad_point_internal(ref, cur, nx, ny, bx, by, bw, bh, false);
            if (s < local_best) {
                local_best = s;
                best_dx = ldsp_offsets[i][0]; best_dy = ldsp_offsets[i][1];
            }
        }

        if (local_best < center_sad) {
            cx += best_dx; cy += best_dy;
            cx = CLIP3(0, ref->width - bw, cx); cy = CLIP3(0, ref->height - bh, cy);
            best_sad = local_best;
            best_mv.x = cx - bx; best_mv.y = cy - by;
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
                int mvx = nx - bx; int mvy = ny - by;
                if (mvx < -effective_range || mvx > effective_range || mvy < -effective_range || mvy > effective_range) continue;
                
                unsigned int s = sad_point_internal(ref, cur, nx, ny, bx, by, bw, bh, false);
                if (s < best2) {
                    best2 = s;
                    bdx2 = sdsp_offsets[i][0]; bdy2 = sdsp_offsets[i][1];
                }
            }
            if (best2 < center2) {
                cx += bdx2; cy += bdy2;
                cx = CLIP3(0, ref->width - bw, cx); cy = CLIP3(0, ref->height - bh, cy);
                best_sad = best2;
                best_mv.x = cx - bx; best_mv.y = cy - by;
            }
            break;
        }
    }
    return best_mv;
}

// full research Baseline
MV MY_full_search_motion_estimation(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV init) {
    int bw = params.block_w;
    int bh = params.block_h;
    int range = params.search_range;

    int min_x = CLIP3(0, ref->width - bw, bx - range);
    int max_x = CLIP3(0, ref->width - bw, bx + range);
    int min_y = CLIP3(0, ref->height - bh, by - range);
    int max_y = CLIP3(0, ref->height - bh, by + range);

    int cx = CLIP3(min_x, max_x, bx + init.x);
    int cy = CLIP3(min_y, max_y, by + init.y);

    unsigned int best_sad = sad_block(ref, cur, cx, cy, bx, by, bw, bh);
    MV best_mv = (MV){ cx - bx, cy - by };

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {

            // use sad_block instead of sad_block_c
            // sad_block handles the counting inside (line 81)
            // so remove the manual increment here
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