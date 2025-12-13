#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "defines.h"
#include "my_fast_me.h"

// Keep a BlockMotionSearch-like wrapper to ease JM integration
static MV BlockMotionSearch(const Frame* ref, const Frame* cur, int bx, int by, MEParams params, MV pred) {
    return xDiamondSearch(ref, cur, bx, by, params, pred);
}

// SAD accounting to compare search effort between FS and ADS
unsigned long long g_sad_count_fs = 0;
unsigned long long g_sad_count_ds = 0;
int g_count_mode = 0; // 0: none, 1: FS, 2: DS

typedef struct {
    const char* input_path; // YUV420 file (we consume only Y plane)
    int width;
    int height;
    int frames;     // number of frames to process (>=2 if using input)
    int block_w;
    int block_h;
    int search_range;
    int max_iters;
    int ads_only;   // if non-zero, skip full search baseline
    int verbose;    // if non-zero, print per-block logs
} CLIParams;

static void print_usage(const char* exe) {
    printf("Usage: %s [-i input.yuv -w W -h H --frames N] [--block B] [--range R] [--max-iters M] [--ads-only] [--verbose]\n", exe);
    printf("If no -i is provided, a synthetic gradient + shift (3,-2) test is used.\n");
}

static int parse_int(const char* s, int* out) {
    if (!s || !out) return 0;
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    *out = (int)v;
    return 1;
}

// Read one Y plane of size width*height from file into Frame; returns 1 on success
static int read_y_plane(FILE* f, Frame* dst, int width, int height) {
    size_t sz = (size_t)width * (size_t)height;
    size_t rd = fread(dst->data, 1, sz, f);
    return rd == sz;
}

