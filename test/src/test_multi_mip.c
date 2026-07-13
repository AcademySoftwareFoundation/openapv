#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../inc/oapv.h"
#include "../../src/oapv_port.h"

// Simple file-based I/O stream for testing
typedef struct {
    FILE *fp;
} file_istream_data_t;

static long file_istream_tell(oapvd_istream_t *stream) {
    file_istream_data_t *data = (file_istream_data_t*)stream->data;
    return ftell(data->fp);
}

static int file_istream_seek(oapvd_istream_t *stream, long offset, int origin) {
    file_istream_data_t *data = (file_istream_data_t*)stream->data;
    return fseek(data->fp, offset, origin);
}

static size_t file_istream_read(oapvd_istream_t *stream, void *buffer, size_t size, size_t count) {
    file_istream_data_t *data = (file_istream_data_t*)stream->data;
    return fread(buffer, size, count, data->fp);
}

// Test multi-mip decoding
int test_multi_mip_decode(const char *apv_file, int num_mips, int *mip_levels, int tiles_per_mip) {
    int ret = OAPV_OK;
    oapvd_t decoder = NULL;
    oapvd_cdesc_t cdesc;
    oapv_multi_mip_decode_t multi_mip;
    oapv_mip_request_t *mip_requests = NULL;
    oapvd_stat_t stat = {0};

    printf("Testing multi-mip decode with %d mip levels\n", num_mips);

    // Open APV file
    FILE *fp = fopen(apv_file, "rb");
    if(!fp) {
        printf("Failed to open file: %s\n", apv_file);
        return OAPV_ERR_INVALID_ARGUMENT;
    }

    // Create I/O stream
    file_istream_data_t stream_data = {fp};
    oapvd_istream_t istream = {
        .data = &stream_data,
        .tell = file_istream_tell,
        .seek = file_istream_seek,
        .read = file_istream_read
    };

    // Create decoder
    memset(&cdesc, 0, sizeof(cdesc));
    cdesc.threads = 8; // Use 8 threads

    decoder = oapvd_create(&cdesc, &ret);
    if(!decoder || ret != OAPV_OK) {
        printf("Failed to create decoder: %d\n", ret);
        fclose(fp);
        return ret;
    }

    // Allocate mip requests
    mip_requests = (oapv_mip_request_t*)calloc(num_mips, sizeof(oapv_mip_request_t));
    if(!mip_requests) {
        oapvd_delete(decoder);
        fclose(fp);
        return OAPV_ERR_OUT_OF_MEMORY;
    }

    // Setup mip requests
    for(int m = 0; m < num_mips; m++) {
        mip_requests[m].mip_level = mip_levels[m];
        mip_requests[m].num_tiles = tiles_per_mip;

        // Create a simple tile pattern (e.g., 2x2 block)
        if(tiles_per_mip == 4) {
            mip_requests[m].tile_coords[0] = 0; mip_requests[m].tile_coords[1] = 0;
            mip_requests[m].tile_coords[2] = 1; mip_requests[m].tile_coords[3] = 0;
            mip_requests[m].tile_coords[4] = 0; mip_requests[m].tile_coords[5] = 1;
            mip_requests[m].tile_coords[6] = 1; mip_requests[m].tile_coords[7] = 1;
        } else {
            // Single tile at origin
            mip_requests[m].tile_coords[0] = 0;
            mip_requests[m].tile_coords[1] = 0;
        }

        mip_requests[m].output_buffer = NULL; // First call for metadata
    }

    multi_mip.num_mips = num_mips;
    multi_mip.mip_requests = mip_requests;

    // First call: Get metadata
    printf("First call: Getting metadata for all mips...\n");
    ret = oapvd_decode_selective_multi_mips(decoder, &istream, &multi_mip, NULL, &stat);
    if(ret != OAPV_OK) {
        printf("Failed to get metadata: %d\n", ret);
        goto cleanup;
    }

    // Print metadata
    for(int m = 0; m < num_mips; m++) {
        printf("Mip %d metadata:\n", mip_levels[m]);
        printf("  Frame: %dx%d\n", mip_requests[m].frame_width_mb_aligned, mip_requests[m].frame_height_mb_aligned);
        printf("  Tiles: %dx%d\n", mip_requests[m].tile_width_mb_aligned, mip_requests[m].tile_height_mb_aligned);
        printf("  Bit depth: %d\n", mip_requests[m].bit_depth);
        printf("  Chroma format: %d\n", mip_requests[m].chroma_format);
    }

    // Allocate output buffers
    for(int m = 0; m < num_mips; m++) {
        oapv_imgb_t *imgb = (oapv_imgb_t*)calloc(1, sizeof(oapv_imgb_t));
        if(!imgb) {
            ret = OAPV_ERR_OUT_OF_MEMORY;
            goto cleanup;
        }

        // Setup image buffer for YUV422 10-bit
        imgb->cs = OAPV_CS_SET(OAPV_CF_YCBCR422, 10, 0);
        imgb->np = 3; // Y, U, V planes

        int width = mip_requests[m].frame_width_mb_aligned;
        int height = mip_requests[m].frame_height_mb_aligned;

        // Y plane
        imgb->w[0] = width;
        imgb->h[0] = height;
        imgb->s[0] = width * 2; // 2 bytes per pixel for 10-bit
        imgb->a[0] = calloc(height * imgb->s[0], 1);

        // U plane (half width for 422)
        imgb->w[1] = width / 2;
        imgb->h[1] = height;
        imgb->s[1] = (width / 2) * 2;
        imgb->a[1] = calloc(height * imgb->s[1], 1);

        // V plane (half width for 422)
        imgb->w[2] = width / 2;
        imgb->h[2] = height;
        imgb->s[2] = (width / 2) * 2;
        imgb->a[2] = calloc(height * imgb->s[2], 1);

        mip_requests[m].output_buffer = imgb;
    }

    // Second call: Decode tiles
    printf("\nSecond call: Decoding tiles...\n");
    istream.seek(&istream, 0, SEEK_SET); // Reset to beginning

    clock_t start = clock();
    ret = oapvd_decode_selective_multi_mips(decoder, &istream, &multi_mip, NULL, &stat);
    clock_t end = clock();

    if(ret != OAPV_OK) {
        printf("Failed to decode tiles: %d\n", ret);
        goto cleanup;
    }

    double decode_time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Decode completed in %.2f ms\n", decode_time_ms);
    printf("Total bytes read: %d\n", stat.read);

    // Validate decoded data (simple check for non-zero data)
    for(int m = 0; m < num_mips; m++) {
        oapv_imgb_t *imgb = mip_requests[m].output_buffer;
        if(imgb) {
            int has_data = 0;
            unsigned short *y_data = (unsigned short*)imgb->a[0];
            for(int i = 0; i < 100 && i < (imgb->w[0] * imgb->h[0]); i++) {
                if(y_data[i] != 0) {
                    has_data = 1;
                    break;
                }
            }
            printf("Mip %d: %s\n", mip_levels[m], has_data ? "Data decoded successfully" : "Warning: No data decoded");
        }
    }

cleanup:
    // Free output buffers
    for(int m = 0; m < num_mips; m++) {
        if(mip_requests[m].output_buffer) {
            oapv_imgb_t *imgb = mip_requests[m].output_buffer;
            for(int c = 0; c < 3; c++) {
                if(imgb->a[c]) free(imgb->a[c]);
            }
            free(imgb);
        }
    }

    if(mip_requests) free(mip_requests);
    if(decoder) oapvd_delete(decoder);
    fclose(fp);

    return ret;
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Usage: %s <apv_file>\n", argv[0]);
        return 1;
    }

    printf("OpenAPV Multi-Mip Decoder Test\n");
    printf("===============================\n\n");

    // Test 1: Single mip (backward compatibility)
    printf("Test 1: Single mip level (mip 0)\n");
    printf("---------------------------------\n");
    int single_mip[] = {0};
    int ret = test_multi_mip_decode(argv[1], 1, single_mip, 4);
    if(ret != OAPV_OK) {
        printf("Test 1 FAILED: %d\n", ret);
        return 1;
    }
    printf("Test 1 PASSED\n\n");

    // Test 2: Multiple mips
    printf("Test 2: Multiple mip levels (mips 0, 1, 2)\n");
    printf("-------------------------------------------\n");
    int multi_mips[] = {0, 1, 2};
    ret = test_multi_mip_decode(argv[1], 3, multi_mips, 1);
    if(ret != OAPV_OK) {
        printf("Test 2 FAILED: %d\n", ret);
        return 1;
    }
    printf("Test 2 PASSED\n\n");

    // Test 3: Non-sequential mips
    printf("Test 3: Non-sequential mips (mips 0, 2)\n");
    printf("----------------------------------------\n");
    int non_seq_mips[] = {0, 2};
    ret = test_multi_mip_decode(argv[1], 2, non_seq_mips, 2);
    if(ret != OAPV_OK) {
        printf("Test 3 FAILED: %d\n", ret);
        return 1;
    }
    printf("Test 3 PASSED\n\n");

    printf("All tests completed successfully!\n");
    return 0;
}