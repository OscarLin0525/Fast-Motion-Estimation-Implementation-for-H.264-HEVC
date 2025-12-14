#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "defines.h"
#include "ads_search.h"

unsigned long long g_sad_count_fs = 0;
unsigned long long g_sad_count_ds = 0;
int g_count_mode = 0; // 0: none, 1: FS, 2: DS

typedef struct {
    const char* input_path; // YUV420 file (Y plane only)
    int width;
    int height;
    int frames;
    int block_w;
    int block_h;
    int search_range;
    int max_iters;
    int verbose;
} CLIParams;

static void print_usage(const char* exe) {
    printf("Usage: %s [-i input.yuv -w W -h H --frames N] [--block B] [--range R] [--max-iters M] [--verbose]\n", exe);
    printf("If -i is omitted, runs synthetic gradient + shift (3,-2) unit test; otherwise runs YUV test (Y plane only).\n");
}

static int parse_int(const char* s, int* out) {
    if (!s || !out) return 0;
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    *out = (int)v;
    return 1;
}

static int read_y_plane(FILE* f, Frame* dst, int width, int height) {
    size_t sz = (size_t)width * (size_t)height;
    size_t rd = fread(dst->data, 1, sz, f);
    return rd == sz;
}

static void run_ds(const char* label,
                   MV (*ds_fn)(const Frame*, const Frame*, int, int, MEParams, MV),
                   const Frame* ref, const Frame* cur, const MEParams* params,
                   int blocks_x, int blocks_y, int block_w, int block_h,
                   const unsigned int* fs_costs, int verbose) {
    g_count_mode = 2;
    clock_t start_ds = clock();
    unsigned long long total_sad_ds = 0;
    double total_loss = 0.0;
    int total_blocks = 0;
    int idx = 0;

    for (int by = 0; by < blocks_y; ++by) {
        for (int bx = 0; bx < blocks_x; ++bx, ++idx) {
            int px = bx * block_w;
            int py = by * block_h;
            MV pred = (MV){0,0};
            MV mv_ds = ds_fn(ref, cur, px, py, *params, pred);
            unsigned int cost_ds = sad_block(ref, cur, px + mv_ds.x, py + mv_ds.y, px, py, block_w, block_h);
            unsigned int cost_fs = fs_costs[idx];
            total_sad_ds += cost_ds;
            double loss = 0.0;
            if (cost_fs > 0) loss = ((double)(cost_ds - cost_fs) / (double)cost_fs) * 100.0;
            total_loss += loss;
            total_blocks++;
            if (verbose) {
                printf("%s Block (%2d,%2d): DS_MV=(%3d,%3d) Cost_DS=%6u Cost_FS=%6u Loss=%6.2f%%\n",
                       label, bx, by, mv_ds.x, mv_ds.y, cost_ds, cost_fs, loss);
            }
        }
    }
    clock_t end_ds = clock();
    double time_ds = ((double)(end_ds - start_ds)) / CLOCKS_PER_SEC * 1000.0;
    double avg_loss = total_blocks ? (total_loss / total_blocks) : 0.0;
    printf("[%s] Points: %llu | Time: %.2f ms | Avg SAD: %.2f | Avg Loss vs FS: %.2f%%\n",
           label, g_sad_count_ds, time_ds, (double)total_sad_ds / total_blocks, avg_loss);
    g_sad_count_ds = 0;
    g_count_mode = 0;
}

int main(int argc, char** argv) {
    CLIParams cli = {0};
    cli.width = 176;
    cli.height = 144;
    cli.frames = 2;
    cli.block_w = 16;
    cli.block_h = 16;
    cli.search_range = 32;
    cli.max_iters = 64;
    cli.verbose = 0;

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

        if (!read_y_plane(f, &ref, W, H)) {
            fprintf(stderr, "Failed to read frame 0\n");
            fclose(f);
            return 1;
        }
        fseek(f, (long)(frame_sz - y_sz), SEEK_CUR);
        if (!read_y_plane(f, &cur, W, H)) {
            memcpy(cur.data, ref.data, y_sz);
        } else {
            fseek(f, (long)(frame_sz - y_sz), SEEK_CUR);
        }
        fclose(f);
    } else {
        int shift_x = 3, shift_y = -2;
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
    int blocks_total = blocks_x * blocks_y;

    unsigned int* fs_costs = (unsigned int*)malloc((size_t)blocks_total * sizeof(unsigned int));
    if (!fs_costs) return 1;

    printf("===== Motion Estimation Comparison =====\n");
    printf("Frame: %dx%d, Block: %dx%d, Search Range: %d\n", W, H, block_w, block_h, params.search_range);
    printf("Modes: FS baseline, DS opt, DS base\n\n");

    // Full Search baseline
    g_count_mode = 1;
    clock_t start_fs = clock();
    unsigned long long total_sad_fs = 0;
    int idx = 0;
    for (int by = 0; by < blocks_y; ++by) {
        for (int bx = 0; bx < blocks_x; ++bx, ++idx) {
            int px = bx * block_w;
            int py = by * block_h;
            MV pred = (MV){0,0};
            MV mv_fs = full_search_motion_estimation(&ref, &cur, px, py, params, pred);
            unsigned int sad_fs = sad_block(&ref, &cur, px + mv_fs.x, py + mv_fs.y, px, py, block_w, block_h);
            fs_costs[idx] = sad_fs;
            total_sad_fs += sad_fs;
            if (cli.verbose) {
                printf("FS Block (%2d,%2d): MV=(%3d,%3d) Cost=%6u\n", bx, by, mv_fs.x, mv_fs.y, sad_fs);
            }
        }
    }
    clock_t end_fs = clock();
    double time_fs = ((double)(end_fs - start_fs)) / CLOCKS_PER_SEC * 1000.0;
    g_count_mode = 0;

    printf("FS baseline: Points: %llu | Time: %.2f ms | Avg SAD: %.2f\n",
           g_sad_count_fs, time_fs, (double)total_sad_fs / blocks_total);
    g_sad_count_fs = 0;

    run_ds("DS opt", xDiamondSearchOpt, &ref, &cur, &params, blocks_x, blocks_y, block_w, block_h, fs_costs, cli.verbose);
    run_ds("DS base", xDiamondSearchADS, &ref, &cur, &params, blocks_x, blocks_y, block_w, block_h, fs_costs, cli.verbose);

    printf("\n");

    free(fs_costs);
    free(ref.data);
    free(cur.data);
    return 0;
}
