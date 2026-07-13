#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../inc/oapv.h"
#include "../../src/oapv_port.h"

/* File-based I/O stream */
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

/* Allocate image buffer */
static oapv_imgb_t* allocate_imgb(int width, int height) {
    oapv_imgb_t *imgb = (oapv_imgb_t*)calloc(1, sizeof(oapv_imgb_t));
    if(!imgb) return NULL;

    imgb->cs = OAPV_CS_SET(OAPV_CF_YCBCR422, 10, 0);
    imgb->np = 3;

    imgb->w[0] = width;
    imgb->h[0] = height;
    imgb->s[0] = width * 2;
    imgb->a[0] = calloc(height * imgb->s[0], 1);

    imgb->w[1] = width / 2;
    imgb->h[1] = height;
    imgb->s[1] = (width / 2) * 2;
    imgb->a[1] = calloc(height * imgb->s[1], 1);

    imgb->w[2] = width / 2;
    imgb->h[2] = height;
    imgb->s[2] = (width / 2) * 2;
    imgb->a[2] = calloc(height * imgb->s[2], 1);

    if(!imgb->a[0] || !imgb->a[1] || !imgb->a[2]) {
        if(imgb->a[0]) free(imgb->a[0]);
        if(imgb->a[1]) free(imgb->a[1]);
        if(imgb->a[2]) free(imgb->a[2]);
        free(imgb);
        return NULL;
    }

    return imgb;
}

static void free_imgb(oapv_imgb_t *imgb) {
    if(!imgb) return;
    for(int c = 0; c < 3; c++) {
        if(imgb->a[c]) free(imgb->a[c]);
    }
    free(imgb);
}

/*
 * Test 1: Non-existent mip level mixed with valid mips
 * Expected: Valid mips decode successfully, invalid mip gets OAPV_ERR_NOT_FOUND
 */
static int test_nonexistent_mip(const char *apv_file) {
    printf("\n=== Test 1: Non-existent Mip Level ===\n");
    printf("Testing with mips: 0 (valid), 1 (valid), 99 (invalid)\n");

    FILE *fp = fopen(apv_file, "rb");
    if(!fp) {
        printf("FAILED: Cannot open file\n");
        return -1;
    }

    file_istream_data_t stream_data = {fp};
    oapvd_istream_t istream = {
        .data = &stream_data,
        .tell = file_istream_tell,
        .seek = file_istream_seek,
        .read = file_istream_read
    };

    oapvd_cdesc_t cdesc = {0};
    cdesc.threads = 4;
    int ret;
    oapvd_t decoder = oapvd_create(&cdesc, &ret);
    if(!decoder) {
        fclose(fp);
        return -1;
    }

    oapv_mip_request_t mip_requests[3] = {0};

    /* Mip 0: valid */
    mip_requests[0].mip_level = 0;
    mip_requests[0].num_tiles = 1;
    mip_requests[0].tile_coords[0] = 0;
    mip_requests[0].tile_coords[1] = 0;
    mip_requests[0].output_buffer = NULL;

    /* Mip 1: valid */
    mip_requests[1].mip_level = 1;
    mip_requests[1].num_tiles = 1;
    mip_requests[1].tile_coords[0] = 0;
    mip_requests[1].tile_coords[1] = 0;
    mip_requests[1].output_buffer = NULL;

    /* Mip 99: should not exist */
    mip_requests[2].mip_level = 99;
    mip_requests[2].num_tiles = 1;
    mip_requests[2].tile_coords[0] = 0;
    mip_requests[2].tile_coords[1] = 0;
    mip_requests[2].output_buffer = NULL;

    oapv_multi_mip_decode_t multi_mip = {
        .num_mips = 3,
        .mip_requests = mip_requests
    };

    /* First call: Get metadata */
    ret = oapvd_decode_selective_multi_mips(decoder, &istream, &multi_mip, NULL, NULL);

    printf("Results:\n");
    printf("  Mip 0 status: %d (%s)\n", mip_requests[0].status,
           mip_requests[0].status == OAPV_OK ? "OK" : "ERROR");
    printf("  Mip 1 status: %d (%s)\n", mip_requests[1].status,
           mip_requests[1].status == OAPV_OK ? "OK" : "ERROR");
    printf("  Mip 99 status: %d (%s)\n", mip_requests[2].status,
           mip_requests[2].status == OAPV_ERR_NOT_FOUND ? "NOT_FOUND (correct)" : "UNEXPECTED");

    int test_passed = (mip_requests[0].status == OAPV_OK) &&
                      (mip_requests[1].status == OAPV_OK) &&
                      (mip_requests[2].status == OAPV_ERR_NOT_FOUND);

    if(test_passed) {
        printf("PASSED: Invalid mip marked as NOT_FOUND, valid mips succeeded\n");
    } else {
        printf("FAILED: Unexpected status codes\n");
    }

    oapvd_delete(decoder);
    fclose(fp);
    return test_passed ? 0 : -1;
}

