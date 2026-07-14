#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <errno.h>
#include "../../inc/oapv.h"
#include "../../src/oapv_port.h"

// defined in oapv_app_util.h
#define ALIGN_VAL(val, align) ((((val) + (align) - 1) / (align)) * (align))

typedef unsigned char u8;
typedef unsigned short u16;

// Multi-mip test configuration structure
typedef struct {
    int mip_level;
    int num_tiles;
    int tile_coords[2000]; // Coordinate pairs - supports up to 1000 tiles per mip
} multi_mip_config_t;

// Test configuration structure
typedef struct {
    const char* name;
    const char* description;
    enum { TEST_SINGLE_TILE, TEST_MULTI_TILE, TEST_MULTI_MIP } test_type;
    int mip_level;
    int tile_coords[5000]; // Coordinate pairs terminated by {-1, -1} - supports up to 2500 tiles (enough for 16K)
    int thread_counts[16]; // Thread counts to test, terminated by 0
    enum { OUTPUT_NONE, OUTPUT_RAW, OUTPUT_Y4M } output_format;
    int measure_performance;
    enum { VALIDATE_QUICK, VALIDATE_FULL } validation_level;

    // Multi-mip specific configuration
    int num_mips; // Number of mip levels to decode simultaneously
    multi_mip_config_t mip_configs[10]; // Support up to 10 mip levels
} test_config_t;

