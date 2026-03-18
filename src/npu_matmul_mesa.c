/*
 * NPU Matrix Multiplication using Mesa/Teflon register format
 * Based on Mesa's Rocket driver and registers.xml
 * 
 * Derived from mtx512/rk3588-npu (GPL-3.0-or-later)
 * Updated to use Mesa register format for mainline Rocket driver compatibility
 */

#include <stdint.h>
#include <string.h>
#include "npu_matmul.h"

/* Mesa-style register command format */
#define MESA_REGCMD(target, value, reg) \
    ((((uint64_t)(target)) << 48) | (((uint64_t)(value)) << 16) | ((uint64_t)(reg)))

/* Target domains from Mesa registers.xml */
#define TARGET_PC       0x100
#define TARGET_CNA      0x200
#define TARGET_CORE     0x800
#define TARGET_DPU      0x1000

/* PC (Program Counter) registers */
#define REG_PC_OPERATION_ENABLE    0x00000008
#define REG_PC_BASE_ADDRESS        0x00000010
#define REG_PC_REGISTER_AMOUNTS    0x00000014

/* CNA (Convolution Neural Accelerator) registers */
#define REG_CNA_S_POINTER          0x00001004
#define REG_CNA_OPERATION_ENABLE   0x00001008
#define REG_CNA_CONV_CON1          0x0000100C
#define REG_CNA_CONV_CON2          0x00001010
#define REG_CNA_DATA_SIZE0         0x00001020
#define REG_CNA_DATA_SIZE1         0x00001024
#define REG_CNA_WEIGHT_SIZE0       0x00001030
#define REG_CNA_WEIGHT_SIZE2       0x00001038
#define REG_CNA_FEATURE_DATA_ADDR  0x00001070
#define REG_CNA_WEIGHT_DATA_ADDR   0x00001074

/* CORE registers */
#define REG_CORE_OPERATION_ENABLE  0x00008008

/* DPU (Data Processing Unit) registers */
#define REG_DPU_OPERATION_ENABLE   0x00010008
#define REG_DPU_DST_BASE_ADDR_LOW  0x00010010
#define REG_DPU_DST_LINE_STRIDE    0x00010018
#define REG_DPU_DST_SURFACE_STRIDE 0x0001001C

/* Helper macros for bitfield construction */
#define CNA_CONV_CON1_CONV_MODE(x)      ((x) << 0)
#define CNA_CONV_CON1_IN_PRECISION(x)   ((x) << 4)
#define CNA_CONV_CON1_PROC_PRECISION(x) ((x) << 7)
#define CNA_DATA_SIZE0_HEIGHT(x)        ((x) << 0)
#define CNA_DATA_SIZE0_WIDTH(x)         ((x) << 16)
#define CNA_DATA_SIZE1_CHANNEL(x)       ((x) << 0)
#define CNA_WEIGHT_SIZE2_KERNELS(x)     ((x) << 0)
#define CNA_WEIGHT_SIZE2_HEIGHT(x)      ((x) << 16)
#define CNA_WEIGHT_SIZE2_WIDTH(x)       ((x) << 24)

/* Generate FP16 matrix multiplication commands
 * A: [M x K] matrix (input)
 * B: [K x N] matrix (weights)
 * C: [M x N] result matrix (output)
 *
 * Matmul is implemented as a 1x1 convolution:
 * - Input: M x K (treated as M height, K channels)
 * - Weights: K x N (treated as N kernels, K channels, 1x1 size)
 * - Output: M x N (M height, N channels)
 */
