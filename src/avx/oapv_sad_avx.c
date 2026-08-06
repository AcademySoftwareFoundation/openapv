/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * - Neither the name of the copyright owner, nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "oapv_sad_avx.h"

#if X86_SSE

/* SAD ***********************************************************************/
/* SSD ***********************************************************************/
static s64 ssd_16b_avx_8x8(int w, int h, void* src1, void* src2, int s_src1, int s_src2)
{
    s16* s1 = (s16*)src1;
    s16* s2 = (s16*)src2;
    __m256i s1_vector, s2_vector, diff_vector, sq_vector1, sq_vector2;
    s64 sum_arr[4];
    // Because we are working with 16 elements at a time, stride is multiplied by 2.
    s16 s1_stride = 2 * s_src1;
    s16 s2_stride = 2 * s_src2;
    s64 ssd = 0;
    { // Row 0 and Row 1
        // Load Row 0 and Row 1 data into registers.
        s1_vector = _mm256_loadu_si256((const __m256i*)(s1));
        s1 += s1_stride;
        s2_vector = _mm256_loadu_si256((const __m256i*)(s2));
        s2 += s2_stride;
        // Calculate squared difference between two rows.
        diff_vector = _mm256_sub_epi16(s1_vector, s2_vector);
        sq_vector1 = _mm256_madd_epi16(diff_vector, diff_vector);
    }
    { // Row 2 and Row 3
        s1_vector = _mm256_loadu_si256((const __m256i*)(s1));
        s1 += s1_stride;
        s2_vector = _mm256_loadu_si256((const __m256i*)(s2));
        s2 += s2_stride;
        diff_vector = _mm256_sub_epi16(s1_vector, s2_vector);
        sq_vector2 = _mm256_madd_epi16(diff_vector, diff_vector);
    }
    // Add squared differences to running total.
    __m256i sum = _mm256_add_epi32(sq_vector1, sq_vector2);
    { // Row 4 and Row 5
        s1_vector = _mm256_loadu_si256((const __m256i*)(s1));
        s1 += s1_stride;
        s2_vector = _mm256_loadu_si256((const __m256i*)(s2));
        s2 += s2_stride;
        diff_vector = _mm256_sub_epi16(s1_vector, s2_vector);
        sq_vector2 = _mm256_madd_epi16(diff_vector, diff_vector);
        sum = _mm256_add_epi32(sum, sq_vector2);
    }
    { // Row 6 and Row 7
        s1_vector = _mm256_loadu_si256((const __m256i*)(s1));
        s2_vector = _mm256_loadu_si256((const __m256i*)(s2));
        diff_vector = _mm256_sub_epi16(s1_vector, s2_vector);
        sq_vector2 = _mm256_madd_epi16(diff_vector, diff_vector);
        sum = _mm256_add_epi32(sum, sq_vector2);
    }
    // Convert 16-bit integers to 32-bit integers for summation.
    __m128i sum_low = _mm256_extracti128_si256(sum, 0);
    __m128i sum_high = _mm256_extracti128_si256(sum, 1);
    __m256i sum_low_64 = _mm256_cvtepi32_epi64(sum_low);
    __m256i sum_high_64 = _mm256_cvtepi32_epi64(sum_high);
    // Sum up all the values in the array to get final SSD value.
    sum = _mm256_add_epi64(sum_low_64, sum_high_64);
    _mm256_storeu_si256((__m256i*)sum_arr, sum); // store in array for summation.
    ssd = sum_arr[0] + sum_arr[1] + sum_arr[2] + sum_arr[3];
    return ssd;
}

const oapv_fn_ssd_t oapv_tbl_fn_ssd_16b_avx[2] =
{
    ssd_16b_avx_8x8,
    NULL
};

/* DIFF ***********************************************************************/
#endif