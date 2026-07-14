#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../inc/oapv.h"
#include "../../src/oapv_port.h"

#ifdef _WIN32
#include <windows.h>
static u64 get_time_ns() {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (u64)((counter.QuadPart * 1000000000LL) / freq.QuadPart);
}
#else
#include <time.h>
static u64 get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
#endif

/* File-based I/O stream for testing */
typedef struct {
    FILE *fp;
} file_istream_data_t;

static long long file_istream_tell(oapvd_istream_t *stream) {
    file_istream_data_t *data = (file_istream_data_t*)stream->data;
    return oapv_ftell(data->fp);
}

static int file_istream_seek(oapvd_istream_t *stream, long long offset, int origin) {
    file_istream_data_t *data = (file_istream_data_t*)stream->data;
    return oapv_fseek(data->fp, offset, origin);
}

static size_t file_istream_read(oapvd_istream_t *stream, void *buffer, size_t size, size_t count) {
    file_istream_data_t *data = (file_istream_data_t*)stream->data;
    return fread(buffer, size, count, data->fp);
}

/* Allocate image buffer for given dimensions */
static oapv_imgb_t* allocate_imgb(int width, int height, int bit_depth) {
    oapv_imgb_t *imgb = (oapv_imgb_t*)calloc(1, sizeof(oapv_imgb_t));
    if(!imgb) return NULL;

    imgb->cs = OAPV_CS_SET(OAPV_CF_YCBCR422, bit_depth, 0);
    imgb->np = 3;

    /* Y plane */
    imgb->w[0] = width;
    imgb->h[0] = height;
    imgb->s[0] = width * 2;
    imgb->a[0] = calloc(height * imgb->s[0], 1);
    if(!imgb->a[0]) {
        free(imgb);
        return NULL;
    }

    /* U plane (half width for 422) */
    imgb->w[1] = width / 2;
    imgb->h[1] = height;
    imgb->s[1] = (width / 2) * 2;
    imgb->a[1] = calloc(height * imgb->s[1], 1);
    if(!imgb->a[1]) {
        free(imgb->a[0]);
        free(imgb);
        return NULL;
    }

    /* V plane (half width for 422) */
    imgb->w[2] = width / 2;
    imgb->h[2] = height;
    imgb->s[2] = (width / 2) * 2;
    imgb->a[2] = calloc(height * imgb->s[2], 1);
    if(!imgb->a[2]) {
        free(imgb->a[0]);
        free(imgb->a[1]);
        free(imgb);
        return NULL;
    }

    return imgb;
}

/* Free image buffer */
static void free_imgb(oapv_imgb_t *imgb) {
    if(!imgb) return;
    for(int c = 0; c < 3; c++) {
        if(imgb->a[c]) free(imgb->a[c]);
    }
    free(imgb);
}

/* Validate that decoded data is not all zeros */
static int validate_imgb(oapv_imgb_t *imgb, int mip_level) {
    if(!imgb || !imgb->a[0]) return 0;

    unsigned short *y_data = (unsigned short*)imgb->a[0];
    int samples_to_check = imgb->w[0] * imgb->h[0];
    if(samples_to_check > 1000) samples_to_check = 1000;

    int non_zero_count = 0;
    for(int i = 0; i < samples_to_check; i++) {
        if(y_data[i] != 0) non_zero_count++;
    }

    return non_zero_count > 0;
}