/*
 * Test 2: Invalid tile coordinates (out of bounds)
 * Expected: Valid tiles decode, invalid tiles fail gracefully
 */
static int test_invalid_tile_coordinates(const char *apv_file) {
    printf("\n=== Test 2: Invalid Tile Coordinates ===\n");
    printf("Testing with valid tile (0,0) and invalid tile (999,999)\n");

    FILE *fp = fopen(apv_file, "rb");
    if(!fp) {
        printf("FAILED: Cannot open file\n");
        return -1;
    }

    file_istream_data_t stream_data = {fp};
    oapvd_istream_t istream = {
        .data = &stream_data,
        .tell = file_istream_tell,
        .seek = file_istream_seek,
        .read = file_istream_read
    };

    oapvd_cdesc_t cdesc = {0};
    cdesc.threads = 4;
    int ret;
    oapvd_t decoder = oapvd_create(&cdesc, &ret);
    if(!decoder) {
        fclose(fp);
        return -1;
    }

    oapv_mip_request_t mip_request = {0};
    mip_request.mip_level = 0;
    mip_request.num_tiles = 2;
    mip_request.tile_coords[0] = 0;   /* Valid tile */
    mip_request.tile_coords[1] = 0;
    mip_request.tile_coords[2] = 999; /* Invalid tile - way out of bounds */
    mip_request.tile_coords[3] = 999;
    mip_request.output_buffer = NULL;

    oapv_multi_mip_decode_t multi_mip = {
        .num_mips = 1,
        .mip_requests = &mip_request
    };

    /* Get metadata */
    ret = oapvd_decode_selective_multi_mips(decoder, &istream, &multi_mip, NULL, NULL);

    if(mip_request.status == OAPV_OK) {
        /* Allocate output buffer */
        mip_request.output_buffer = allocate_imgb(
            mip_request.frame_width_mb_aligned,
            mip_request.frame_height_mb_aligned);

        if(mip_request.output_buffer) {
            /* Try to decode */
            istream.seek(&istream, 0, SEEK_SET);
            ret = oapvd_decode_selective_multi_mips(decoder, &istream, &multi_mip, NULL, NULL);

            printf("Decode returned: %d\n", ret);
            printf("Per-mip status: %d\n", mip_request.status);

            if(mip_request.status == OAPV_ERR_INVALID_ARGUMENT) {
                printf("PASSED: Invalid tile coordinates correctly rejected with INVALID_ARGUMENT\n");
                free_imgb(mip_request.output_buffer);
                oapvd_delete(decoder);
                fclose(fp);
                return 0;
            } else {
                printf("FAILED: Expected INVALID_ARGUMENT (%d) but got %d\n",
                       OAPV_ERR_INVALID_ARGUMENT, mip_request.status);
                free_imgb(mip_request.output_buffer);
                oapvd_delete(decoder);
                fclose(fp);
                return -1;
            }
        }
    }

    oapvd_delete(decoder);
    fclose(fp);
    return -1; /* Failed if we get here */
}

/*
 * Test 3: All mips invalid
 * Expected: Function returns error, all statuses set appropriately
 */
static int test_all_invalid_mips(const char *apv_file) {
    printf("\n=== Test 3: All Invalid Mip Levels ===\n");
    printf("Testing with mips: 50, 60, 70 (all should be invalid)\n");

    FILE *fp = fopen(apv_file, "rb");
    if(!fp) {
        printf("FAILED: Cannot open file\n");
        return -1;
    }

    file_istream_data_t stream_data = {fp};
    oapvd_istream_t istream = {
        .data = &stream_data,
        .tell = file_istream_tell,
        .seek = file_istream_seek,
        .read = file_istream_read
    };

    oapvd_cdesc_t cdesc = {0};
    cdesc.threads = 4;
    int ret;
    oapvd_t decoder = oapvd_create(&cdesc, &ret);
    if(!decoder) {
        fclose(fp);
        return -1;
    }

    oapv_mip_request_t mip_requests[3] = {0};

    for(int i = 0; i < 3; i++) {
        mip_requests[i].mip_level = 50 + i * 10;
        mip_requests[i].num_tiles = 1;
        mip_requests[i].tile_coords[0] = 0;
        mip_requests[i].tile_coords[1] = 0;
        mip_requests[i].output_buffer = NULL;
    }

    oapv_multi_mip_decode_t multi_mip = {
        .num_mips = 3,
        .mip_requests = mip_requests
    };

    ret = oapvd_decode_selective_multi_mips(decoder, &istream, &multi_mip, NULL, NULL);

    printf("Results:\n");
    int all_not_found = 1;
    for(int i = 0; i < 3; i++) {
        printf("  Mip %d status: %d (%s)\n",
               mip_requests[i].mip_level,
               mip_requests[i].status,
               mip_requests[i].status == OAPV_ERR_NOT_FOUND ? "NOT_FOUND" : "UNEXPECTED");
        if(mip_requests[i].status != OAPV_ERR_NOT_FOUND) {
            all_not_found = 0;
        }
    }

    if(all_not_found) {
        printf("PASSED: All invalid mips correctly marked as NOT_FOUND\n");
    } else {
        printf("FAILED: Some mips not marked correctly\n");
    }

    oapvd_delete(decoder);
    fclose(fp);
    return all_not_found ? 0 : -1;
}