// Simple harness: partition current frame into blocks and search
int main(int argc, char** argv) {
    // Defaults (synthetic QCIF gradient test)
    CLIParams cli = {0};
    cli.width = 176;
    cli.height = 144;
    cli.frames = 2;
    cli.block_w = 16;
    cli.block_h = 16;
    cli.search_range = 32;
    cli.max_iters = 64;
    cli.ads_only = 0;
    cli.verbose = 0;

    // Parse CLI
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-i") && i + 1 < argc) {
            cli.input_path = argv[++i];
        } else if (!strcmp(argv[i], "-w") && i + 1 < argc) {
            parse_int(argv[++i], &cli.width);
        } else if (!strcmp(argv[i], "-h") && i + 1 < argc) {
            parse_int(argv[++i], &cli.height);
        } else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            parse_int(argv[++i], &cli.frames);
        } else if (!strcmp(argv[i], "--block") && i + 1 < argc) {
            parse_int(argv[++i], &cli.block_w);
            cli.block_h = cli.block_w;
        } else if (!strcmp(argv[i], "--range") && i + 1 < argc) {
            parse_int(argv[++i], &cli.search_range);
        } else if (!strcmp(argv[i], "--max-iters") && i + 1 < argc) {
            parse_int(argv[++i], &cli.max_iters);
        } else if (!strcmp(argv[i], "--ads-only")) {
            cli.ads_only = 1;
        } else if (!strcmp(argv[i], "--verbose")) {
            cli.verbose = 1;
        } else if (!strcmp(argv[i], "-help") || !strcmp(argv[i], "--help")) {
            print_usage(argv[0]);
            return 0;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    int W = cli.width;
    int H = cli.height;
    int block_w = cli.block_w;
    int block_h = cli.block_h;
    if (block_w <= 0 || block_h <= 0 || W < block_w || H < block_h) {
        fprintf(stderr, "Invalid block or frame size.\n");
        return 1;
    }

    Frame ref = { W, H, W, NULL };
    Frame cur = { W, H, W, NULL };
    ref.data = (uint8_t*)malloc((size_t)W * (size_t)H);
    cur.data = (uint8_t*)malloc((size_t)W * (size_t)H);
    if (!ref.data || !cur.data) return 1;

    if (cli.input_path) {
        FILE* f = fopen(cli.input_path, "rb");
        if (!f) {
            fprintf(stderr, "Cannot open input file: %s\n", cli.input_path);
            return 1;
        }
        size_t y_sz = (size_t)W * (size_t)H;
        size_t frame_sz = y_sz + 2 * (y_sz / 4); // YUV420

        // Need at least 2 frames for motion; if only 1, reuse as both ref/cur
        if (!read_y_plane(f, &ref, W, H)) {
            fprintf(stderr, "Failed to read frame 0\n");
            fclose(f);
            return 1;
        }
        fseek(f, (long)(frame_sz - y_sz), SEEK_CUR); // skip UV
        if (!read_y_plane(f, &cur, W, H)) {
            memcpy(cur.data, ref.data, y_sz);
        } else {
            fseek(f, (long)(frame_sz - y_sz), SEEK_CUR); // skip UV of frame 1
        }
        fclose(f);
    } else {
        // Synthetic gradient with known shift
        int shift_x = 3, shift_y = -2; // ground truth integer motion
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                ref.data[y*W + x] = (uint8_t)((x + y) & 0xFF);
            }
        }
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                int sx = CLIP3(0, W-1, x + shift_x);
                int sy = CLIP3(0, H-1, y + shift_y);
                cur.data[y*W + x] = ref.data[sy*W + sx];
            }
        }
    }

    MEParams params;
    params.block_w = block_w;
    params.block_h = block_h;
    params.search_range = cli.search_range;
    params.max_iters = cli.max_iters;

    int blocks_x = W / block_w;
    int blocks_y = H / block_h;
    
    clock_t start_fs = 0, end_fs = 0, start_ds = 0, end_ds = 0;
    double time_fs = 0.0, time_ds = 0.0;
    double total_loss = 0.0;
    int total_blocks = 0;
    unsigned long long total_sad_fs = 0, total_sad_ds = 0;

    printf("===== Motion Estimation Comparison: Diamond Search vs Full Search =====\n");
    printf("Frame: %dx%d, Block: %dx%d, Search Range: %d\n\n", W, H, block_w, block_h, params.search_range);

    // Full Search baseline (optional)
    if (!cli.ads_only) {
        g_count_mode = 1;
        start_fs = clock();
        for (int by = 0; by < blocks_y; ++by) {
            for (int bx = 0; bx < blocks_x; ++bx) {
                int px = bx * block_w;
                int py = by * block_h;
                MV pred = (MV){0,0};
                MV mv_fs = xFullSearch(&ref, &cur, px, py, params, pred);
                unsigned int sad_fs = sad_block(&ref, &cur, px + mv_fs.x, py + mv_fs.y, px, py, block_w, block_h);
                total_sad_fs += sad_fs;
            }
        }
        end_fs = clock();
        time_fs = ((double)(end_fs - start_fs)) / CLOCKS_PER_SEC * 1000.0; // ms
        g_count_mode = 0;
    }

    // Diamond Search
    g_count_mode = 2;
    start_ds = clock();
    for (int by = 0; by < blocks_y; ++by) {
        for (int bx = 0; bx < blocks_x; ++bx) {
            int px = bx * block_w;
            int py = by * block_h;
            MV pred = (MV){0,0};
            
            unsigned int cost_fs = 0;
            MV mv_fs = (MV){0,0};
            if (!cli.ads_only) {
                mv_fs = xFullSearch(&ref, &cur, px, py, params, pred);
                cost_fs = sad_block(&ref, &cur, px + mv_fs.x, py + mv_fs.y, px, py, block_w, block_h);
            }

            MV mv_ds = xDiamondSearch(&ref, &cur, px, py, params, pred);
            unsigned int cost_ds = sad_block(&ref, &cur, px + mv_ds.x, py + mv_ds.y, px, py, block_w, block_h);
            
            total_sad_ds += cost_ds;
            
            double loss_percentage = 0.0;
            if (!cli.ads_only && cost_fs > 0) {
                loss_percentage = ((double)(cost_ds - cost_fs) / (double)cost_fs) * 100.0;
            }
            total_loss += loss_percentage;
            total_blocks++;

            if (cli.verbose) {
                if (cli.ads_only) {
                    printf("Block (%2d,%2d): DS_MV=(%3d,%3d) | Cost_DS=%6u\n",
                           bx, by, mv_ds.x, mv_ds.y, cost_ds);
                } else {
                    printf("Block (%2d,%2d): DS_MV=(%3d,%3d) FS_MV=(%3d,%3d) | Cost_DS=%6u Cost_FS=%6u | Loss=%6.2f%%\n",
                           bx, by, mv_ds.x, mv_ds.y, mv_fs.x, mv_fs.y, cost_ds, cost_fs, loss_percentage);
                }
            }
        }
    }
    end_ds = clock();
    g_count_mode = 0;
    time_ds = ((double)(end_ds - start_ds)) / CLOCKS_PER_SEC * 1000.0; // ms

    // Summary statistics
    double avg_loss = cli.ads_only ? 0.0 : total_loss / total_blocks;
    double time_saving = cli.ads_only ? 0.0 : ((time_fs - time_ds) / time_fs) * 100.0;
    
    printf("\n===== Summary =====\n");
    if (!cli.ads_only) {
        printf("Full Search Points:    %llu\n", g_sad_count_fs);
    }
    printf("Diamond Search Points: %llu\n", g_sad_count_ds);
    if (!cli.ads_only) {
        printf("Full Search Time:    %.2f ms\n", time_fs);
    }
    printf("Diamond Search Time: %.2f ms\n", time_ds);
    if (!cli.ads_only) {
        printf("Time Saving:         %.2f%%\n", time_saving);
        printf("\n");
        printf("Average SAD (Full Search):    %.2f\n", (double)total_sad_fs / total_blocks);
    }
    printf("Average SAD (Diamond Search): %.2f\n", (double)total_sad_ds / total_blocks);
    if (!cli.ads_only) {
        printf("Average Quality Loss:         %.2f%%\n", avg_loss);
        if (avg_loss <= 5.0) {
            printf("\nQuality Loss <= 5%% - Diamond Search is effective!\n");
        } else {
            printf("\nQuality Loss > 5%% - Consider tuning parameters\n");
        }
    }
    printf("\n");

    free(ref.data);
    free(cur.data);
    return 0;
}