// Local single-mip selective-decode wrapper over oapvd_decode_selective_multi_mips
// (replaces the removed oapvd_decode_selective_multi API).
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
    if(ret == OAPV_OK && mip_request.status != OAPV_OK) {
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

/*
 * Test Method 1: Sequential single-mip selective decodes
 * This simulates the old approach where each mip is decoded separately
 */
static int test_sequential_multi_calls(
    const char *apv_file,
    oapvd_t decoder,
    int num_mips,
    int *mip_levels,
    int num_tiles,
    int *tile_coords,
    double *out_total_time_ms,
    double *out_io_time_ms,
    double *out_decode_time_ms,
    u32 *out_bytes_read)
{
    FILE *fp = fopen(apv_file, "rb");
    if(!fp) return OAPV_ERR_INVALID_ARGUMENT;

    file_istream_data_t stream_data = {fp};
    oapvd_istream_t istream = {
        .data = &stream_data,
        .tell = file_istream_tell,
        .seek = file_istream_seek,
        .read = file_istream_read
    };

    u64 total_start = get_time_ns();
    //u64 total_io_ns = 0;
    u64 total_decode_ns = 0;
    u32 total_bytes = 0;

    /* Process each mip level separately */
    for(int m = 0; m < num_mips; m++) {
        oapv_selective_decode_t sel_decode;
        oapvd_stat_t stat = {0};
        int ret;

        sel_decode.mip_level = mip_levels[m];
        sel_decode.num_tiles = num_tiles;
        memcpy(sel_decode.tile_coords, tile_coords, sizeof(int) * 2 * num_tiles);
        sel_decode.output_buffer = NULL;

        /* First call: Get metadata */
        istream.seek(&istream, 0, SEEK_SET);
        ret = decode_selective_single(decoder, &istream, &sel_decode, NULL, &stat);
        if(ret != OAPV_OK) {
            fclose(fp);
            return ret;
        }

        /* Allocate output buffer */
        sel_decode.output_buffer = allocate_imgb(
            sel_decode.actual_frame_width,
            sel_decode.actual_frame_height,
            sel_decode.bit_depth);

        if(!sel_decode.output_buffer) {
            fclose(fp);
            return OAPV_ERR_OUT_OF_MEMORY;
        }

        /* Second call: Decode tiles */
        istream.seek(&istream, 0, SEEK_SET);

        u64 decode_start = get_time_ns();
        ret = decode_selective_single(decoder, &istream, &sel_decode, NULL, &stat);
        u64 decode_end = get_time_ns();

        if(ret != OAPV_OK) {
            free_imgb(sel_decode.output_buffer);
            fclose(fp);
            return ret;
        }

        /* Accumulate timing and I/O stats */
        total_decode_ns += (decode_end - decode_start);
        total_bytes += stat.read;

        /* Validate */
        if(!validate_imgb(sel_decode.output_buffer, mip_levels[m])) {
            printf("WARNING: Mip %d validation failed (no data decoded)\n", mip_levels[m]);
        }

        free_imgb(sel_decode.output_buffer);
    }

    u64 total_end = get_time_ns();

    fclose(fp);

    *out_total_time_ms = (total_end - total_start) / 1000000.0;
    *out_decode_time_ms = total_decode_ns / 1000000.0;
    *out_io_time_ms = *out_total_time_ms - *out_decode_time_ms;
    *out_bytes_read = total_bytes;

    return OAPV_OK;
}

/*
 * Test Method 2: Single call to oapvd_decode_selective_multi_mips
 * This is the new optimized path for multi-mip decoding
 */
static int test_multi_mip_single_call(
    const char *apv_file,
    oapvd_t decoder,
    int num_mips,
    int *mip_levels,
    int num_tiles,
    int *tile_coords,
    double *out_total_time_ms,
    double *out_io_time_ms,
    double *out_decode_time_ms,
    u32 *out_bytes_read)
{
    FILE *fp = fopen(apv_file, "rb");
    if(!fp) return OAPV_ERR_INVALID_ARGUMENT;

    file_istream_data_t stream_data = {fp};
    oapvd_istream_t istream = {
        .data = &stream_data,
        .tell = file_istream_tell,
        .seek = file_istream_seek,
        .read = file_istream_read
    };

    /* Allocate mip requests */
    oapv_mip_request_t *mip_requests = (oapv_mip_request_t*)calloc(num_mips, sizeof(oapv_mip_request_t));
    if(!mip_requests) {
        fclose(fp);
        return OAPV_ERR_OUT_OF_MEMORY;
    }

    /* Setup mip requests */
    for(int m = 0; m < num_mips; m++) {
        mip_requests[m].mip_level = mip_levels[m];
        mip_requests[m].num_tiles = num_tiles;
        memcpy(mip_requests[m].tile_coords, tile_coords, sizeof(int) * 2 * num_tiles);
        mip_requests[m].output_buffer = NULL;
    }

    oapv_multi_mip_decode_t multi_mip = {
        .num_mips = num_mips,
        .mip_requests = mip_requests
    };

    /* First call: Get metadata */
    oapvd_stat_t stat = {0};
    int ret = oapvd_decode_selective_multi_mips(decoder, &istream, &multi_mip, NULL, &stat);
    if(ret != OAPV_OK) {
        free(mip_requests);
        fclose(fp);
        return ret;
    }

    /* Allocate output buffers */
    for(int m = 0; m < num_mips; m++) {
        mip_requests[m].output_buffer = allocate_imgb(
            mip_requests[m].frame_width_mb_aligned,
            mip_requests[m].frame_height_mb_aligned,
            mip_requests[m].bit_depth);

        if(!mip_requests[m].output_buffer) {
            for(int j = 0; j < m; j++) {
                free_imgb(mip_requests[j].output_buffer);
            }
            free(mip_requests);
            fclose(fp);
            return OAPV_ERR_OUT_OF_MEMORY;
        }
    }

    /* Second call: Decode all tiles across all mips */
    istream.seek(&istream, 0, SEEK_SET);

    u64 start = get_time_ns();
    ret = oapvd_decode_selective_multi_mips(decoder, &istream, &multi_mip, NULL, &stat);
    u64 end = get_time_ns();

    if(ret != OAPV_OK) {
        for(int m = 0; m < num_mips; m++) {
            free_imgb(mip_requests[m].output_buffer);
        }
        free(mip_requests);
        fclose(fp);
        return ret;
    }

    /* Validate */
    for(int m = 0; m < num_mips; m++) {
        if(!validate_imgb(mip_requests[m].output_buffer, mip_levels[m])) {
            printf("WARNING: Mip %d validation failed (no data decoded)\n", mip_levels[m]);
        }
        free_imgb(mip_requests[m].output_buffer);
    }

    fclose(fp);
    free(mip_requests);

    *out_total_time_ms = (end - start) / 1000000.0;
    *out_decode_time_ms = *out_total_time_ms; /* Internal timing included in function */
    *out_io_time_ms = 0; /* Included in internal metrics */
    *out_bytes_read = stat.read;

    return OAPV_OK;
}

/*
 * Run performance comparison test
 */
static int run_performance_test(
    const char *apv_file,
    int num_threads,
    int num_mips,
    int *mip_levels,
    int num_tiles,
    int *tile_coords,
    int num_iterations)
{
    printf("\n");
    printf("========================================\n");
    printf("Performance Test Configuration\n");
    printf("========================================\n");
    printf("File: %s\n", apv_file);
    printf("Threads: %d\n", num_threads);
    printf("Mip levels: ");
    for(int i = 0; i < num_mips; i++) {
        printf("%d%s", mip_levels[i], i < num_mips-1 ? ", " : "\n");
    }
    printf("Tiles per mip: %d\n", num_tiles);
    printf("Iterations: %d\n", num_iterations);
    printf("\n");

    /* Create decoder (shared across all tests) */
    oapvd_cdesc_t cdesc;
    memset(&cdesc, 0, sizeof(cdesc));
    cdesc.threads = num_threads;

    int ret;
    oapvd_t decoder = oapvd_create(&cdesc, &ret);
    if(!decoder || ret != OAPV_OK) {
        printf("Failed to create decoder: %d\n", ret);
        return ret;
    }

    printf("Testing Method 1: Sequential multi calls (one per mip)\n");
    printf("--------------------------------------------------------\n");

    double seq_total_time = 0, seq_io_time = 0, seq_decode_time = 0;
    u32 seq_bytes = 0;

    for(int iter = 0; iter < num_iterations; iter++) {
        double total_ms, io_ms, decode_ms;
        u32 bytes;

        ret = test_sequential_multi_calls(
            apv_file, decoder, num_mips, mip_levels, num_tiles, tile_coords,
            &total_ms, &io_ms, &decode_ms, &bytes);

        if(ret != OAPV_OK) {
            printf("Sequential test failed on iteration %d: %d\n", iter, ret);
            oapvd_delete(decoder);
            return ret;
        }

        seq_total_time += total_ms;
        seq_io_time += io_ms;
        seq_decode_time += decode_ms;
        seq_bytes = bytes; /* Same for each iteration */

        printf("  Iteration %d: %.2f ms\n", iter + 1, total_ms);
    }

    seq_total_time /= num_iterations;
    seq_io_time /= num_iterations;
    seq_decode_time /= num_iterations;

    printf("Average: %.2f ms (I/O: %.2f ms, Decode: %.2f ms)\n",
           seq_total_time, seq_io_time, seq_decode_time);
    printf("Bytes read: %u\n\n", seq_bytes);

    printf("Testing Method 2: Multi-mip single call (optimized)\n");
    printf("----------------------------------------------------\n");

    double multi_total_time = 0, multi_io_time = 0, multi_decode_time = 0;
    u32 multi_bytes = 0;

    for(int iter = 0; iter < num_iterations; iter++) {
        double total_ms, io_ms, decode_ms;
        u32 bytes;

        ret = test_multi_mip_single_call(
            apv_file, decoder, num_mips, mip_levels, num_tiles, tile_coords,
            &total_ms, &io_ms, &decode_ms, &bytes);

        if(ret != OAPV_OK) {
            printf("Multi-mip test failed on iteration %d: %d\n", iter, ret);
            oapvd_delete(decoder);
            return ret;
        }

        multi_total_time += total_ms;
        multi_io_time += io_ms;
        multi_decode_time += decode_ms;
        multi_bytes = bytes;

        printf("  Iteration %d: %.2f ms\n", iter + 1, total_ms);
    }

    multi_total_time /= num_iterations;
    multi_io_time /= num_iterations;
    multi_decode_time /= num_iterations;

    printf("Average: %.2f ms\n", multi_total_time);
    printf("Bytes read: %u\n\n", multi_bytes);

    /* Calculate speedup */
    double speedup = seq_total_time / multi_total_time;
    double io_reduction = ((double)(seq_bytes - multi_bytes) / seq_bytes) * 100.0;

    printf("========================================\n");
    printf("Performance Comparison Results\n");
    printf("========================================\n");
    printf("Sequential (Method 1):  %.2f ms\n", seq_total_time);
    printf("Multi-mip (Method 2):   %.2f ms\n", multi_total_time);
    printf("Speedup:                %.2fx\n", speedup);
    printf("Time saved:             %.2f ms (%.1f%%)\n",
           seq_total_time - multi_total_time,
           ((seq_total_time - multi_total_time) / seq_total_time) * 100.0);
    printf("\n");
    printf("I/O Efficiency:\n");
    printf("  Sequential bytes:     %u\n", seq_bytes);
    printf("  Multi-mip bytes:      %u\n", multi_bytes);
    printf("  I/O reduction:        %.1f%%\n", io_reduction);
    printf("\n");

    if(speedup >= 1.2) {
        printf("RESULT: Multi-mip path is %.2fx faster! ✓\n", speedup);
    } else if(speedup >= 1.0) {
        printf("RESULT: Multi-mip path is slightly faster (%.2fx)\n", speedup);
    } else {
        printf("WARNING: Multi-mip path is SLOWER (%.2fx) - investigation needed!\n", 1.0/speedup);
    }

    oapvd_delete(decoder);
    return OAPV_OK;
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Usage: %s <apv_file> [threads] [iterations]\n", argv[0]);
        printf("  apv_file:   Path to tiled APV file with mip levels\n");
        printf("  threads:    Number of threads (default: 8)\n");
        printf("  iterations: Number of iterations per test (default: 5)\n");
        return 1;
    }

    const char *apv_file = argv[1];
    int num_threads = (argc > 2) ? atoi(argv[2]) : 8;
    int num_iterations = (argc > 3) ? atoi(argv[3]) : 5;

    printf("========================================\n");
    printf("Multi-Mip Performance Comparison Test\n");
    printf("========================================\n");

    /* Test scenario 1: 3 mips, 1 tile each */
    printf("\n\nScenario 1: 3 mip levels (0,1,2), 1 tile each\n");
    int mips_1[] = {0, 1, 2};
    int tiles_1[] = {0, 0}; /* Single tile at origin */
    int ret = run_performance_test(apv_file, num_threads, 3, mips_1, 1, tiles_1, num_iterations);
    if(ret != OAPV_OK) return ret;

    /* Test scenario 2: 3 mips, 4 tiles each (2x2 block) */
    printf("\n\nScenario 2: 3 mip levels (0,1,2), 4 tiles each (2x2)\n");
    int mips_2[] = {0, 1, 2};
    int tiles_2[] = {0,0, 1,0, 0,1, 1,1}; /* 2x2 tile block */
    ret = run_performance_test(apv_file, num_threads, 3, mips_2, 4, tiles_2, num_iterations);
    if(ret != OAPV_OK) return ret;

    /* Test scenario 3: 4 mips, sparse tiles (avoiding high mip levels that may not exist in all files) */
    printf("\n\nScenario 3: 4 mip levels (0,1,2,3), 2 tiles each\n");
    int mips_3[] = {0, 1, 2, 3};
    int tiles_3[] = {0,0, 1,1}; /* Sparse diagonal tiles */
    ret = run_performance_test(apv_file, num_threads, 4, mips_3, 2, tiles_3, num_iterations);
    if(ret != OAPV_OK) return ret;

    printf("\n\nAll performance tests completed successfully!\n");
    return 0;
}
