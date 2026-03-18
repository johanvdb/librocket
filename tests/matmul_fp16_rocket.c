/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * FP16 Matrix Multiplication Test for Rocket NPU Driver
 * 
 * Ported from mtx512/rk3588-npu to use mainline Rocket driver
 * Original: https://github.com/mtx512/rk3588-npu (GPL-3.0)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "rocket_interface.h"
#include "npu_matmul.h"

#define MAX_M 384 
#define MAX_K 4096 
#define MAX_N 4096 

/* Test matrices */
static _Float16 matrixA[(MAX_M*MAX_K)];
static _Float16 matrixB[(MAX_N*MAX_K)];
static float expected_result[MAX_M*MAX_N];
static uint64_t npu_regs[112];

/* CPU reference implementation */
static void matmul_fp32_cpu(int m, int k, int n, _Float16 *src0, _Float16 *src1, float *dst)
{
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0;
            for (int l = 0; l < k; l++) {
                sum += src0[i*k + l] * src1[j*k + l];
            }
            dst[i*n + j] = sum;
        }
    }
}

static float rand_float(void)
{
    return rand() / (float)RAND_MAX;
}

int main(int argc, char **argv)
{
    unsigned int M = 0, K = 0, N = 0;
    struct rocket_ctx ctx;
    struct rocket_bo regcmd_bo, input_bo, weights_bo, output_bo;
    int ret;

    if (argc != 4) {
        printf("Usage: %s <M> <K> <N>\n", argv[0]);
        printf("  M: rows of matrix A (multiple of 4, max %d)\n", MAX_M);
        printf("  K: cols of A / rows of B (multiple of 32, max %d)\n", MAX_K);
        printf("  N: cols of matrix B (multiple of 16, max %d)\n", MAX_N);
        return 1;
    }

    M = atoi(argv[1]);
    K = atoi(argv[2]);
    N = atoi(argv[3]);

    /* Validate dimensions */
    if ((M <= 0) || (M > MAX_M) || (((M % 4) != 0) && (M != 1))) {
        printf("M [%d] must be 1 or multiple of 4, max %d\n", M, MAX_M);
        return 1;
    }
    if ((K <= 0) || (K > MAX_K) || ((K % 32) != 0)) {
        printf("K [%d] must be multiple of 32, max %d\n", K, MAX_K);
        return 1;
    }
    if ((N <= 0) || (N > MAX_N) || ((N % 16) != 0)) {
        printf("N [%d] must be multiple of 16, max %d\n", N, MAX_N);
        return 1;
    }

    printf("Testing matmul: [%d x %d] x [%d x %d] = [%d x %d]\n", M, K, K, N, M, N);

    /* Open Rocket NPU */
    ret = rocket_open(&ctx);
    if (ret < 0) {
        printf("Failed to open Rocket NPU: %d\n", ret);
        return 1;
    }
    printf("Opened /dev/accel/accel0 successfully\n");

    /* Allocate buffers */
    ret = rocket_bo_create(&ctx, &regcmd_bo, sizeof(npu_regs));
    if (ret < 0) goto err_close;

    ret = rocket_bo_create(&ctx, &input_bo, M * K * sizeof(_Float16));
    if (ret < 0) goto err_free_regcmd;

    ret = rocket_bo_create(&ctx, &weights_bo, N * K * sizeof(_Float16));
    if (ret < 0) goto err_free_input;

    ret = rocket_bo_create(&ctx, &output_bo, M * N * sizeof(float));
    if (ret < 0) goto err_free_weights;

    printf("Buffers allocated:\n");
    printf("  regcmd:  dma=0x%lx size=%zu\n", regcmd_bo.dma_addr, regcmd_bo.size);
    printf("  input:   dma=0x%lx size=%zu\n", input_bo.dma_addr, input_bo.size);
    printf("  weights: dma=0x%lx size=%zu\n", weights_bo.dma_addr, weights_bo.size);
    printf("  output:  dma=0x%lx size=%zu\n", output_bo.dma_addr, output_bo.size);

    /* Generate matmul commands */
    matmul_params_t params = {
        .m = M,
        .k = K,
        .n = N,
        .input_dma = input_bo.dma_addr,
        .weights_dma = weights_bo.dma_addr,
        .output_dma = output_bo.dma_addr,
        .tasks = npu_regs,
        .fp32tofp16 = 0,
    };

    ret = gen_matmul_fp16(&params);
    if (ret != 0) {
        printf("gen_matmul_fp16 failed: %d\n", ret);
        goto err_free_output;
    }
    printf("Generated %zu register commands\n", sizeof(npu_regs) / sizeof(uint64_t));

    /* Copy commands to DMA buffer */
    memcpy(regcmd_bo.map, npu_regs, sizeof(npu_regs));

    /* Generate random test data */
    srand(time(NULL));
    for (int i = 0; i < M * K; i++) {
        matrixA[i] = (_Float16)((int)(10.0 * rand_float()));
    }
    for (int i = 0; i < N * K; i++) {
        matrixB[i] = (_Float16)((int)(10.0 * rand_float()));
    }

    /* Layout weights for NPU */
    _Float16 *weights_fp16 = (_Float16 *)weights_bo.map;
    memset(weights_fp16, 0, N * K * sizeof(_Float16));
    for (int n = 1; n <= N; n++) {
        for (int k = 1; k <= K; k++) {
            weights_fp16[weight_fp16(K, n, k)] = matrixB[((n-1)*K) + (k-1)];
        }
    }

    /* Layout input for NPU */
    _Float16 *input_fp16 = (_Float16 *)input_bo.map;
    memset(input_fp16, 0, M * K * sizeof(_Float16));
    for (int m = 1; m <= M; m++) {
        for (int k = 1; k <= K; k++) {
            input_fp16[feature_data(K, M, 1, 8, k, m, 1)] = matrixA[((m-1)*K) + (k-1)];
        }
    }

    /* Clear output */
    memset(output_bo.map, 0, M * N * sizeof(float));

    /* Compute CPU reference */
    matmul_fp32_cpu(M, K, N, matrixA, matrixB, expected_result);

    /* Sync buffers to device */
    rocket_bo_fini(&ctx, &regcmd_bo);
    rocket_bo_fini(&ctx, &input_bo);
    rocket_bo_fini(&ctx, &weights_bo);
    rocket_bo_fini(&ctx, &output_bo);

    /* Submit to NPU */
    printf("Submitting to NPU...\n");
    struct rocket_bo *in_bos[] = { &input_bo, &weights_bo, &regcmd_bo };
    struct rocket_bo *out_bos[] = { &output_bo };

    /* Calculate regcmd_count - subtract PC header/footer commands */
    uint32_t regcmd_count = sizeof(npu_regs) / sizeof(uint64_t) - 8;

    ret = rocket_submit(&ctx, &regcmd_bo, regcmd_count, in_bos, 3, out_bos, 1);
    if (ret < 0) {
        printf("rocket_submit failed: %d\n", ret);
        goto err_free_output;
    }
    printf("NPU job submitted\n");

    /* Wait for result and sync back */
    ret = rocket_bo_prep(&ctx, &output_bo, 5000000000LL);  /* 5 second timeout */
    if (ret < 0) {
        printf("rocket_bo_prep (wait) failed: %d\n", ret);
        goto err_free_output;
    }

    /* Verify results */
    printf("Verifying results...\n");
    float *output_data = (float *)output_bo.map;
    int errors = 0;
    for (int m = 1; m <= M; m++) {
        for (int n = 1; n < N; n++) {
            float actual = output_data[feature_data(N, M, 1, 4, n, m, 1)];
            float expected = expected_result[((m-1)*N) + (n-1)];
            if (actual != expected) {
                if (errors < 10) {
                    printf("MISMATCH m=%d n=%d: expected=%f actual=%f\n",
                           m, n, expected, actual);
                }
                errors++;
            }
        }
    }

    if (errors == 0) {
        printf("SUCCESS: [%d x %d] x [%d x %d] matmul verified!\n", M, K, K, N);
        ret = 0;
    } else {
        printf("FAILED: %d mismatches out of %d values\n", errors, M * N);
        ret = 1;
    }

err_free_output:
    rocket_bo_destroy(&ctx, &output_bo);
err_free_weights:
    rocket_bo_destroy(&ctx, &weights_bo);
err_free_input:
    rocket_bo_destroy(&ctx, &input_bo);
err_free_regcmd:
    rocket_bo_destroy(&ctx, &regcmd_bo);
err_close:
    rocket_close(&ctx);
    return ret;
}