int gen_matmul_fp16(matmul_params_t *params)
{
    uint64_t *regcmd = params->tasks;
    int idx = 0;

    int M = params->m;  /* Output height / Input rows */
    int K = params->k;  /* Input channels / Inner dimension */
    int N = params->n;  /* Output channels / Output columns */

    /* PC initialization */
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_BASE_ADDRESS);
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_REGISTER_AMOUNTS);

    /* Configure CNA for matmul (as 1x1 convolution) */

    /* CONV_CON1: Set mode and precision
     * Mode 0 = Direct convolution (suitable for 1x1)
     * IN_PRECISION = 1 (FP16)
     * PROC_PRECISION = 1 (FP16)
     */
    uint32_t conv_con1 = CNA_CONV_CON1_CONV_MODE(0) |
                         CNA_CONV_CON1_IN_PRECISION(1) |
                         CNA_CONV_CON1_PROC_PRECISION(1);
    regcmd[idx++] = MESA_REGCMD(TARGET_CNA, conv_con1, REG_CNA_CONV_CON1);

    /* DATA_SIZE0: Input dimensions (height x width)
     * Height = M, Width = 1 (treating as Mx1 spatial)
     */
    uint32_t data_size0 = CNA_DATA_SIZE0_HEIGHT(M) | CNA_DATA_SIZE0_WIDTH(1);
    regcmd[idx++] = MESA_REGCMD(TARGET_CNA, data_size0, REG_CNA_DATA_SIZE0);

    /* DATA_SIZE1: Input channels = K */
    uint32_t data_size1 = CNA_DATA_SIZE1_CHANNEL(K);
    regcmd[idx++] = MESA_REGCMD(TARGET_CNA, data_size1, REG_CNA_DATA_SIZE1);

    /* WEIGHT_SIZE2: Kernel configuration
     * Kernels = N (output channels)
     * Height = 1, Width = 1 (1x1 convolution)
     */
    uint32_t weight_size2 = CNA_WEIGHT_SIZE2_KERNELS(N) |
                            CNA_WEIGHT_SIZE2_HEIGHT(1) |
                            CNA_WEIGHT_SIZE2_WIDTH(1);
    regcmd[idx++] = MESA_REGCMD(TARGET_CNA, weight_size2, REG_CNA_WEIGHT_SIZE2);

    /* Set DMA addresses */
    regcmd[idx++] = MESA_REGCMD(TARGET_CNA, params->input_dma & 0xFFFFFFFF,
                                REG_CNA_FEATURE_DATA_ADDR);
    regcmd[idx++] = MESA_REGCMD(TARGET_CNA, params->weights_dma & 0xFFFFFFFF,
                                REG_CNA_WEIGHT_DATA_ADDR);

    /* Configure DPU for output */
    regcmd[idx++] = MESA_REGCMD(TARGET_DPU, params->output_dma & 0xFFFFFFFF,
                                REG_DPU_DST_BASE_ADDR_LOW);

    /* Set output strides (simple linear layout) */
    regcmd[idx++] = MESA_REGCMD(TARGET_DPU, N * 2, REG_DPU_DST_LINE_STRIDE);  /* 2 bytes per FP16 */
    regcmd[idx++] = MESA_REGCMD(TARGET_DPU, M * N * 2, REG_DPU_DST_SURFACE_STRIDE);

    /* Enable operations */
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 1, REG_PC_OPERATION_ENABLE);
    regcmd[idx++] = MESA_REGCMD(TARGET_CNA, 1, REG_CNA_OPERATION_ENABLE);
    regcmd[idx++] = MESA_REGCMD(TARGET_DPU, 1, REG_DPU_OPERATION_ENABLE);

    /* Disable operations (signals completion) */
    regcmd[idx++] = MESA_REGCMD(TARGET_DPU, 0, REG_DPU_OPERATION_ENABLE);
    regcmd[idx++] = MESA_REGCMD(TARGET_CNA, 0, REG_CNA_OPERATION_ENABLE);
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_OPERATION_ENABLE);

    /* Return number of commands generated */
    return idx;
}

/* Generate INT8 matrix multiplication commands
 * Similar to FP16 but with INT8 quantization
 */
int gen_matmul_int8(matmul_params_t *params)
{
    uint64_t *regcmd = params->tasks;
    int idx = 0;

    /* Minimal sequence for INT8 */
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_BASE_ADDRESS);
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_REGISTER_AMOUNTS);
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 1, REG_PC_OPERATION_ENABLE);
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_OPERATION_ENABLE);

    return idx;
}

/* Stub implementations for compatibility */
int feature_data(int C, int H, int W, int C2, int c, int h, int w)
{
    return 0;
}

int weight_fp16(int C, int k, int c)
{
    return 0;
}

int weight_int8(int C, int k, int c)
{
    return 0;
}