// Predefined test configurations covering all current use cases
static test_config_t test_configs[] = {
    // Single tile tests
    {
        .name = "single_tile_mip0_origin",
        .description = "Single tile at origin (0,0) from mip level 0",
        .test_type = TEST_SINGLE_TILE,
        .mip_level = 0,
        .tile_coords = {0, 0, -1, -1}, // Sentinel terminated
        .thread_counts = {1, 0}, // Single thread, terminated by 0
        .output_format = OUTPUT_Y4M,
        .measure_performance = 0,
        .validation_level = VALIDATE_FULL
    },
    {
        .name = "single_tile_mip1_center", 
        .description = "Single tile at center from mip level 1",
        .test_type = TEST_SINGLE_TILE,
        .mip_level = 1,
        .tile_coords = {3, 2, -1, -1},
        .thread_counts = {1, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 0,
        .validation_level = VALIDATE_FULL
    },
    // Multi-tile performance tests (from test_multi_tile_performance.c patterns)
    {
        .name = "multi_2x2_contiguous",
        .description = "2x2 contiguous tile block (zoom close scenario)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {0,0, 1,0, 0,1, 1,1, -1,-1},
        .thread_counts = {4, 8, 16, 0},
        .output_format = OUTPUT_NONE,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_4x4_contiguous",
        .description = "4x4 contiguous tile block (very close scenario)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {0,0, 1,0, 2,0, 3,0, 0,1, 1,1, 2,1, 3,1, 
                       0,2, 1,2, 2,2, 3,2, 0,3, 1,3, 2,3, 3,3, -1,-1},
        .thread_counts = {4, 8, 16, 0},
        .output_format = OUTPUT_NONE,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_middle_6x4_scaling",
        .description = "Middle 6x4 tile block with thread scaling (main test case)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {4,2, 5,2, 6,2, 7,2, 8,2, 9,2,  // Row 2
                       4,3, 5,3, 6,3, 7,3, 8,3, 9,3,  // Row 3  
                       4,4, 5,4, 6,4, 7,4, 8,4, 9,4,  // Row 4
                       4,5, 5,5, 6,5, 7,5, 8,5, 9,5,  // Row 5
                       -1,-1},
        .thread_counts = {4, 8, 16, 24, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_FULL
    },
    {
        .name = "multi_horizontal_strip",
        .description = "Horizontal strip (camera pan scenario)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {4,4, 5,4, 6,4, 7,4, 8,4, 9,4, -1,-1},
        .thread_counts = {4, 6, 0},
        .output_format = OUTPUT_NONE,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_vertical_strip",
        .description = "Vertical strip (camera tilt scenario)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {7,2, 7,3, 7,4, 7,5, 7,6, 7,7, -1,-1},
        .thread_counts = {4, 6, 0},
        .output_format = OUTPUT_NONE,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_sparse_corners",
        .description = "Sparse corner tiles (wide view scenario)", 
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {0,0, 14,0, 0,8, 14,8, -1,-1},
        .thread_counts = {1, 4, 32, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_single_center",
        .description = "Single center tile via multi-tile decoder (comparison test)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {7,4, -1,-1},
        .thread_counts = {1, 4, 0},
        .output_format = OUTPUT_NONE,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_full_frame_validation",
        .description = "Full frame (255 tiles) with detailed metrics",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {
                       0,0, 1,0, 2,0, 3,0, 4,0, 5,0, 6,0, 7,0, 8,0, 9,0, 10,0, 11,0, 12,0, 13,0, 14,0,  // Row 0
                       0,1, 1,1, 2,1, 3,1, 4,1, 5,1, 6,1, 7,1, 8,1, 9,1, 10,1, 11,1, 12,1, 13,1, 14,1,  // Row 1
                       0,2, 1,2, 2,2, 3,2, 4,2, 5,2, 6,2, 7,2, 8,2, 9,2, 10,2, 11,2, 12,2, 13,2, 14,2,  // Row 2
                       0,3, 1,3, 2,3, 3,3, 4,3, 5,3, 6,3, 7,3, 8,3, 9,3, 10,3, 11,3, 12,3, 13,3, 14,3,  // Row 3
                       0,4, 1,4, 2,4, 3,4, 4,4, 5,4, 6,4, 7,4, 8,4, 9,4, 10,4, 11,4, 12,4, 13,4, 14,4,  // Row 4
                       0,5, 1,5, 2,5, 3,5, 4,5, 5,5, 6,5, 7,5, 8,5, 9,5, 10,5, 11,5, 12,5, 13,5, 14,5,  // Row 5
                       0,6, 1,6, 2,6, 3,6, 4,6, 5,6, 6,6, 7,6, 8,6, 9,6, 10,6, 11,6, 12,6, 13,6, 14,6,  // Row 6
                       0,7, 1,7, 2,7, 3,7, 4,7, 5,7, 6,7, 7,7, 8,7, 9,7, 10,7, 11,7, 12,7, 13,7, 14,7,  // Row 7
                       0,8, 1,8, 2,8, 3,8, 4,8, 5,8, 6,8, 7,8, 8,8, 9,8, 10,8, 11,8, 12,8, 13,8, 14,8,  // Row 8
                       -1,-1},
        .thread_counts = {1, 4, 8, 16, 24, 32, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_all_mip0",
        .description = "All tiles from mip level 0 (full resolution)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {
                       // 15x9 grid = 135 tiles for 3840x2160 with 256x256 tiles
                       0,0, 1,0, 2,0, 3,0, 4,0, 5,0, 6,0, 7,0, 8,0, 9,0, 10,0, 11,0, 12,0, 13,0, 14,0,  // Row 0
                       0,1, 1,1, 2,1, 3,1, 4,1, 5,1, 6,1, 7,1, 8,1, 9,1, 10,1, 11,1, 12,1, 13,1, 14,1,  // Row 1
                       0,2, 1,2, 2,2, 3,2, 4,2, 5,2, 6,2, 7,2, 8,2, 9,2, 10,2, 11,2, 12,2, 13,2, 14,2,  // Row 2
                       0,3, 1,3, 2,3, 3,3, 4,3, 5,3, 6,3, 7,3, 8,3, 9,3, 10,3, 11,3, 12,3, 13,3, 14,3,  // Row 3
                       0,4, 1,4, 2,4, 3,4, 4,4, 5,4, 6,4, 7,4, 8,4, 9,4, 10,4, 11,4, 12,4, 13,4, 14,4,  // Row 4
                       0,5, 1,5, 2,5, 3,5, 4,5, 5,5, 6,5, 7,5, 8,5, 9,5, 10,5, 11,5, 12,5, 13,5, 14,5,  // Row 5
                       0,6, 1,6, 2,6, 3,6, 4,6, 5,6, 6,6, 7,6, 8,6, 9,6, 10,6, 11,6, 12,6, 13,6, 14,6,  // Row 6
                       0,7, 1,7, 2,7, 3,7, 4,7, 5,7, 6,7, 7,7, 8,7, 9,7, 10,7, 11,7, 12,7, 13,7, 14,7,  // Row 7
                       0,8, 1,8, 2,8, 3,8, 4,8, 5,8, 6,8, 7,8, 8,8, 9,8, 10,8, 11,8, 12,8, 13,8, 14,8,  // Row 8
                       -1,-1},
        .thread_counts = {32, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_all_mip1",
        .description = "All tiles from mip level 1 (half resolution)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 1,
        .tile_coords = {
                       // 8x5 grid = 40 tiles for 1920x1080 with 256x256 tiles (last column is 128px, last row is 56px)
                       0,0, 1,0, 2,0, 3,0, 4,0, 5,0, 6,0, 7,0,  // Row 0 (256px tall)
                       0,1, 1,1, 2,1, 3,1, 4,1, 5,1, 6,1, 7,1,  // Row 1 (256px tall)
                       0,2, 1,2, 2,2, 3,2, 4,2, 5,2, 6,2, 7,2,  // Row 2 (256px tall)
                       0,3, 1,3, 2,3, 3,3, 4,3, 5,3, 6,3, 7,3,  // Row 3 (256px tall)
                       0,4, 1,4, 2,4, 3,4, 4,4, 5,4, 6,4, 7,4,  // Row 4 (56px tall)
                       -1,-1},
        .thread_counts = {8, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_some_mip1",
        .description = "Some tiles from mip level 1 (half resolution)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 1,
        .tile_coords = {
                       // 8x5 grid = 40 tiles for 1920x1080 with 256x256 tiles (last column is 128px, last row is 56px)
                       0,4,  // Right:128. Bottom:56
                       -1,-1},
        .thread_counts = {8, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_all_mip2",
        .description = "All tiles from mip level 2 (quarter resolution 960x540)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 2,
        .tile_coords = {
                       // 4x3 grid = 12 tiles for 960x540 with 256x256 tiles
                       0,0, 1,0, 2,0, 3,0,  // Row 0
                       0,1, 1,1, 2,1, 3,1,  // Row 1
                       0,2, 1,2, 2,2, 3,2,  // Row 2
                       -1,-1},
        .thread_counts = {8, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_some_mip2",
        .description = "Some tiles from mip level 2 (quarter resolution 960x540)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 2,
        .tile_coords = {
                       // 4x3 grid = 12 tiles for 960x540 with 256x256 tiles
                       0,2,  // Right:192. Bottom:28
                       -1,-1},
        .thread_counts = {1, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_all_mip3",
        .description = "All tiles from mip level 3 (480x270)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 3,
        .tile_coords = {
                       // 2x2 grid = 4 tiles for 480x270 with 256x256 tiles
                       0,0, 1,0,  // Row 0
                       0,1, 1,1,  // Row 1
                       -1,-1},
        .thread_counts = {4, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_all_mip4",
        .description = "Single tile from mip level 4 (240x135)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 4,
        .tile_coords = {
                       0,0,  // Single tile covers entire frame
                       -1,-1},
        .thread_counts = {1, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_all_mip5",
        .description = "Single tile from mip level 5 (120x67)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 5,
        .tile_coords = {
                       0,0,  // Single tile covers entire frame
                       -1,-1},
        .thread_counts = {1, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_all_mip6",
        .description = "Single tile from mip level 6 (60x33)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 6,
        .tile_coords = {
                       0,0,  // Single tile covers entire frame
                       -1,-1},
        .thread_counts = {1, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_all_mip7",
        .description = "Single tile from mip level 7 (30x16)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 7,
        .tile_coords = {
                       0,0,  // Single tile covers entire frame
                       -1,-1},
        .thread_counts = {1, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_mip1_single",
        .description = "Single tile from mip level 1 (debug test)",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 1,
        .tile_coords = {
                       0,0,  // Just tile (0,0)
                       -1,-1},
        .thread_counts = {1, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "multi_mip1_first_row",
        .description = "First row of tiles from mip level 1",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 1,
        .tile_coords = {
                       0,0, 1,0, 2,0, 3,0, 4,0, 5,0, 6,0, 7,0,  // Row 0 only
                       -1,-1},
        .thread_counts = {1, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    // Individual single-tile tests for all 40 tiles in mip level 1 (8x5 grid)
    // Row 0 (y=0)
    {"single_mip1_tile_0_0", "Single tile (0,0) from mip level 1", TEST_SINGLE_TILE, 1, {0,0, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_1_0", "Single tile (1,0) from mip level 1", TEST_SINGLE_TILE, 1, {1,0, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_2_0", "Single tile (2,0) from mip level 1", TEST_SINGLE_TILE, 1, {2,0, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_3_0", "Single tile (3,0) from mip level 1", TEST_SINGLE_TILE, 1, {3,0, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_4_0", "Single tile (4,0) from mip level 1", TEST_SINGLE_TILE, 1, {4,0, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_5_0", "Single tile (5,0) from mip level 1", TEST_SINGLE_TILE, 1, {5,0, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_6_0", "Single tile (6,0) from mip level 1", TEST_SINGLE_TILE, 1, {6,0, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_7_0", "Single tile (7,0) from mip level 1", TEST_SINGLE_TILE, 1, {7,0, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    // Row 1 (y=1)
    {"single_mip1_tile_0_1", "Single tile (0,1) from mip level 1", TEST_SINGLE_TILE, 1, {0,1, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_1_1", "Single tile (1,1) from mip level 1", TEST_SINGLE_TILE, 1, {1,1, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_2_1", "Single tile (2,1) from mip level 1", TEST_SINGLE_TILE, 1, {2,1, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_3_1", "Single tile (3,1) from mip level 1", TEST_SINGLE_TILE, 1, {3,1, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_4_1", "Single tile (4,1) from mip level 1", TEST_SINGLE_TILE, 1, {4,1, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_5_1", "Single tile (5,1) from mip level 1", TEST_SINGLE_TILE, 1, {5,1, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_6_1", "Single tile (6,1) from mip level 1", TEST_SINGLE_TILE, 1, {6,1, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_7_1", "Single tile (7,1) from mip level 1", TEST_SINGLE_TILE, 1, {7,1, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    // Row 2 (y=2)
    {"single_mip1_tile_0_2", "Single tile (0,2) from mip level 1", TEST_SINGLE_TILE, 1, {0,2, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_1_2", "Single tile (1,2) from mip level 1", TEST_SINGLE_TILE, 1, {1,2, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_2_2", "Single tile (2,2) from mip level 1", TEST_SINGLE_TILE, 1, {2,2, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_3_2", "Single tile (3,2) from mip level 1", TEST_SINGLE_TILE, 1, {3,2, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_4_2", "Single tile (4,2) from mip level 1", TEST_SINGLE_TILE, 1, {4,2, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_5_2", "Single tile (5,2) from mip level 1", TEST_SINGLE_TILE, 1, {5,2, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_6_2", "Single tile (6,2) from mip level 1", TEST_SINGLE_TILE, 1, {6,2, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_7_2", "Single tile (7,2) from mip level 1", TEST_SINGLE_TILE, 1, {7,2, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    // Row 3 (y=3)
    {"single_mip1_tile_0_3", "Single tile (0,3) from mip level 1", TEST_SINGLE_TILE, 1, {0,3, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_1_3", "Single tile (1,3) from mip level 1", TEST_SINGLE_TILE, 1, {1,3, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_2_3", "Single tile (2,3) from mip level 1", TEST_SINGLE_TILE, 1, {2,3, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_3_3", "Single tile (3,3) from mip level 1", TEST_SINGLE_TILE, 1, {3,3, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_4_3", "Single tile (4,3) from mip level 1", TEST_SINGLE_TILE, 1, {4,3, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_5_3", "Single tile (5,3) from mip level 1", TEST_SINGLE_TILE, 1, {5,3, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_6_3", "Single tile (6,3) from mip level 1", TEST_SINGLE_TILE, 1, {6,3, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_7_3", "Single tile (7,3) from mip level 1", TEST_SINGLE_TILE, 1, {7,3, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    // Row 4 (y=4) - last row with 56px height
    {"single_mip1_tile_0_4", "Single tile (0,4) from mip level 1", TEST_SINGLE_TILE, 1, {0,4, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_1_4", "Single tile (1,4) from mip level 1", TEST_SINGLE_TILE, 1, {1,4, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_2_4", "Single tile (2,4) from mip level 1", TEST_SINGLE_TILE, 1, {2,4, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_3_4", "Single tile (3,4) from mip level 1", TEST_SINGLE_TILE, 1, {3,4, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_4_4", "Single tile (4,4) from mip level 1", TEST_SINGLE_TILE, 1, {4,4, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_5_4", "Single tile (5,4) from mip level 1", TEST_SINGLE_TILE, 1, {5,4, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_6_4", "Single tile (6,4) from mip level 1", TEST_SINGLE_TILE, 1, {6,4, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},
    {"single_mip1_tile_7_4", "Single tile (7,4) from mip level 1", TEST_SINGLE_TILE, 1, {7,4, -1,-1}, {1, 0}, OUTPUT_Y4M, 0, VALIDATE_QUICK},

    // 16K frame tests (60x34 = 2040 tiles)
    {
        .name = "16k_single_corner",
        .description = "Single tile at 16K frame corner (0,0)",
        .test_type = TEST_SINGLE_TILE,
        .mip_level = 0,
        .tile_coords = {0, 0, -1, -1},
        .thread_counts = {1, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 0,
        .validation_level = VALIDATE_FULL
    },
    {
        .name = "16k_single_center",
        .description = "Single tile at 16K frame center (30,17)",
        .test_type = TEST_SINGLE_TILE,
        .mip_level = 0,
        .tile_coords = {30, 17, -1, -1},
        .thread_counts = {1, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 0,
        .validation_level = VALIDATE_FULL
    },
    {
        .name = "16k_single_edge_right",
        .description = "Single tile at 16K frame right edge (59,17)",
        .test_type = TEST_SINGLE_TILE,
        .mip_level = 0,
        .tile_coords = {59, 17, -1, -1},
        .thread_counts = {1, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 0,
        .validation_level = VALIDATE_FULL
    },
    {
        .name = "16k_single_edge_bottom",
        .description = "Single tile at 16K frame bottom edge (30,33)",
        .test_type = TEST_SINGLE_TILE,
        .mip_level = 0,
        .tile_coords = {30, 33, -1, -1},
        .thread_counts = {1, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 0,
        .validation_level = VALIDATE_FULL
    },
    {
        .name = "16k_multi_2x2_center",
        .description = "16K 2x2 contiguous tile block at center",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {29,16, 30,16, 29,17, 30,17, -1,-1},
        .thread_counts = {4, 8, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_FULL
    },
    {
        .name = "16k_multi_10x10_center",
        .description = "16K 10x10 tile block testing high tile count",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {
            25,12, 26,12, 27,12, 28,12, 29,12, 30,12, 31,12, 32,12, 33,12, 34,12,  // Row 12
            25,13, 26,13, 27,13, 28,13, 29,13, 30,13, 31,13, 32,13, 33,13, 34,13,  // Row 13
            25,14, 26,14, 27,14, 28,14, 29,14, 30,14, 31,14, 32,14, 33,14, 34,14,  // Row 14
            25,15, 26,15, 27,15, 28,15, 29,15, 30,15, 31,15, 32,15, 33,15, 34,15,  // Row 15
            25,16, 26,16, 27,16, 28,16, 29,16, 30,16, 31,16, 32,16, 33,16, 34,16,  // Row 16
            25,17, 26,17, 27,17, 28,17, 29,17, 30,17, 31,17, 32,17, 33,17, 34,17,  // Row 17
            25,18, 26,18, 27,18, 28,18, 29,18, 30,18, 31,18, 32,18, 33,18, 34,18,  // Row 18
            25,19, 26,19, 27,19, 28,19, 29,19, 30,19, 31,19, 32,19, 33,19, 34,19,  // Row 19
            25,20, 26,20, 27,20, 28,20, 29,20, 30,20, 31,20, 32,20, 33,20, 34,20,  // Row 20
            25,21, 26,21, 27,21, 28,21, 29,21, 30,21, 31,21, 32,21, 33,21, 34,21,  // Row 21
            -1,-1
        },
        .thread_counts = {8, 16, 24, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    {
        .name = "16k_tile_limit_validation",
        .description = "16K frame verifying 2040 tiles > old 400 limit",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {
            // Sample tiles from all 4 corners and center to prove we can address the full 60x34 grid
            0,0,    // Top-left corner
            59,0,   // Top-right corner
            0,33,   // Bottom-left corner
            59,33,  // Bottom-right corner
            30,17,  // Center
            -1,-1
        },
        .thread_counts = {4, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_FULL
    },
    {
        .name = "16k_all_tiles_performance",
        .description = "16K frame decoding ALL 2040 tiles (60x34 grid) - performance test",
        .test_type = TEST_MULTI_TILE,
        .mip_level = 0,
        .tile_coords = {
            // 60x34 grid = 2040 tiles for 16K frame (16384x8704)
            // Row 0
            0,0, 1,0, 2,0, 3,0, 4,0, 5,0, 6,0, 7,0, 8,0, 9,0, 10,0, 11,0, 12,0, 13,0, 14,0, 15,0, 16,0, 17,0, 18,0, 19,0, 20,0, 21,0, 22,0, 23,0, 24,0, 25,0, 26,0, 27,0, 28,0, 29,0, 30,0, 31,0, 32,0, 33,0, 34,0, 35,0, 36,0, 37,0, 38,0, 39,0, 40,0, 41,0, 42,0, 43,0, 44,0, 45,0, 46,0, 47,0, 48,0, 49,0, 50,0, 51,0, 52,0, 53,0, 54,0, 55,0, 56,0, 57,0, 58,0, 59,0,
            // Row 1
            0,1, 1,1, 2,1, 3,1, 4,1, 5,1, 6,1, 7,1, 8,1, 9,1, 10,1, 11,1, 12,1, 13,1, 14,1, 15,1, 16,1, 17,1, 18,1, 19,1, 20,1, 21,1, 22,1, 23,1, 24,1, 25,1, 26,1, 27,1, 28,1, 29,1, 30,1, 31,1, 32,1, 33,1, 34,1, 35,1, 36,1, 37,1, 38,1, 39,1, 40,1, 41,1, 42,1, 43,1, 44,1, 45,1, 46,1, 47,1, 48,1, 49,1, 50,1, 51,1, 52,1, 53,1, 54,1, 55,1, 56,1, 57,1, 58,1, 59,1,
            // Row 2
            0,2, 1,2, 2,2, 3,2, 4,2, 5,2, 6,2, 7,2, 8,2, 9,2, 10,2, 11,2, 12,2, 13,2, 14,2, 15,2, 16,2, 17,2, 18,2, 19,2, 20,2, 21,2, 22,2, 23,2, 24,2, 25,2, 26,2, 27,2, 28,2, 29,2, 30,2, 31,2, 32,2, 33,2, 34,2, 35,2, 36,2, 37,2, 38,2, 39,2, 40,2, 41,2, 42,2, 43,2, 44,2, 45,2, 46,2, 47,2, 48,2, 49,2, 50,2, 51,2, 52,2, 53,2, 54,2, 55,2, 56,2, 57,2, 58,2, 59,2,
            // Row 3
            0,3, 1,3, 2,3, 3,3, 4,3, 5,3, 6,3, 7,3, 8,3, 9,3, 10,3, 11,3, 12,3, 13,3, 14,3, 15,3, 16,3, 17,3, 18,3, 19,3, 20,3, 21,3, 22,3, 23,3, 24,3, 25,3, 26,3, 27,3, 28,3, 29,3, 30,3, 31,3, 32,3, 33,3, 34,3, 35,3, 36,3, 37,3, 38,3, 39,3, 40,3, 41,3, 42,3, 43,3, 44,3, 45,3, 46,3, 47,3, 48,3, 49,3, 50,3, 51,3, 52,3, 53,3, 54,3, 55,3, 56,3, 57,3, 58,3, 59,3,
            // Row 4
            0,4, 1,4, 2,4, 3,4, 4,4, 5,4, 6,4, 7,4, 8,4, 9,4, 10,4, 11,4, 12,4, 13,4, 14,4, 15,4, 16,4, 17,4, 18,4, 19,4, 20,4, 21,4, 22,4, 23,4, 24,4, 25,4, 26,4, 27,4, 28,4, 29,4, 30,4, 31,4, 32,4, 33,4, 34,4, 35,4, 36,4, 37,4, 38,4, 39,4, 40,4, 41,4, 42,4, 43,4, 44,4, 45,4, 46,4, 47,4, 48,4, 49,4, 50,4, 51,4, 52,4, 53,4, 54,4, 55,4, 56,4, 57,4, 58,4, 59,4,
            // Row 5
            0,5, 1,5, 2,5, 3,5, 4,5, 5,5, 6,5, 7,5, 8,5, 9,5, 10,5, 11,5, 12,5, 13,5, 14,5, 15,5, 16,5, 17,5, 18,5, 19,5, 20,5, 21,5, 22,5, 23,5, 24,5, 25,5, 26,5, 27,5, 28,5, 29,5, 30,5, 31,5, 32,5, 33,5, 34,5, 35,5, 36,5, 37,5, 38,5, 39,5, 40,5, 41,5, 42,5, 43,5, 44,5, 45,5, 46,5, 47,5, 48,5, 49,5, 50,5, 51,5, 52,5, 53,5, 54,5, 55,5, 56,5, 57,5, 58,5, 59,5,
            // Row 6
            0,6, 1,6, 2,6, 3,6, 4,6, 5,6, 6,6, 7,6, 8,6, 9,6, 10,6, 11,6, 12,6, 13,6, 14,6, 15,6, 16,6, 17,6, 18,6, 19,6, 20,6, 21,6, 22,6, 23,6, 24,6, 25,6, 26,6, 27,6, 28,6, 29,6, 30,6, 31,6, 32,6, 33,6, 34,6, 35,6, 36,6, 37,6, 38,6, 39,6, 40,6, 41,6, 42,6, 43,6, 44,6, 45,6, 46,6, 47,6, 48,6, 49,6, 50,6, 51,6, 52,6, 53,6, 54,6, 55,6, 56,6, 57,6, 58,6, 59,6,
            // Row 7
            0,7, 1,7, 2,7, 3,7, 4,7, 5,7, 6,7, 7,7, 8,7, 9,7, 10,7, 11,7, 12,7, 13,7, 14,7, 15,7, 16,7, 17,7, 18,7, 19,7, 20,7, 21,7, 22,7, 23,7, 24,7, 25,7, 26,7, 27,7, 28,7, 29,7, 30,7, 31,7, 32,7, 33,7, 34,7, 35,7, 36,7, 37,7, 38,7, 39,7, 40,7, 41,7, 42,7, 43,7, 44,7, 45,7, 46,7, 47,7, 48,7, 49,7, 50,7, 51,7, 52,7, 53,7, 54,7, 55,7, 56,7, 57,7, 58,7, 59,7,
            // Row 8
            0,8, 1,8, 2,8, 3,8, 4,8, 5,8, 6,8, 7,8, 8,8, 9,8, 10,8, 11,8, 12,8, 13,8, 14,8, 15,8, 16,8, 17,8, 18,8, 19,8, 20,8, 21,8, 22,8, 23,8, 24,8, 25,8, 26,8, 27,8, 28,8, 29,8, 30,8, 31,8, 32,8, 33,8, 34,8, 35,8, 36,8, 37,8, 38,8, 39,8, 40,8, 41,8, 42,8, 43,8, 44,8, 45,8, 46,8, 47,8, 48,8, 49,8, 50,8, 51,8, 52,8, 53,8, 54,8, 55,8, 56,8, 57,8, 58,8, 59,8,
            // Row 9
            0,9, 1,9, 2,9, 3,9, 4,9, 5,9, 6,9, 7,9, 8,9, 9,9, 10,9, 11,9, 12,9, 13,9, 14,9, 15,9, 16,9, 17,9, 18,9, 19,9, 20,9, 21,9, 22,9, 23,9, 24,9, 25,9, 26,9, 27,9, 28,9, 29,9, 30,9, 31,9, 32,9, 33,9, 34,9, 35,9, 36,9, 37,9, 38,9, 39,9, 40,9, 41,9, 42,9, 43,9, 44,9, 45,9, 46,9, 47,9, 48,9, 49,9, 50,9, 51,9, 52,9, 53,9, 54,9, 55,9, 56,9, 57,9, 58,9, 59,9,
            // Row 10
            0,10, 1,10, 2,10, 3,10, 4,10, 5,10, 6,10, 7,10, 8,10, 9,10, 10,10, 11,10, 12,10, 13,10, 14,10, 15,10, 16,10, 17,10, 18,10, 19,10, 20,10, 21,10, 22,10, 23,10, 24,10, 25,10, 26,10, 27,10, 28,10, 29,10, 30,10, 31,10, 32,10, 33,10, 34,10, 35,10, 36,10, 37,10, 38,10, 39,10, 40,10, 41,10, 42,10, 43,10, 44,10, 45,10, 46,10, 47,10, 48,10, 49,10, 50,10, 51,10, 52,10, 53,10, 54,10, 55,10, 56,10, 57,10, 58,10, 59,10,
            // Row 11
            0,11, 1,11, 2,11, 3,11, 4,11, 5,11, 6,11, 7,11, 8,11, 9,11, 10,11, 11,11, 12,11, 13,11, 14,11, 15,11, 16,11, 17,11, 18,11, 19,11, 20,11, 21,11, 22,11, 23,11, 24,11, 25,11, 26,11, 27,11, 28,11, 29,11, 30,11, 31,11, 32,11, 33,11, 34,11, 35,11, 36,11, 37,11, 38,11, 39,11, 40,11, 41,11, 42,11, 43,11, 44,11, 45,11, 46,11, 47,11, 48,11, 49,11, 50,11, 51,11, 52,11, 53,11, 54,11, 55,11, 56,11, 57,11, 58,11, 59,11,
            // Row 12
            0,12, 1,12, 2,12, 3,12, 4,12, 5,12, 6,12, 7,12, 8,12, 9,12, 10,12, 11,12, 12,12, 13,12, 14,12, 15,12, 16,12, 17,12, 18,12, 19,12, 20,12, 21,12, 22,12, 23,12, 24,12, 25,12, 26,12, 27,12, 28,12, 29,12, 30,12, 31,12, 32,12, 33,12, 34,12, 35,12, 36,12, 37,12, 38,12, 39,12, 40,12, 41,12, 42,12, 43,12, 44,12, 45,12, 46,12, 47,12, 48,12, 49,12, 50,12, 51,12, 52,12, 53,12, 54,12, 55,12, 56,12, 57,12, 58,12, 59,12,
            // Row 13
            0,13, 1,13, 2,13, 3,13, 4,13, 5,13, 6,13, 7,13, 8,13, 9,13, 10,13, 11,13, 12,13, 13,13, 14,13, 15,13, 16,13, 17,13, 18,13, 19,13, 20,13, 21,13, 22,13, 23,13, 24,13, 25,13, 26,13, 27,13, 28,13, 29,13, 30,13, 31,13, 32,13, 33,13, 34,13, 35,13, 36,13, 37,13, 38,13, 39,13, 40,13, 41,13, 42,13, 43,13, 44,13, 45,13, 46,13, 47,13, 48,13, 49,13, 50,13, 51,13, 52,13, 53,13, 54,13, 55,13, 56,13, 57,13, 58,13, 59,13,
            // Row 14
            0,14, 1,14, 2,14, 3,14, 4,14, 5,14, 6,14, 7,14, 8,14, 9,14, 10,14, 11,14, 12,14, 13,14, 14,14, 15,14, 16,14, 17,14, 18,14, 19,14, 20,14, 21,14, 22,14, 23,14, 24,14, 25,14, 26,14, 27,14, 28,14, 29,14, 30,14, 31,14, 32,14, 33,14, 34,14, 35,14, 36,14, 37,14, 38,14, 39,14, 40,14, 41,14, 42,14, 43,14, 44,14, 45,14, 46,14, 47,14, 48,14, 49,14, 50,14, 51,14, 52,14, 53,14, 54,14, 55,14, 56,14, 57,14, 58,14, 59,14,
            // Row 15
            0,15, 1,15, 2,15, 3,15, 4,15, 5,15, 6,15, 7,15, 8,15, 9,15, 10,15, 11,15, 12,15, 13,15, 14,15, 15,15, 16,15, 17,15, 18,15, 19,15, 20,15, 21,15, 22,15, 23,15, 24,15, 25,15, 26,15, 27,15, 28,15, 29,15, 30,15, 31,15, 32,15, 33,15, 34,15, 35,15, 36,15, 37,15, 38,15, 39,15, 40,15, 41,15, 42,15, 43,15, 44,15, 45,15, 46,15, 47,15, 48,15, 49,15, 50,15, 51,15, 52,15, 53,15, 54,15, 55,15, 56,15, 57,15, 58,15, 59,15,
            // Row 16
            0,16, 1,16, 2,16, 3,16, 4,16, 5,16, 6,16, 7,16, 8,16, 9,16, 10,16, 11,16, 12,16, 13,16, 14,16, 15,16, 16,16, 17,16, 18,16, 19,16, 20,16, 21,16, 22,16, 23,16, 24,16, 25,16, 26,16, 27,16, 28,16, 29,16, 30,16, 31,16, 32,16, 33,16, 34,16, 35,16, 36,16, 37,16, 38,16, 39,16, 40,16, 41,16, 42,16, 43,16, 44,16, 45,16, 46,16, 47,16, 48,16, 49,16, 50,16, 51,16, 52,16, 53,16, 54,16, 55,16, 56,16, 57,16, 58,16, 59,16,
            // Row 17
            0,17, 1,17, 2,17, 3,17, 4,17, 5,17, 6,17, 7,17, 8,17, 9,17, 10,17, 11,17, 12,17, 13,17, 14,17, 15,17, 16,17, 17,17, 18,17, 19,17, 20,17, 21,17, 22,17, 23,17, 24,17, 25,17, 26,17, 27,17, 28,17, 29,17, 30,17, 31,17, 32,17, 33,17, 34,17, 35,17, 36,17, 37,17, 38,17, 39,17, 40,17, 41,17, 42,17, 43,17, 44,17, 45,17, 46,17, 47,17, 48,17, 49,17, 50,17, 51,17, 52,17, 53,17, 54,17, 55,17, 56,17, 57,17, 58,17, 59,17,
            // Row 18
            0,18, 1,18, 2,18, 3,18, 4,18, 5,18, 6,18, 7,18, 8,18, 9,18, 10,18, 11,18, 12,18, 13,18, 14,18, 15,18, 16,18, 17,18, 18,18, 19,18, 20,18, 21,18, 22,18, 23,18, 24,18, 25,18, 26,18, 27,18, 28,18, 29,18, 30,18, 31,18, 32,18, 33,18, 34,18, 35,18, 36,18, 37,18, 38,18, 39,18, 40,18, 41,18, 42,18, 43,18, 44,18, 45,18, 46,18, 47,18, 48,18, 49,18, 50,18, 51,18, 52,18, 53,18, 54,18, 55,18, 56,18, 57,18, 58,18, 59,18,
            // Row 19
            0,19, 1,19, 2,19, 3,19, 4,19, 5,19, 6,19, 7,19, 8,19, 9,19, 10,19, 11,19, 12,19, 13,19, 14,19, 15,19, 16,19, 17,19, 18,19, 19,19, 20,19, 21,19, 22,19, 23,19, 24,19, 25,19, 26,19, 27,19, 28,19, 29,19, 30,19, 31,19, 32,19, 33,19, 34,19, 35,19, 36,19, 37,19, 38,19, 39,19, 40,19, 41,19, 42,19, 43,19, 44,19, 45,19, 46,19, 47,19, 48,19, 49,19, 50,19, 51,19, 52,19, 53,19, 54,19, 55,19, 56,19, 57,19, 58,19, 59,19,
            // Row 20
            0,20, 1,20, 2,20, 3,20, 4,20, 5,20, 6,20, 7,20, 8,20, 9,20, 10,20, 11,20, 12,20, 13,20, 14,20, 15,20, 16,20, 17,20, 18,20, 19,20, 20,20, 21,20, 22,20, 23,20, 24,20, 25,20, 26,20, 27,20, 28,20, 29,20, 30,20, 31,20, 32,20, 33,20, 34,20, 35,20, 36,20, 37,20, 38,20, 39,20, 40,20, 41,20, 42,20, 43,20, 44,20, 45,20, 46,20, 47,20, 48,20, 49,20, 50,20, 51,20, 52,20, 53,20, 54,20, 55,20, 56,20, 57,20, 58,20, 59,20,
            // Row 21
            0,21, 1,21, 2,21, 3,21, 4,21, 5,21, 6,21, 7,21, 8,21, 9,21, 10,21, 11,21, 12,21, 13,21, 14,21, 15,21, 16,21, 17,21, 18,21, 19,21, 20,21, 21,21, 22,21, 23,21, 24,21, 25,21, 26,21, 27,21, 28,21, 29,21, 30,21, 31,21, 32,21, 33,21, 34,21, 35,21, 36,21, 37,21, 38,21, 39,21, 40,21, 41,21, 42,21, 43,21, 44,21, 45,21, 46,21, 47,21, 48,21, 49,21, 50,21, 51,21, 52,21, 53,21, 54,21, 55,21, 56,21, 57,21, 58,21, 59,21,
            // Row 22
            0,22, 1,22, 2,22, 3,22, 4,22, 5,22, 6,22, 7,22, 8,22, 9,22, 10,22, 11,22, 12,22, 13,22, 14,22, 15,22, 16,22, 17,22, 18,22, 19,22, 20,22, 21,22, 22,22, 23,22, 24,22, 25,22, 26,22, 27,22, 28,22, 29,22, 30,22, 31,22, 32,22, 33,22, 34,22, 35,22, 36,22, 37,22, 38,22, 39,22, 40,22, 41,22, 42,22, 43,22, 44,22, 45,22, 46,22, 47,22, 48,22, 49,22, 50,22, 51,22, 52,22, 53,22, 54,22, 55,22, 56,22, 57,22, 58,22, 59,22,
            // Row 23
            0,23, 1,23, 2,23, 3,23, 4,23, 5,23, 6,23, 7,23, 8,23, 9,23, 10,23, 11,23, 12,23, 13,23, 14,23, 15,23, 16,23, 17,23, 18,23, 19,23, 20,23, 21,23, 22,23, 23,23, 24,23, 25,23, 26,23, 27,23, 28,23, 29,23, 30,23, 31,23, 32,23, 33,23, 34,23, 35,23, 36,23, 37,23, 38,23, 39,23, 40,23, 41,23, 42,23, 43,23, 44,23, 45,23, 46,23, 47,23, 48,23, 49,23, 50,23, 51,23, 52,23, 53,23, 54,23, 55,23, 56,23, 57,23, 58,23, 59,23,
            // Row 24
            0,24, 1,24, 2,24, 3,24, 4,24, 5,24, 6,24, 7,24, 8,24, 9,24, 10,24, 11,24, 12,24, 13,24, 14,24, 15,24, 16,24, 17,24, 18,24, 19,24, 20,24, 21,24, 22,24, 23,24, 24,24, 25,24, 26,24, 27,24, 28,24, 29,24, 30,24, 31,24, 32,24, 33,24, 34,24, 35,24, 36,24, 37,24, 38,24, 39,24, 40,24, 41,24, 42,24, 43,24, 44,24, 45,24, 46,24, 47,24, 48,24, 49,24, 50,24, 51,24, 52,24, 53,24, 54,24, 55,24, 56,24, 57,24, 58,24, 59,24,
            // Row 25
            0,25, 1,25, 2,25, 3,25, 4,25, 5,25, 6,25, 7,25, 8,25, 9,25, 10,25, 11,25, 12,25, 13,25, 14,25, 15,25, 16,25, 17,25, 18,25, 19,25, 20,25, 21,25, 22,25, 23,25, 24,25, 25,25, 26,25, 27,25, 28,25, 29,25, 30,25, 31,25, 32,25, 33,25, 34,25, 35,25, 36,25, 37,25, 38,25, 39,25, 40,25, 41,25, 42,25, 43,25, 44,25, 45,25, 46,25, 47,25, 48,25, 49,25, 50,25, 51,25, 52,25, 53,25, 54,25, 55,25, 56,25, 57,25, 58,25, 59,25,
            // Row 26
            0,26, 1,26, 2,26, 3,26, 4,26, 5,26, 6,26, 7,26, 8,26, 9,26, 10,26, 11,26, 12,26, 13,26, 14,26, 15,26, 16,26, 17,26, 18,26, 19,26, 20,26, 21,26, 22,26, 23,26, 24,26, 25,26, 26,26, 27,26, 28,26, 29,26, 30,26, 31,26, 32,26, 33,26, 34,26, 35,26, 36,26, 37,26, 38,26, 39,26, 40,26, 41,26, 42,26, 43,26, 44,26, 45,26, 46,26, 47,26, 48,26, 49,26, 50,26, 51,26, 52,26, 53,26, 54,26, 55,26, 56,26, 57,26, 58,26, 59,26,
            // Row 27
            0,27, 1,27, 2,27, 3,27, 4,27, 5,27, 6,27, 7,27, 8,27, 9,27, 10,27, 11,27, 12,27, 13,27, 14,27, 15,27, 16,27, 17,27, 18,27, 19,27, 20,27, 21,27, 22,27, 23,27, 24,27, 25,27, 26,27, 27,27, 28,27, 29,27, 30,27, 31,27, 32,27, 33,27, 34,27, 35,27, 36,27, 37,27, 38,27, 39,27, 40,27, 41,27, 42,27, 43,27, 44,27, 45,27, 46,27, 47,27, 48,27, 49,27, 50,27, 51,27, 52,27, 53,27, 54,27, 55,27, 56,27, 57,27, 58,27, 59,27,
            // Row 28
            0,28, 1,28, 2,28, 3,28, 4,28, 5,28, 6,28, 7,28, 8,28, 9,28, 10,28, 11,28, 12,28, 13,28, 14,28, 15,28, 16,28, 17,28, 18,28, 19,28, 20,28, 21,28, 22,28, 23,28, 24,28, 25,28, 26,28, 27,28, 28,28, 29,28, 30,28, 31,28, 32,28, 33,28, 34,28, 35,28, 36,28, 37,28, 38,28, 39,28, 40,28, 41,28, 42,28, 43,28, 44,28, 45,28, 46,28, 47,28, 48,28, 49,28, 50,28, 51,28, 52,28, 53,28, 54,28, 55,28, 56,28, 57,28, 58,28, 59,28,
            // Row 29
            0,29, 1,29, 2,29, 3,29, 4,29, 5,29, 6,29, 7,29, 8,29, 9,29, 10,29, 11,29, 12,29, 13,29, 14,29, 15,29, 16,29, 17,29, 18,29, 19,29, 20,29, 21,29, 22,29, 23,29, 24,29, 25,29, 26,29, 27,29, 28,29, 29,29, 30,29, 31,29, 32,29, 33,29, 34,29, 35,29, 36,29, 37,29, 38,29, 39,29, 40,29, 41,29, 42,29, 43,29, 44,29, 45,29, 46,29, 47,29, 48,29, 49,29, 50,29, 51,29, 52,29, 53,29, 54,29, 55,29, 56,29, 57,29, 58,29, 59,29,
            // Row 30
            0,30, 1,30, 2,30, 3,30, 4,30, 5,30, 6,30, 7,30, 8,30, 9,30, 10,30, 11,30, 12,30, 13,30, 14,30, 15,30, 16,30, 17,30, 18,30, 19,30, 20,30, 21,30, 22,30, 23,30, 24,30, 25,30, 26,30, 27,30, 28,30, 29,30, 30,30, 31,30, 32,30, 33,30, 34,30, 35,30, 36,30, 37,30, 38,30, 39,30, 40,30, 41,30, 42,30, 43,30, 44,30, 45,30, 46,30, 47,30, 48,30, 49,30, 50,30, 51,30, 52,30, 53,30, 54,30, 55,30, 56,30, 57,30, 58,30, 59,30,
            // Row 31
            0,31, 1,31, 2,31, 3,31, 4,31, 5,31, 6,31, 7,31, 8,31, 9,31, 10,31, 11,31, 12,31, 13,31, 14,31, 15,31, 16,31, 17,31, 18,31, 19,31, 20,31, 21,31, 22,31, 23,31, 24,31, 25,31, 26,31, 27,31, 28,31, 29,31, 30,31, 31,31, 32,31, 33,31, 34,31, 35,31, 36,31, 37,31, 38,31, 39,31, 40,31, 41,31, 42,31, 43,31, 44,31, 45,31, 46,31, 47,31, 48,31, 49,31, 50,31, 51,31, 52,31, 53,31, 54,31, 55,31, 56,31, 57,31, 58,31, 59,31,
            // Row 32
            0,32, 1,32, 2,32, 3,32, 4,32, 5,32, 6,32, 7,32, 8,32, 9,32, 10,32, 11,32, 12,32, 13,32, 14,32, 15,32, 16,32, 17,32, 18,32, 19,32, 20,32, 21,32, 22,32, 23,32, 24,32, 25,32, 26,32, 27,32, 28,32, 29,32, 30,32, 31,32, 32,32, 33,32, 34,32, 35,32, 36,32, 37,32, 38,32, 39,32, 40,32, 41,32, 42,32, 43,32, 44,32, 45,32, 46,32, 47,32, 48,32, 49,32, 50,32, 51,32, 52,32, 53,32, 54,32, 55,32, 56,32, 57,32, 58,32, 59,32,
            // Row 33 (last row)
            0,33, 1,33, 2,33, 3,33, 4,33, 5,33, 6,33, 7,33, 8,33, 9,33, 10,33, 11,33, 12,33, 13,33, 14,33, 15,33, 16,33, 17,33, 18,33, 19,33, 20,33, 21,33, 22,33, 23,33, 24,33, 25,33, 26,33, 27,33, 28,33, 29,33, 30,33, 31,33, 32,33, 33,33, 34,33, 35,33, 36,33, 37,33, 38,33, 39,33, 40,33, 41,33, 42,33, 43,33, 44,33, 45,33, 46,33, 47,33, 48,33, 49,33, 50,33, 51,33, 52,33, 53,33, 54,33, 55,33, 56,33, 57,33, 58,33, 59,33,
            -1,-1
        },
        .thread_counts = {8, 16, 32, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_QUICK
    },
    // Multi-mip tests
    {
        .name = "multimip_single_center",
        .description = "Multi-mip decode: single center tile from different mip levels",
        .test_type = TEST_MULTI_MIP,
        .thread_counts = {8, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_FULL,
        .num_mips = 3,
        .mip_configs = {
            {.mip_level = 0, .num_tiles = 1, .tile_coords = {7, 4}},
            {.mip_level = 1, .num_tiles = 1, .tile_coords = {3, 2}},
            {.mip_level = 2, .num_tiles = 1, .tile_coords = {1, 1}}
        }
    },
    {
        .name = "multimip_2x2_blocks",
        .description = "Multi-mip decode: 2x2 tile blocks from different mip levels",
        .test_type = TEST_MULTI_MIP,
        .thread_counts = {8, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_FULL,
        .num_mips = 3,
        .mip_configs = {
            {.mip_level = 0, .num_tiles = 4, .tile_coords = {6, 3, 7, 3, 6, 4, 7, 4}},
            {.mip_level = 1, .num_tiles = 4, .tile_coords = {2, 1, 3, 1, 2, 2, 3, 2}},
            {.mip_level = 2, .num_tiles = 4, .tile_coords = {0, 0, 1, 0, 0, 1, 1, 1}}
        }
    },
    {
        .name = "multimip_sparse",
        .description = "Multi-mip decode: sparse tiles from different mip levels",
        .test_type = TEST_MULTI_MIP,
        .thread_counts = {8, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_FULL,
        .num_mips = 4,
        .mip_configs = {
            {.mip_level = 0, .num_tiles = 4, .tile_coords = {0, 0, 14, 0, 0, 8, 14, 8}},
            {.mip_level = 1, .num_tiles = 2, .tile_coords = {0, 0, 7, 4}},
            {.mip_level = 2, .num_tiles = 1, .tile_coords = {1, 1}},
            {.mip_level = 3, .num_tiles = 1, .tile_coords = {0, 0}}
        }
    },
    {
        .name = "multi_all_mip9",
        .description = "Multi-mip decode: request non-existent mip level 9 along with valid ones",
        .test_type = TEST_MULTI_MIP,
        .thread_counts = {8, 0},
        .output_format = OUTPUT_Y4M,
        .measure_performance = 1,
        .validation_level = VALIDATE_FULL,
        .num_mips = 4,
        .mip_configs = {
            {.mip_level = 0, .num_tiles = 1, .tile_coords = {7, 4}},
            {.mip_level = 2, .num_tiles = 1, .tile_coords = {1, 1}},
            {.mip_level = 5, .num_tiles = 1, .tile_coords = {0, 0}},
            {.mip_level = 9, .num_tiles = 1, .tile_coords = {0, 0}} // This should fail
        }
    },
    {
        .name = "multimip_performance_comparison",
        .description = "Performance comparison: multi-mip vs individual mip decodes",
        .test_type = TEST_MULTI_MIP,
        .thread_counts = {8, 0},
        .output_format = OUTPUT_NONE,
        .measure_performance = 1,
        .validation_level = VALIDATE_FULL,
        .num_mips = 3,
        .mip_configs = {
            {.mip_level = 0, .num_tiles = 6, .tile_coords = {6, 3, 7, 3, 8, 3, 6, 4, 7, 4, 8, 4}},
            {.mip_level = 1, .num_tiles = 4, .tile_coords = {2, 1, 3, 1, 2, 2, 3, 2}},
            {.mip_level = 2, .num_tiles = 2, .tile_coords = {1, 1, 2, 1}}
        }
    },
    {
        .name = "invalid_test",
        .description = "Test with only invalid mip levels to test error handling",
        .test_type = TEST_MULTI_MIP,
        .thread_counts = {1, 0},
        .output_format = OUTPUT_NONE,
        .measure_performance = 0,
        .validation_level = VALIDATE_FULL,
        .num_mips = 2,
        .mip_configs = {
            {.mip_level = 15, .num_tiles = 1, .tile_coords = {0, 0}}, // Should fail
            {.mip_level = 20, .num_tiles = 1, .tile_coords = {0, 0}}  // Should fail
        }
    },
    {
        .name = "debug_single_mip0",
        .description = "Debug test: single mip level 0 only",
        .test_type = TEST_MULTI_MIP,
        .thread_counts = {1, 0},
        .output_format = OUTPUT_NONE,
        .measure_performance = 0,
        .validation_level = VALIDATE_FULL,
        .num_mips = 1,
        .mip_configs = {
            {.mip_level = 0, .num_tiles = 1, .tile_coords = {7, 4}} // Should work
        }
    }
};

static int num_test_configs = sizeof(test_configs) / sizeof(test_configs[0]);

// Function declarations
int run_multi_mip_test_config(const char* input_file, const test_config_t* config);

// Count tiles from sentinel-terminated coordinate array
int count_tiles_from_coords(const int* tile_coords) {
    int count = 0;
    for(int i = 0; tile_coords[i] != -1 && tile_coords[i+1] != -1; i += 2) {
        count++;
    }
    return count;
}

void delete_frame_buffer(oapv_imgb_t* imgb)
{
    if(imgb) {
        for(int c = 0; c < imgb->np; ++c) {
            if(imgb->a[c]) {
                oapv_mfree(imgb->a[c]);
            }
        }

        oapv_mfree(imgb);
    }
}

int chroma_format_idc_to_color_format(int chroma_format_idc)
{
    return ((chroma_format_idc == 0)   ? OAPV_CF_YCBCR400
            : (chroma_format_idc == 1) ? OAPV_CF_YCBCR420
            : (chroma_format_idc == 2) ? OAPV_CF_YCBCR422
            : (chroma_format_idc == 3) ? OAPV_CF_YCBCR444
                                       : OAPV_CF_YCBCR4444);
}

int color_format_to_chroma_format_idc(int color_format)
{
    if(color_format == OAPV_CF_PLANAR2) {
        return 2;
    }
    else {
        return ((color_format == OAPV_CF_YCBCR400)   ? 0
                : (color_format == OAPV_CF_YCBCR420) ? 1
                : (color_format == OAPV_CF_YCBCR422) ? 2
                : (color_format == OAPV_CF_YCBCR444) ? 3
                                                     : 4);
    }
}

// Retrieve the number of components for the given format.
int get_num_components(int color_format)
{
    // todo: OAPV_CF_PLANAR2
    switch(color_format) {
    case OAPV_CF_YCBCR400:
        return 1;
    case OAPV_CF_YCBCR420:
    case OAPV_CF_YCBCR422:
    case OAPV_CF_YCBCR444:
        return 3;
    case OAPV_CF_YCBCR4444:
        return 4;
    default:
        return 0; // not supported for now.
    }
}

int get_chroma_width_factor(int color_format, int c)
{
    // todo: OAPV_CF_PLANAR2
    switch(color_format) {
    case OAPV_CF_YCBCR400:
        return 1;
    case OAPV_CF_YCBCR420:
    case OAPV_CF_YCBCR422:
        return c == 0 ? 1 : 2;
    case OAPV_CF_YCBCR444:
    case OAPV_CF_YCBCR4444:
    default:
        return 1; // not supported for now.
    }
}

int get_chroma_height_factor(int color_format, int c)
{
    // todo: OAPV_CF_PLANAR2
    switch(color_format) {
    case OAPV_CF_YCBCR400:
        return 1;
    case OAPV_CF_YCBCR420:
        return c == 0 ? 1 : 2;
    case OAPV_CF_YCBCR422:
    case OAPV_CF_YCBCR444:
    case OAPV_CF_YCBCR4444:
    default:
        return 1; // not supported for now.
    }
}

// Create frame buffer for specific component
oapv_imgb_t *create_frame_buffer(int width, int height, int color_format, int bit_depth)
{
    oapv_imgb_t *imgb = oapv_malloc(sizeof(oapv_imgb_t));
    if (!imgb) return NULL;

    int num_components = get_num_components(color_format);
    
    memset(imgb, 0, sizeof(oapv_imgb_t));

    imgb->np = num_components;
    int bd = (bit_depth + 7) >> 3; 

    for(int c = 0; c < num_components; ++c) {

        imgb->w[c] = width / get_chroma_width_factor(color_format, c);
        imgb->h[c] = height / get_chroma_height_factor(color_format, c);
    
        // width and height need to be aligned to macroblock size
        imgb->aw[c] = ALIGN_VAL(imgb->w[c], OAPV_MB_W);
        imgb->s[c] = imgb->aw[c] * bd;
        imgb->ah[c] = ALIGN_VAL(imgb->h[c], OAPV_MB_H);
        imgb->e[c] = imgb->ah[c];

        imgb->bsize[c] = imgb->s[c] * imgb->e[c];
        imgb->a[c] = imgb->baddr[c] = oapv_calloc(imgb->bsize[c], 1);
        if(!imgb->a[c]) {
            delete_frame_buffer(imgb);
            return NULL;
        }
    }
    
    imgb->cs = OAPV_CS_SET(color_format, bit_depth, 0);
    imgb->refcnt = 1;
    
    return imgb;
}

// Write Y4M file for full frame
void write_frame_y4m(const char* filename, oapv_imgb_t* frame_buffer) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        printf("ERROR: Cannot create output file %s\n", filename);
        return;
    }
    
    int width = frame_buffer->w[0];
    int height = frame_buffer->h[0];
    int color_format = OAPV_CS_GET_FORMAT(frame_buffer->cs);
    int bit_depth = OAPV_CS_GET_BIT_DEPTH(frame_buffer->cs);
    int bd = OAPV_CS_GET_BYTE_DEPTH(frame_buffer->cs); /* byte unit */
    
    char color_buf[16] = { '\0' };
    switch (color_format)
    {
    case OAPV_CF_YCBCR400:
        if(bit_depth == 8)
            strcpy(color_buf, "mono");
        else if(bit_depth == 10)
            strcpy(color_buf, "mono10");
        break;
    case OAPV_CF_YCBCR420:
        if(bit_depth == 8)
            strcpy(color_buf, "420mpeg2");
        else if(bit_depth == 10)
            strcpy(color_buf, "420p10");
        break;
    case OAPV_CF_YCBCR422:
        if(bit_depth == 8)
            strcpy(color_buf, "422");
        else if(bit_depth == 10)
            strcpy(color_buf, "422p10");
        else if(bit_depth == 12)
            strcpy(color_buf, "422p12");
        break;
    case OAPV_CF_YCBCR444:
    case OAPV_CF_YCBCR4444: // for testing 4444 is considered 444.
        if(bit_depth == 8)
            strcpy(color_buf, "444");
        else if(bit_depth == 10)
            strcpy(color_buf, "444p10");
        else if(bit_depth == 12)
            strcpy(color_buf, "444p12");
        break;
    default:
        break;
    }

    if(strlen(color_buf) == 0) {
        printf("ERROR: Color format is not supported by y4m\n");
        return;
    }
    
    // Y4M header
    fprintf(fp, "YUV4MPEG2 W%d H%d F25:1 Ip A1:1 C%s\n", width, height, color_buf);
    fprintf(fp, "FRAME\n");

    // Note: because of 4444, we only save up to 3 components because y4m doesn't support 4.
    int num_components = frame_buffer->np > 3 ? 3 : frame_buffer->np;
 
    // Note: Buffer stride may have some padding for MB alignement.
    for(int i = 0; i < num_components; i++) {
        u8 *p8 = (u8 *)frame_buffer->a[i] + (frame_buffer->s[i] * frame_buffer->y[i]) + (frame_buffer->x[i] * bd);

        for(int j = 0; j < frame_buffer->h[i]; j++) {
            fwrite(p8, frame_buffer->w[i] * bd, 1, fp);
            p8 += frame_buffer->s[i];
        }
    }
    
    fclose(fp);
}

// Write raw file with header
int write_frame_raw(const char* filename, oapv_imgb_t* frame_buffer) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        printf("ERROR: Cannot create output file %s - %s (errno: %d)\n", filename, strerror(errno), errno);
        return 0;
    }
    
    int width = frame_buffer->w[0];
    int height = frame_buffer->h[0];
    int bit_depth = OAPV_CS_GET_BIT_DEPTH(frame_buffer->cs);
    int chroma_format_idc = color_format_to_chroma_format_idc(OAPV_CS_GET_FORMAT(frame_buffer->cs));
    int version = 1;
    
    // Write header
    fwrite(&width, sizeof(int), 1, fp);
    fwrite(&height, sizeof(int), 1, fp);
    fwrite(&bit_depth, sizeof(int), 1, fp);
    fwrite(&chroma_format_idc, sizeof(int), 1, fp);
    fwrite(&version, sizeof(int), 1, fp);

    int bd = OAPV_CS_GET_BYTE_DEPTH(frame_buffer->cs);

    // Note: Buffer stride may have some padding for MB alignement.
    for(int i = 0; i < frame_buffer->np; i++) {
        u8 *p8 = (u8 *)frame_buffer->a[i] + (frame_buffer->s[i] * frame_buffer->y[i]) + (frame_buffer->x[i] * bd);

        for(int j = 0; j < frame_buffer->h[i]; j++) {
            fwrite(p8, frame_buffer->w[i] * bd, 1, fp);
            p8 += frame_buffer->s[i];
        }
    }

    fclose(fp);
    return 1;
}

// Quick validation - just count non-zero pixels
void validate_quick(oapv_imgb_t* y_buffer, int num_tiles) {
    u16* y_data = (u16*)y_buffer->a[0];
    int width = y_buffer->w[0];
    int height = y_buffer->h[0];
    
    int nonzero_pixels = 0;
    int total_pixels = width * height;
    
    for(int i = 0; i < total_pixels; i++) {
        if(y_data[i] != 0) nonzero_pixels++;
    }
    
    printf("Validation: %d/%d non-zero pixels (%.2f%%)\n", 
           nonzero_pixels, total_pixels, 100.0 * nonzero_pixels / total_pixels);
}

// Full validation - check chroma patterns too
void validate_full(oapv_imgb_t* frame_buffer, int num_tiles) {
    // Y validation
    u16* y_data = (u16*)frame_buffer->a[0];
    int width = frame_buffer->w[0];
    int height = frame_buffer->h[0];
    
    int y_nonzero = 0;
    for(int i = 0; i < width * height; i++) {
        if(y_data[i] != 0) y_nonzero++;
    }
    
    // U/V validation  
    u16* u_data = (u16*)frame_buffer->a[1];
    u16* v_data = (u16*)frame_buffer->a[2];
    int chroma_width_factor = get_chroma_width_factor(OAPV_CS_GET_FORMAT(frame_buffer->cs), 1);
    int chroma_width = width / chroma_width_factor;
    int chroma_pixels = chroma_width * height;
    
    int u_nonzero = 0, v_nonzero = 0;
    for(int i = 0; i < chroma_pixels; i++) {
        if(u_data[i] != 0) u_nonzero++;
        if(v_data[i] != 0) v_nonzero++;
    }
    
    // Find the bounding box of non-zero Y pixels to determine decoded region
    int min_x = width, max_x = -1, min_y = height, max_y = -1;
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            if(y_data[y * width + x] != 0) {
                if(x < min_x) min_x = x;
                if(x > max_x) max_x = x;
                if(y < min_y) min_y = y;
                if(y > max_y) max_y = y;
            }
        }
    }
    
    printf("Full validation:\n");
    printf("  Y: %d/%d non-zero pixels (%.2f%%)\n", y_nonzero, width*height, 100.0*y_nonzero/(width*height));
    printf("  U: %d/%d non-zero pixels (%.2f%%)\n", u_nonzero, chroma_pixels, 100.0*u_nonzero/chroma_pixels);
    printf("  V: %d/%d non-zero pixels (%.2f%%)\n", v_nonzero, chroma_pixels, 100.0*v_nonzero/chroma_pixels);
    
    // For selective decoding, only check chroma quality in the decoded region
    if(min_x <= max_x && min_y <= max_y) {
        // Convert to chroma coordinates
        int chroma_min_x = min_x / chroma_width_factor;
        int chroma_max_x = max_x / chroma_width_factor;
        
        // Check for chroma stripe artifacts only in decoded region
        int u_zero_cols = 0, v_zero_cols = 0;
        int decoded_cols = chroma_max_x - chroma_min_x + 1;
        
        for(int col = chroma_min_x; col <= chroma_max_x; col++) {
            int u_col_zeros = 0, v_col_zeros = 0;
            for(int row = min_y; row <= max_y; row++) {
                if(u_data[row * chroma_width + col] == 0) u_col_zeros++;
                if(v_data[row * chroma_width + col] == 0) v_col_zeros++;
            }
            int decoded_rows = max_y - min_y + 1;
            if(u_col_zeros == decoded_rows) u_zero_cols++;
            if(v_col_zeros == decoded_rows) v_zero_cols++;
        }
        
        printf("  Decoded region: (%d,%d) to (%d,%d)\n", min_x, min_y, max_x, max_y);
        printf("  Chroma quality in decoded region: U zero columns: %d/%d, V zero columns: %d/%d\n", 
               u_zero_cols, decoded_cols, v_zero_cols, decoded_cols);
        
        if(u_zero_cols > 0 || v_zero_cols > 0) {
            printf("  WARNING: Chroma stripe artifacts detected in decoded region!\n");
        } else {
            printf("  SUCCESS: Perfect chroma quality in decoded region\n");
        }
    } else {
        printf("  No decoded pixels found for validation\n");
    }
}

long long file_istream_tell(oapvd_istream_t *istream)
{
    FILE *fp = (FILE *)istream->data;
    return oapv_ftell(fp);
}

int file_istream_seek(oapvd_istream_t *istream, long long offset, int origin)
{
    FILE *fp = (FILE *)istream->data;
    return oapv_fseek(fp, offset, origin);
}

size_t file_istream_read(oapvd_istream_t *istream, void* buffer, size_t size, size_t count)
{
    FILE *fp = (FILE *)istream->data;

    return fread(buffer, size, count, fp);
}

void file_istream_init(oapvd_istream_t* istream, FILE* fp)
{
    istream->data = fp;
    istream->tell = file_istream_tell;
    istream->seek = file_istream_seek;
    istream->read = file_istream_read;
}

// Local single-mip selective-decode wrapper over oapvd_decode_selective_multi_mips.
// Replaces the removed oapvd_decode_selective / oapvd_decode_selective_multi APIs:
// it builds a single-entry multi-mip request (exactly what the removed _multi
// wrapper did internally) and copies the returned metadata back to sel_decode.
static int decode_selective_single(oapvd_t did, oapvd_istream_t *istream,
                                   oapv_selective_decode_t *sel_decode, oapvm_t mid, oapvd_stat_t *stat)
{
    oapv_mip_request_t mip_request;
    memset(&mip_request, 0, sizeof(mip_request));
    mip_request.mip_level = sel_decode->mip_level;
    mip_request.num_tiles = sel_decode->num_tiles;
    memcpy(mip_request.tile_coords, sel_decode->tile_coords, sizeof(sel_decode->tile_coords));
    mip_request.output_buffer = sel_decode->output_buffer;

    oapv_multi_mip_decode_t multi;
    memset(&multi, 0, sizeof(multi));
    multi.num_mips = 1;
    multi.mip_requests = &mip_request;

    int ret = oapvd_decode_selective_multi_mips(did, istream, &multi, mid, stat);
    if(OAPV_SUCCEEDED(ret) && OAPV_FAILED(mip_request.status)) {
        ret = mip_request.status;
    }

    sel_decode->actual_frame_width = mip_request.frame_width_mb_aligned;
    sel_decode->actual_frame_height = mip_request.frame_height_mb_aligned;
    sel_decode->actual_tile_width = mip_request.tile_width_mb_aligned;
    sel_decode->actual_tile_height = mip_request.tile_height_mb_aligned;
    sel_decode->bit_depth = mip_request.bit_depth;
    sel_decode->chroma_format_idc = mip_request.chroma_format_idc;
    return ret;
}

// Decode a full frame (all tiles) with a pre-existing decoder.
static int decode_mip(const char* input_file, int mip_level, oapvd_t decoder_id)
{
    // The input file contains one input frame and a mip level to load per line.
    FILE *fp = fopen(input_file, "rb");
    if(!fp) {
        printf("ERROR: Cannot open input file %s\n", input_file);
        return -1;
    }

    // Set up selective decode structure on the heap (to test this because that's how it is done in UE).
    oapv_selective_decode_t *sel_decode = oapv_malloc(sizeof(oapv_selective_decode_t));
    if (sel_decode == NULL)
    {
        printf("ERROR: Failed to allocate memory for sel_decode\n");
        fclose(fp);
        return -1;
    }

    memset(sel_decode, 0, sizeof(oapv_selective_decode_t));

    // Select just one tile of the specified mip for now. We want to read the frame and tile size first. 
    sel_decode->mip_level = mip_level;
    sel_decode->num_tiles = 1;
    sel_decode->tile_coords[0] = sel_decode->tile_coords[1] = 0;

    oapvd_stat_t stat = { 0 };

    oapvd_istream_t istream;
    file_istream_init(&istream, fp);

    // Get frame and tile sizes.
    int ret = decode_selective_single(decoder_id, &istream, sel_decode, 0, &stat);

    if(OAPV_FAILED(ret)) {
        printf("ERROR: Failed to get metadata (return code: %d)\n", ret);
        oapvd_delete(decoder_id);
        fclose(fp);
        oapv_mfree(sel_decode);
        return -1;
    }

    printf("Frame: %dx%d, Tile size: %dx%d\n",
           sel_decode->actual_frame_width, sel_decode->actual_frame_height,
           sel_decode->actual_tile_width, sel_decode->actual_tile_height);

    // Create output buffers if needed
    int          color_format = chroma_format_idc_to_color_format(sel_decode->chroma_format_idc);
    oapv_imgb_t *frame_buffer = create_frame_buffer(sel_decode->actual_frame_width, sel_decode->actual_frame_height, color_format, sel_decode->bit_depth);

    if(!frame_buffer) {
        printf("ERROR: Failed to allocate frame buffers\n");
        fclose(fp);
        oapv_mfree(sel_decode);
        return -1;
    }

    sel_decode->output_buffer = frame_buffer;

    // List all tile coords for this mip.
    int num_tile_col = (sel_decode->actual_frame_width + sel_decode->actual_tile_width - 1) / sel_decode->actual_tile_width;
    int num_tile_row = (sel_decode->actual_frame_height + sel_decode->actual_tile_height - 1) / sel_decode->actual_tile_height;
    int num_tiles = num_tile_col * num_tile_row;
    sel_decode->num_tiles = num_tiles;
    for(int j = 0; j < num_tile_row; j++) {
        for(int i = 0; i < num_tile_col; i++) {
            int tile_idx = j * num_tile_col + i;
            sel_decode->tile_coords[tile_idx * 2] = i;
            sel_decode->tile_coords[tile_idx * 2 + 1] = j;
        }
    }

    // Reset stat for actual decode (metadata call already updated it)
    stat.read = 0;

    // Run the decode
    ret = decode_selective_single(decoder_id, &istream, sel_decode, 0, &stat);

    if(OAPV_SUCCEEDED(ret)) {
        printf("SUCCESS: Decode completed\n");

        // Validation
        if(frame_buffer) {
           validate_quick(frame_buffer, num_tiles);
        }
    }
    else {
        printf("ERROR: Decode failed (return code: %d)\n", ret);
    }

    delete_frame_buffer(frame_buffer);
    oapv_mfree(sel_decode);
    fclose(fp);
    return 0;
}

// Decode multiple frames consecutively with the same decoder context.
// This is making sure no internal data from one frame interferes with decoding the next frame.
int run_multiframe_test(const char* framelist_file, int num_threads)
{
    printf("\n=== Test: Multi-frame ===\n");
    printf("Description: Decoding multiple consecutive frames to test decoder context.\n");

    // The input file contains one input frame and a mip level to load per line.
    FILE *fp = fopen(framelist_file, "rt");
    if(!fp) {
        printf("ERROR: Cannot open input file %s\n", framelist_file);
        return -1;
    }

    // Create initial decoder for metadata
    oapvd_cdesc_t cdesc = { 0 };
    cdesc.threads = num_threads;
    int err;

    oapvd_t decoder_id = oapvd_create(&cdesc, &err);
    if(decoder_id == NULL) {
        printf("ERROR: Failed to create decoder (error code: %d)\n", err);
        fclose(fp);
        return -1;
    }

    // Load the list of files.
    char buffer[1024];
    char input_file[1024];
    int  mip = 0;

    while(fgets(buffer, 1024, fp) != NULL) {        
        if(sscanf(buffer, "%s %d", input_file, &mip) == 2) {
            printf("Decoding Mip %d of %s\n", mip, input_file);
            decode_mip(input_file, mip, decoder_id);
        }
        else
        {
            printf("Error parsing frame list file. Expected format: filename mip_level\n");
        }
    }

    // Cleanup    
    oapvd_delete(decoder_id);
    fclose(fp);

    return 0;
}

// Run a single test configuration
// Function to run multi-mip tests
int run_multi_mip_test_config(const char* input_file, const test_config_t* config) {
    FILE* fp = fopen(input_file, "rb");
    if (!fp) {
        printf("ERROR: Cannot open input file %s\n", input_file);
        return -1;
    }

    // Create initial decoder for metadata
    oapvd_cdesc_t cdesc = {0};
    cdesc.threads = config->thread_counts[0];
    int err;
    oapvd_t decoder_id = oapvd_create(&cdesc, &err);
    if (decoder_id == NULL) {
        printf("ERROR: Failed to create decoder (error code: %d)\n", err);
        fclose(fp);
        return -1;
    }

    // Set up multi-mip decode structure
    oapv_multi_mip_decode_t multi_mip_decode = {0};
    multi_mip_decode.num_mips = config->num_mips;

    // Allocate mip requests array
    oapv_mip_request_t *mip_requests = (oapv_mip_request_t*)calloc(config->num_mips, sizeof(oapv_mip_request_t));
    if (!mip_requests) {
        printf("ERROR: Failed to allocate mip requests\n");
        oapvd_delete(decoder_id);
        fclose(fp);
        return -1;
    }
    multi_mip_decode.mip_requests = mip_requests;

    oapvd_istream_t istream;
    file_istream_init(&istream, fp);

    // Initialize mip requests from config
    int total_tiles = 0;
    for (int m = 0; m < config->num_mips; m++) {
        mip_requests[m].mip_level = config->mip_configs[m].mip_level;
        mip_requests[m].num_tiles = config->mip_configs[m].num_tiles;

        // Copy tile coordinates
        for (int i = 0; i < config->mip_configs[m].num_tiles * 2; i++) {
            mip_requests[m].tile_coords[i] = config->mip_configs[m].tile_coords[i];
        }

        mip_requests[m].output_buffer = NULL; // First pass for metadata only
        total_tiles += config->mip_configs[m].num_tiles;

        printf("  Mip %d: %d tiles\n", config->mip_configs[m].mip_level, config->mip_configs[m].num_tiles);
    }

    printf("Total tiles across all mips: %d\n", total_tiles);

    // Get metadata
    oapvd_stat_t stat = {0};
    int ret = oapvd_decode_selective_multi_mips(decoder_id, &istream, &multi_mip_decode, 0, &stat);

    if (OAPV_FAILED(ret)) {
        printf("ERROR: Failed to get metadata (return code: %d)\n", ret);
        free(mip_requests);
        oapvd_delete(decoder_id);
        fclose(fp);
        return -1;
    }

    // Print metadata and status for each mip
    printf("\nMip level metadata:\n");
    for (int m = 0; m < config->num_mips; m++) {
        oapv_mip_request_t *mip_req = &mip_requests[m];
        printf("  Mip %d: Status=%s", mip_req->mip_level,
               (mip_req->status == OAPV_OK) ? "OK" : "ERROR");

        if (mip_req->status == OAPV_OK) {
            printf(", Frame=%dx%d, Tile=%dx%d, Depth=%d\n",
                   mip_req->frame_width_mb_aligned, mip_req->frame_height_mb_aligned,
                   mip_req->tile_width_mb_aligned, mip_req->tile_height_mb_aligned,
                   mip_req->bit_depth);
        } else {
            printf(" (code=%d)\n", mip_req->status);
        }
    }

    // Create output buffers for valid mips if needed
    if (config->output_format != OUTPUT_NONE || config->validation_level == VALIDATE_FULL) {
        for (int m = 0; m < config->num_mips; m++) {
            oapv_mip_request_t *mip_req = &mip_requests[m];
            if (mip_req->status == OAPV_OK) {
                int color_format = chroma_format_idc_to_color_format(mip_req->chroma_format_idc);
                mip_req->output_buffer = create_frame_buffer(
                    mip_req->frame_width_mb_aligned, mip_req->frame_height_mb_aligned,
                    color_format, mip_req->bit_depth);

                if (!mip_req->output_buffer) {
                    printf("ERROR: Failed to allocate frame buffer for mip %d\n", mip_req->mip_level);
                    // Cleanup and return
                    for (int j = 0; j < m; j++) {
                        if (mip_requests[j].output_buffer) {
                            delete_frame_buffer(mip_requests[j].output_buffer);
                        }
                    }
                    free(mip_requests);
                    oapvd_delete(decoder_id);
                    fclose(fp);
                    return -1;
                }
            }
        }
    }

    // Run tests for each thread count
    for (int t = 0; config->thread_counts[t] != 0; t++) {
        int thread_count = config->thread_counts[t];

        // Update decoder thread count
        oapvd_delete(decoder_id);
        cdesc.threads = thread_count;
        decoder_id = oapvd_create(&cdesc, &err);
        if (!decoder_id) {
            printf("ERROR: Failed to recreate decoder with %d threads\n", thread_count);
            break;
        }

        printf("\n--- Testing with %d thread%s ---\n", thread_count, (thread_count > 1) ? "s" : "");

        // Reset stat for actual decode
        stat.read = 0;

        clock_t start_time = clock();

        // Run the multi-mip decode
        ret = oapvd_decode_selective_multi_mips(decoder_id, &istream, &multi_mip_decode, 0, &stat);

        clock_t end_time = clock();
        double elapsed_ms = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;

        if (OAPV_FAILED(ret)) {
            printf("ERROR: Multi-mip decode failed (return code: %d)\n", ret);
        } else {
            printf("SUCCESS: Multi-mip decode completed\n");
            printf("Timing: %.2f ms, Data read: %d bytes\n", elapsed_ms, stat.read);

            // Check individual mip statuses
            int valid_mips = 0;
            for (int m = 0; m < config->num_mips; m++) {
                if (mip_requests[m].status == OAPV_OK) {
                    valid_mips++;
                }
            }
            printf("Valid mips decoded: %d/%d\n", valid_mips, config->num_mips);
        }

        // VALIDATION - This was completely missing!
        if (ret == OAPV_OK) {
            for (int m = 0; m < config->num_mips; m++) {
                oapv_mip_request_t *mip_req = &mip_requests[m];
                if (mip_req->status == OAPV_OK && mip_req->output_buffer) {
                    printf("Validating mip %d output...\n", mip_req->mip_level);
                    if (config->validation_level == VALIDATE_QUICK) {
                        validate_quick(mip_req->output_buffer, mip_req->num_tiles);
                    } else if (config->validation_level == VALIDATE_FULL) {
                        validate_full(mip_req->output_buffer, mip_req->num_tiles);
                    }
                }
            }
        }

        if (config->measure_performance) {
            printf("Performance: %.2f tiles/ms, %.2f MB/s\n",
                   total_tiles / elapsed_ms,
                   (stat.read / (1024.0 * 1024.0)) / (elapsed_ms / 1000.0));
        }

        // Save output files
        if (config->output_format == OUTPUT_Y4M && ret == OAPV_OK) {
            for (int m = 0; m < config->num_mips; m++) {
                oapv_mip_request_t *mip_req = &mip_requests[m];
                if (mip_req->status == OAPV_OK && mip_req->output_buffer) {
                    char output_filename[256];
                    snprintf(output_filename, sizeof(output_filename),
                             "output/%s_mip%d_threads%d.y4m",
                             config->name, mip_req->mip_level, thread_count);

                    write_frame_y4m(output_filename, mip_req->output_buffer);
                    printf("Saved: %s\n", output_filename);

                    // Auto-generate PNG from Y4M
                    char png_filename[256];
                    char ffmpeg_cmd[512];
                    strcpy(png_filename, output_filename);

                    char *ext = strrchr(png_filename, '.');
                    if (ext) {
                        strcpy(ext, ".png");
                    }

                    snprintf(ffmpeg_cmd, sizeof(ffmpeg_cmd),
                        "ffmpeg -y -i \"%s\" \"%s\" 2>NUL", output_filename, png_filename);

                    int ffmpeg_result = system(ffmpeg_cmd);
                    if (ffmpeg_result == 0) {
                        printf("Saved PNG: %s\n", png_filename);
                    } else {
                        printf("Warning: Failed to convert Y4M to PNG for mip %d (ffmpeg not available or failed)\n", mip_req->mip_level);
                    }
                }
            }
        }
    }

    // Performance comparison test for "multimip_performance_comparison"
    if (strcmp(config->name, "multimip_performance_comparison") == 0) {
        printf("\n=== Performance Comparison: Multi-mip vs Individual ===\n");

        // Time the multi-mip approach (already done above)
        // double multi_mip_time = 0;
        // TODO: Extract timing from above loop

        // Time the individual mip approach
        clock_t individual_start = clock();
        for (int m = 0; m < config->num_mips; m++) {
            oapv_mip_request_t *mip_req = &mip_requests[m];
            if (mip_req->status == OAPV_OK) {
                oapv_selective_decode_t single_decode = {0};
                single_decode.mip_level = mip_req->mip_level;
                single_decode.num_tiles = mip_req->num_tiles;
                memcpy(single_decode.tile_coords, mip_req->tile_coords,
                       mip_req->num_tiles * 2 * sizeof(int));
                single_decode.output_buffer = mip_req->output_buffer;

                ret = decode_selective_single(decoder_id, &istream, &single_decode, 0, &stat);
                if (OAPV_FAILED(ret)) {
                    printf("ERROR: Individual decode failed for mip %d\n", mip_req->mip_level);
                }
            }
        }
        clock_t individual_end = clock();
        double individual_time = ((double)(individual_end - individual_start) / CLOCKS_PER_SEC) * 1000.0;

        printf("Individual approach time: %.2f ms\n", individual_time);
        // TODO: Compare with multi-mip time and show speedup
    }

    // Cleanup
    for (int m = 0; m < config->num_mips; m++) {
        if (mip_requests[m].output_buffer) {
            delete_frame_buffer(mip_requests[m].output_buffer);
        }
    }
    free(mip_requests);
    oapvd_delete(decoder_id);
    fclose(fp);

    return (ret == OAPV_OK) ? 0 : -1;
}

int run_test_config(const char* input_file, const test_config_t* config) {
    printf("\n=== Test: %s ===\n", config->name);
    printf("Description: %s\n", config->description);
    
    const char* type_str = (config->test_type == TEST_SINGLE_TILE) ? "Single" :
                          (config->test_type == TEST_MULTI_TILE) ? "Multi" : "Multi-Mip";

    if (config->test_type == TEST_MULTI_MIP) {
        int total_tiles = 0;
        for (int m = 0; m < config->num_mips; m++) {
            total_tiles += config->mip_configs[m].num_tiles;
        }
        printf("Type: %s, Mips: %d, Total tiles: %d\n", type_str, config->num_mips, total_tiles);
        return run_multi_mip_test_config(input_file, config);
    }

    int num_tiles = count_tiles_from_coords(config->tile_coords);
    printf("Type: %s, Mip: %d, Tiles: %d\n", type_str, config->mip_level, num_tiles);
    
    FILE* fp = fopen(input_file, "rb");
    if (!fp) {
        printf("ERROR: Cannot open input file %s\n", input_file);
        return -1;
    }
    
    // Create initial decoder for metadata
    oapvd_cdesc_t cdesc = {0};
    cdesc.threads = (config->test_type == TEST_SINGLE_TILE) ? 1 : config->thread_counts[0];
    int err;
    oapvd_t decoder_id = oapvd_create(&cdesc, &err);
    if (decoder_id == NULL) {
        printf("ERROR: Failed to create decoder (error code: %d)\n", err);
        fclose(fp);
        return -1;
    }
    
    // Set up selective decode structure
    oapv_selective_decode_t sel_decode = {0};
    sel_decode.mip_level = config->mip_level;
    sel_decode.num_tiles = num_tiles;
    
    // Copy tile coordinates
    for(int i = 0; i < num_tiles * 2; i++) {
        sel_decode.tile_coords[i] = config->tile_coords[i];
    }
    
    oapvd_stat_t stat = {0};

    oapvd_istream_t istream;
    file_istream_init(&istream, fp);
    
    // Get metadata
    int ret;
    if (config->test_type == TEST_SINGLE_TILE) {
        ret = decode_selective_single(decoder_id, &istream, &sel_decode, 0, &stat);
    } else {
        ret = decode_selective_single(decoder_id, &istream, &sel_decode, 0, &stat);
    }
    
    if (OAPV_FAILED(ret)) {
        printf("ERROR: Failed to get metadata (return code: %d)\n", ret);
        oapvd_delete(decoder_id);
        fclose(fp);
        return -1;
    }
    
    printf("Frame: %dx%d, Tile size: %dx%d\n",
           sel_decode.actual_frame_width, sel_decode.actual_frame_height,
           sel_decode.actual_tile_width, sel_decode.actual_tile_height);
    
    // Create output buffers if needed
    oapv_imgb_t *frame_buffer = NULL;
    if (config->output_format != OUTPUT_NONE || config->validation_level == VALIDATE_FULL) {
        int color_format = chroma_format_idc_to_color_format(sel_decode.chroma_format_idc);
        frame_buffer = create_frame_buffer(sel_decode.actual_frame_width, sel_decode.actual_frame_height, color_format, sel_decode.bit_depth);
        
        if (!frame_buffer) {
            printf("ERROR: Failed to allocate frame buffers\n");
            oapvd_delete(decoder_id);
            fclose(fp);
            return -1;
        }
        
        sel_decode.output_buffer = frame_buffer;
    }
    
    // Run tests for each thread count
    for(int t = 0; config->thread_counts[t] != 0; t++) {
        int thread_count = config->thread_counts[t];
        
        if (config->test_type == TEST_MULTI_TILE) {
            // Update decoder thread count
            oapvd_delete(decoder_id);
            cdesc.threads = thread_count;
            decoder_id = oapvd_create(&cdesc, &err);
        }
        
        printf("\n--- Testing with %d thread%s ---\n", thread_count, (thread_count > 1) ? "s" : "");

        // Reset stat for actual decode (metadata call already updated it)
        stat.read = 0;

        clock_t start_time = clock();

        // Run the decode
        if (config->test_type == TEST_SINGLE_TILE) {
            ret = decode_selective_single(decoder_id, &istream, &sel_decode, 0, &stat);
        } else {
            ret = decode_selective_single(decoder_id, &istream, &sel_decode, 0, &stat);
        }
        
        clock_t end_time = clock();
        
        if (OAPV_SUCCEEDED(ret)) {
            printf("SUCCESS: Decode completed\n");
            
            if (config->measure_performance) {
                double total_time_ms = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
                printf("Performance: %.2f ms", total_time_ms);
                if (num_tiles > 1) {
                    printf(", %.2f tiles/sec", num_tiles * 1000.0 / total_time_ms);
                }
                printf("\n");
                
                // Print detailed I/O statistics
                if (stat.read > 0) {
                    printf("I/O Statistics:\n");
                    printf("  Bytes read: %d (%.2f MB)\n", stat.read, stat.read / (1024.0 * 1024.0));
                    printf("  Bandwidth: %.2f MB/sec\n", (stat.read / (1024.0 * 1024.0)) / (total_time_ms / 1000.0));
                    printf("  Bytes per tile: %.0f\n", (double)stat.read / num_tiles);
                }
            }
            
            // Validation
            if (frame_buffer) {
                if (config->validation_level == VALIDATE_QUICK) {
                    validate_quick(frame_buffer, num_tiles);
                } else if (config->validation_level == VALIDATE_FULL) {
                    validate_full(frame_buffer, num_tiles);
                }
            }
        } else {
            printf("ERROR: Decode failed (return code: %d)\n", ret);
        }
        
        // Write output files (for multi-tile tests, include thread count to avoid contention)
        if (config->output_format != OUTPUT_NONE && frame_buffer) {
            char output_filename[256];
            
            if (config->output_format == OUTPUT_Y4M) {
                if (config->test_type == TEST_MULTI_TILE) {
                    snprintf(output_filename, sizeof(output_filename), "output/%s_%dthreads.y4m", config->name, thread_count);
                } else {
                    snprintf(output_filename, sizeof(output_filename), "output/%s.y4m", config->name);
                }
                write_frame_y4m(output_filename, frame_buffer);
                printf("Written Y4M: %s\n", output_filename);

                // Auto-generate PNG from Y4M
                char png_filename[512];
                char ffmpeg_cmd[1024];
                strcpy(png_filename, output_filename);
                // Replace .y4m extension with .png
                char *ext = strrchr(png_filename, '.');
                if (ext) strcpy(ext, ".png");

                snprintf(ffmpeg_cmd, sizeof(ffmpeg_cmd),
                    "ffmpeg -y -i \"%s\" \"%s\" 2>NUL", output_filename, png_filename);

                int ffmpeg_result = system(ffmpeg_cmd);
                if (ffmpeg_result == 0) {
                    printf("Written PNG: %s\n", png_filename);
                } else {
                    printf("Warning: Failed to convert Y4M to PNG (ffmpeg not available or failed)\n");
                }
            } else if (config->output_format == OUTPUT_RAW) {
                if (config->test_type == TEST_MULTI_TILE) {
                    snprintf(output_filename, sizeof(output_filename), "output/%s_%dthreads.raw", config->name, thread_count);
                } else {
                    snprintf(output_filename, sizeof(output_filename), "output/%s.raw", config->name);
                }
                if (write_frame_raw(output_filename, frame_buffer)) {
                    printf("Written RAW: %s\n", output_filename);
                }
            }
        }
    }
    
    // Cleanup
    delete_frame_buffer(frame_buffer);
    oapvd_delete(decoder_id);
    fclose(fp);
    
    return 0;
}

// Print available test configurations
void print_available_tests() {
    printf("Available test configurations:\n");
    for(int i = 0; i < num_test_configs; i++) {
        int num_tiles;
        if (test_configs[i].test_type == TEST_MULTI_MIP) {
            num_tiles = 0;
            for (int m = 0; m < test_configs[i].num_mips; m++) {
                num_tiles += test_configs[i].mip_configs[m].num_tiles;
            }
        } else {
            num_tiles = count_tiles_from_coords(test_configs[i].tile_coords);
        }
        printf("%2d. %-25s - %s (%d tiles)\n",
               i+1, test_configs[i].name, test_configs[i].description, num_tiles);
    }
}

const char *get_file_extension(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) { // No dot found, or dot is the first character (e.g., ".bashrc")
        return "";                // No extension
    }

    return dot + 1; // Return pointer to the character after the dot
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <apv_file> [test_number|test_name|all]\n", argv[0]);
        printf("\nDecoder test with data-driven configuration.\n");
        printf("Supports both single-tile and multi-tile selective decoding.\n\n");
        print_available_tests();
        printf("\nExamples:\n");
        printf("  %s test.apv1 3                        # Run test #3\n", argv[0]);
        printf("  %s test.apv1 multi_middle_6x4_scaling  # Run by name\n", argv[0]);
        printf("  %s test.apv1 all                       # Run all tests\n", argv[0]);
        return -1;
    }
    
    const char* input_file = argv[1];
    const char* test_selector = (argc >= 3) ? argv[2] : "all";

    printf("Decoder Test\n");
    printf("Input: %s\n", input_file);
    printf("Test selector: %s\n", test_selector);

    // Multi-frame test with a file containing a list of frames.
    if(strcmp(get_file_extension(input_file), "txt") == 0) {
        run_multiframe_test(input_file, 16);
        return 0;
    }

    if (strcmp(test_selector, "all") == 0) {
        // Run all tests
        printf("\nRunning all %d test configurations...\n", num_test_configs);
        for(int i = 0; i < num_test_configs; i++) {
            run_test_config(input_file, &test_configs[i]);
        }
    } else {
        // Check if test_selector is a pure number using strtol
        char *endptr;
        long test_num = strtol(test_selector, &endptr, 10);

        if (*endptr == '\0' && test_num > 0 && test_num <= num_test_configs) {
            // Entire string was a valid number - run by number
            run_test_config(input_file, &test_configs[test_num - 1]);
        } else {
            // Run by name
            int found = 0;
            for(int i = 0; i < num_test_configs; i++) {
                if (strcmp(test_configs[i].name, test_selector) == 0) {
                    run_test_config(input_file, &test_configs[i]);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                printf("ERROR: Test '%s' not found\n", test_selector);
                print_available_tests();
                return -1;
            }
        }
    }
    
    printf("\n=== Test Complete ===\n");
    return 0;
}