/*
 * Test 4: Partial success - mix of valid and invalid
 * Verify that valid mips can decode even when others fail
 */
static int test_partial_success(const char *apv_file) {
    printf("\n=== Test 4: Partial Success Scenario ===\n");
    printf("Testing decode with mix of valid (0,1) and invalid (50) mips\n");

    FILE *fp = fopen(apv_file, "rb");
    if(!fp) {
        printf("FAILED: Cannot open file\n");
        return -1;
    }

    file_istream_data_t stream_data = {fp};
    oapvd_istream_t istream = {
        .data = &stream_data,
        .tell = file_istream_tell,
        .seek = file_istream_seek,
        .read = file_istream_read
    };

    oapvd_cdesc_t cdesc = {0};
    cdesc.threads = 4;
    int ret;
    oapvd_t decoder = oapvd_create(&cdesc, &ret);
    if(!decoder) {
        fclose(fp);
        return -1;
    }

    oapv_mip_request_t mip_requests[3] = {0};

    mip_requests[0].mip_level = 0;
    mip_requests[0].num_tiles = 1;
    mip_requests[0].tile_coords[0] = 0;
    mip_requests[0].tile_coords[1] = 0;

    mip_requests[1].mip_level = 50; /* Invalid */
    mip_requests[1].num_tiles = 1;
    mip_requests[1].tile_coords[0] = 0;
    mip_requests[1].tile_coords[1] = 0;

    mip_requests[2].mip_level = 1;
    mip_requests[2].num_tiles = 1;
    mip_requests[2].tile_coords[0] = 0;
    mip_requests[2].tile_coords[1] = 0;

    oapv_multi_mip_decode_t multi_mip = {
        .num_mips = 3,
        .mip_requests = mip_requests
    };

    /* Get metadata */
    ret = oapvd_decode_selective_multi_mips(decoder, &istream, &multi_mip, NULL, NULL);

    printf("Metadata call results:\n");
    for(int i = 0; i < 3; i++) {
        printf("  Mip %d status: %d\n", mip_requests[i].mip_level, mip_requests[i].status);
    }

    /* Allocate buffers for valid mips only */
    for(int i = 0; i < 3; i++) {
        if(mip_requests[i].status == OAPV_OK) {
            mip_requests[i].output_buffer = allocate_imgb(
                mip_requests[i].frame_width_mb_aligned,
                mip_requests[i].frame_height_mb_aligned);
        }
    }

    /* Decode */
    istream.seek(&istream, 0, SEEK_SET);
    ret = oapvd_decode_selective_multi_mips(decoder, &istream, &multi_mip, NULL, NULL);

    printf("\nDecode call results:\n");
    int mip0_ok = (mip_requests[0].status == OAPV_OK);
    int mip50_not_found = (mip_requests[1].status == OAPV_ERR_NOT_FOUND);
    int mip1_ok = (mip_requests[2].status == OAPV_OK);

    for(int i = 0; i < 3; i++) {
        printf("  Mip %d status: %d (%s)\n",
               mip_requests[i].mip_level,
               mip_requests[i].status,
               mip_requests[i].status == OAPV_OK ? "OK" :
               mip_requests[i].status == OAPV_ERR_NOT_FOUND ? "NOT_FOUND" : "ERROR");
    }

    /* Verify valid mips decoded successfully */
    if(mip0_ok && mip50_not_found && mip1_ok) {
        printf("PASSED: Valid mips decoded successfully despite invalid mip in request\n");
    } else {
        printf("FAILED: Unexpected statuses\n");
    }

    /* Cleanup */
    for(int i = 0; i < 3; i++) {
        free_imgb(mip_requests[i].output_buffer);
    }

    oapvd_delete(decoder);
    fclose(fp);
    return (mip0_ok && mip50_not_found && mip1_ok) ? 0 : -1;
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Usage: %s <apv_file>\n", argv[0]);
        printf("  Tests error handling for invalid mip levels and tile coordinates\n");
        return 1;
    }

    const char *apv_file = argv[1];

    printf("========================================\n");
    printf("Multi-Mip Error Handling Tests\n");
    printf("========================================\n");
    printf("File: %s\n", apv_file);

    int failed = 0;

    if(test_nonexistent_mip(apv_file) != 0) failed++;
    if(test_invalid_tile_coordinates(apv_file) != 0) failed++;
    if(test_all_invalid_mips(apv_file) != 0) failed++;
    if(test_partial_success(apv_file) != 0) failed++;

    printf("\n========================================\n");
    if(failed == 0) {
        printf("All error handling tests PASSED!\n");
        printf("========================================\n");
        return 0;
    } else {
        printf("%d test(s) FAILED\n", failed);
        printf("========================================\n");
        return 1;
    }
}
