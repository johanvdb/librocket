/*
 * NPU Matrix Multiplication - Full Mesa-based implementation
 * Based on Mesa's Rocket driver register sequence
 * 
 * This implements matmul as a 1x1 convolution with full register configuration
 * following Mesa's fill_first_regcmd() function.
 */

#include <stdint.h>
#include <string.h>
#include "npu_matmul.h"

/* Mesa-style register command format */
#define MESA_REGCMD(target, value, reg) \
    ((((uint64_t)(target)) << 48) | (((uint64_t)(value)) << 16) | ((uint64_t)(reg)))

/* Target domains */
#define TARGET_PC       0x100
#define TARGET_CNA      0x200
#define TARGET_CORE     0x800
#define TARGET_DPU      0x1000

/* Helper to emit raw command with custom target */
#define EMIT_RAW(target, reg, value) \
    regcmd[idx++] = MESA_REGCMD(target, value, reg)

/* Helper to emit standard command */
#define EMIT(target, reg, value) \
    EMIT_RAW(TARGET_##target, REG_##target##_##reg, value)

/* Include all register definitions from Mesa */
#include "mesa_registers.h"
#include "mesa_helpers.h"

/* Generate FP16 matrix multiplication commands
 * Matmul: C[M,N] = A[M,K] * B[K,N]
 * Implemented as 1x1 convolution:
 *   Input: M x K (height=M, channels=K)
 *   Weights: K x N (kernels=N, channels=K, size=1x1)
 *   Output: M x N (height=M, channels=N)
 */
int gen_matmul_fp16(matmul_params_t *params)
{
    uint64_t *regcmd = params->tasks;
    int idx = 0;
    
    int M = params->m;  /* Output height / Input rows */
    int K = params->k;  /* Input channels / Inner dimension */
    int N = params->n;  /* Output channels / Output columns */

    /* For FP16, we need to adapt Mesa's INT8 sequence */
    /* Mesa uses banks, entries, etc. for INT8 - we'll simplify for FP16 */
    
    /* CNA Configuration */
    
    /* CBUF_CON0: Buffer configuration */
    EMIT(CNA, CBUF_CON0, 
         CNA_CBUF_CON0_WEIGHT_BANK(1) | CNA_CBUF_CON0_DATA_BANK(1));
    
    /* DCOMP: Decompression (disabled for FP16) */
    EMIT(CNA, DCOMP_REGNUM, 0);
    EMIT(CNA, DCOMP_CTRL, 0);
    
    /* CONV_CON1: Convolution mode and precision
     * Mode 0 = Direct convolution (for 1x1)
     * For FP16: IN_PRECISION=2, PROC_PRECISION=2
     */
    uint32_t conv_con1 = CNA_CONV_CON1_CONV_MODE(0) |
                         CNA_CONV_CON1_IN_PRECISION(2) |  /* FP16 */
                         CNA_CONV_CON1_PROC_PRECISION(2); /* FP16 */
    EMIT(CNA, CONV_CON1, conv_con1);
    
    /* DPU S_POINTER configuration */
    EMIT(DPU, S_POINTER, 
         DPU_S_POINTER_POINTER_PP_MODE(1) |
         DPU_S_POINTER_EXECUTER_PP_EN(1) |
         DPU_S_POINTER_POINTER_PP_EN(1));
    
    EMIT(DPU, RDMA_RDMA_S_POINTER,
         DPU_RDMA_RDMA_S_POINTER_POINTER_PP_MODE(1) |
         DPU_RDMA_RDMA_S_POINTER_EXECUTER_PP_EN(1) |
         DPU_RDMA_RDMA_S_POINTER_POINTER_PP_EN(1));
    
    /* CONV_CON2/CON3: Stride configuration */
    EMIT(CNA, CONV_CON1, conv_con1);  /* Repeat as Mesa does */
    EMIT(CNA, CONV_CON2, CNA_CONV_CON2_FEATURE_GRAINS(52)); /* Magic value from Mesa */
    EMIT(CNA, CONV_CON3, 
         CNA_CONV_CON3_CONV_X_STRIDE(1) | CNA_CONV_CON3_CONV_Y_STRIDE(1));
    
    /* DATA_SIZE: Input dimensions */
    EMIT(CNA, DATA_SIZE0,
         CNA_DATA_SIZE0_DATAIN_WIDTH(1) |      /* Width = 1 for matmul */
         CNA_DATA_SIZE0_DATAIN_HEIGHT(M));     /* Height = M */
    
    EMIT(CNA, DATA_SIZE1,
         CNA_DATA_SIZE1_DATAIN_CHANNEL_REAL(K - 1) |
         CNA_DATA_SIZE1_DATAIN_CHANNEL(K));
    
    EMIT(CNA, DATA_SIZE2, CNA_DATA_SIZE2_DATAOUT_WIDTH(1));
    EMIT(CNA, DATA_SIZE3, CNA_DATA_SIZE3_DATAOUT_ATOMICS(M)); /* Atomics = output height */
    
    /* WEIGHT_SIZE: Weight dimensions */
    uint32_t weight_total = 1 * 1 * K * N;  /* 1x1 kernel, K channels, N kernels */
    EMIT(CNA, WEIGHT_SIZE0, weight_total);
    EMIT(CNA, WEIGHT_SIZE1, 1 * 1 * K);  /* Per kernel */
    EMIT(CNA, WEIGHT_SIZE2,
         CNA_WEIGHT_SIZE2_WEIGHT_WIDTH(1) |
         CNA_WEIGHT_SIZE2_WEIGHT_HEIGHT(1) |
         CNA_WEIGHT_SIZE2_WEIGHT_KERNELS(N));
    
    /* CBUF_CON0 again (Mesa does this) */
    EMIT(CNA, CBUF_CON0,
         CNA_CBUF_CON0_WEIGHT_BANK(1) | CNA_CBUF_CON0_DATA_BANK(1));
    
    /* CBUF_CON1: Data entries */
    EMIT(CNA, CBUF_CON1, CNA_CBUF_CON1_DATA_ENTRIES(M * K));
    
    /* CVT: Conversion/scaling - bypass for FP16 */
    EMIT(CNA, CVT_CON0,
         CNA_CVT_CON0_DATA_SIGN(1) |
         CNA_CVT_CON0_CVT_TYPE(1) |
         CNA_CVT_CON0_CVT_BYPASS(1));
    EMIT(CNA, CVT_CON1, CNA_CVT_CON1_CVT_SCALE0(1));
    EMIT(CNA, CVT_CON2, CNA_CVT_CON2_CVT_SCALE1(1));
    EMIT(CNA, CVT_CON3, CNA_CVT_CON3_CVT_SCALE2(1));
    EMIT(CNA, CVT_CON4, CNA_CVT_CON4_CVT_SCALE3(1));
    
    /* FC: Fully connected - not used for convolution mode */
    EMIT(CNA, FC_CON0, 0);
    EMIT(CNA, FC_CON1, 0);
    
    /* PAD: Padding - none for 1x1 */
    EMIT(CNA, PAD_CON0, 0);
    
    /* FEATURE_DATA_ADDR: Input address */
    EMIT(CNA, FEATURE_DATA_ADDR, params->input_dma & 0xFFFFFFFF);
    
    /* FC_CON2 */
    EMIT(CNA, FC_CON2, 0);

    /* FC_DATA_SIZE */
    EMIT(CNA, FC_DATA_SIZE0, 0);
    EMIT(CNA, FC_DATA_SIZE1, 0);

    /* DCOMP: All decompression registers (must be set even if not used) */
    EMIT(CNA, DCOMP_CTRL, 0);
    EMIT(CNA, DCOMP_REGNUM, 0);
    EMIT(CNA, DCOMP_ADDR0, params->weights_dma & 0xFFFFFFFF);  /* Weight address */
    EMIT(CNA, DCOMP_AMOUNT0, 0);
    EMIT(CNA, DCOMP_AMOUNT1, 0);
    EMIT(CNA, DCOMP_AMOUNT2, 0);
    EMIT(CNA, DCOMP_AMOUNT3, 0);
    EMIT(CNA, DCOMP_AMOUNT4, 0);
    EMIT(CNA, DCOMP_AMOUNT5, 0);
    EMIT(CNA, DCOMP_AMOUNT6, 0);
    EMIT(CNA, DCOMP_AMOUNT7, 0);
    EMIT(CNA, DCOMP_AMOUNT8, 0);
    EMIT(CNA, DCOMP_AMOUNT9, 0);
    EMIT(CNA, DCOMP_AMOUNT10, 0);
    EMIT(CNA, DCOMP_AMOUNT11, 0);
    EMIT(CNA, DCOMP_AMOUNT12, 0);
    EMIT(CNA, DCOMP_AMOUNT13, 0);
    EMIT(CNA, DCOMP_AMOUNT14, 0);
    EMIT(CNA, DCOMP_AMOUNT15, 0);

    /* CVT_CON5 */
    EMIT(CNA, CVT_CON5, 0);

    /* PAD_CON1 */
    EMIT(CNA, PAD_CON1, 0);

    /* CORE Configuration */
    EMIT(CORE, MISC_CFG, CORE_MISC_CFG_QD_EN(1));
    EMIT(CORE, DATAOUT_SIZE_0,
         CORE_DATAOUT_SIZE_0_DATAOUT_WIDTH(1) |
         CORE_DATAOUT_SIZE_0_DATAOUT_HEIGHT(M - 1));
    EMIT(CORE, DATAOUT_SIZE_1,
         CORE_DATAOUT_SIZE_1_DATAOUT_CHANNEL(N - 1));
    EMIT(CORE, CLIP_TRUNCATE, 0);  /* No truncation for FP16 */
    EMIT_RAW(TARGET_CORE | 0x1, 0x3030, 0);  /* Magic register from Mesa */

    /* DPU Configuration */
    EMIT(DPU, FEATURE_MODE_CFG,
         DPU_FEATURE_MODE_CFG_BURST_LEN(15) |
         DPU_FEATURE_MODE_CFG_OUTPUT_MODE(2));
    EMIT(DPU, DATA_FORMAT, 0);
    EMIT(DPU, OFFSET_PEND, 0);
    EMIT(DPU, DST_BASE_ADDR, params->output_dma & 0xFFFFFFFF);
    EMIT(DPU, DST_SURF_STRIDE, M * N * 2);  /* 2 bytes per FP16 */
    EMIT(DPU, DATA_CUBE_WIDTH, 0);  /* Width - 1 = 0 */
    EMIT(DPU, DATA_CUBE_HEIGHT, M - 1);
    EMIT(DPU, DATA_CUBE_NOTCH_ADDR, 0);
    EMIT(DPU, DATA_CUBE_CHANNEL,
         DPU_DATA_CUBE_CHANNEL_ORIG_CHANNEL(N - 1) |
         DPU_DATA_CUBE_CHANNEL_CHANNEL(N - 1));

    /* BS (Batch/Scale) Configuration */
    EMIT(DPU, BS_CFG,
         DPU_BS_CFG_BS_ALU_ALGO(2) |
         DPU_BS_CFG_BS_ALU_SRC(1) |
         DPU_BS_CFG_BS_RELU_BYPASS(1) |
         DPU_BS_CFG_BS_MUL_BYPASS(1));
    EMIT(DPU, BS_ALU_CFG, 0);
    EMIT(DPU, BS_MUL_CFG, 0);
    EMIT(DPU, BS_RELUX_CMP_VALUE, 0);
    EMIT(DPU, BS_OW_CFG,
         DPU_BS_OW_CFG_SIZE_E_2(1) |
         DPU_BS_OW_CFG_SIZE_E_1(1) |
         DPU_BS_OW_CFG_SIZE_E_0(1));
    EMIT(DPU, BS_OW_OP, 0);

    /* WDMA (Write DMA) Configuration */
    EMIT(DPU, WDMA_SIZE_0, DPU_WDMA_SIZE_0_CHANNEL_WDMA(N - 1));
    EMIT(DPU, WDMA_SIZE_1,
         DPU_WDMA_SIZE_1_HEIGHT_WDMA(M - 1) |
         DPU_WDMA_SIZE_1_WIDTH_WDMA(0));

    /* BN (Batch Normalization) - bypass for matmul */
    EMIT(DPU, BN_CFG, DPU_BN_CFG_BN_BYPASS(1));
    EMIT(DPU, BN_ALU_CFG, 0);
    EMIT(DPU, BN_MUL_CFG, 0);
    EMIT(DPU, BN_RELUX_CMP_VALUE, 0);
    EMIT(DPU, BN_OW_CFG, 0);

    /* EW (Element-wise) - bypass for matmul */
    EMIT(DPU, EW_CFG,
         DPU_EW_CFG_EW_ALU_ALGO(2) |
         DPU_EW_CFG_EW_ALU_SRC(1) |
         DPU_EW_CFG_EW_RELU_BYPASS(1) |
         DPU_EW_CFG_EW_MUL_BYPASS(1) |
         DPU_EW_CFG_EW_BYPASS(1));
    EMIT(DPU, EW_ALU_CFG, 0);
    EMIT(DPU, EW_ALU_CVT_OFFSET_VALUE, 0);
    EMIT(DPU, EW_ALU_CVT_SCALE_VALUE, 0);
    EMIT(DPU, EW_ALU_CVT_TRUNCATE_VALUE, 0);
    EMIT(DPU, EW_MUL_CFG, 0);
    EMIT(DPU, EW_MUL_CVT_OFFSET_VALUE, 0);
    EMIT(DPU, EW_MUL_CVT_SCALE_VALUE, 0);
    EMIT(DPU, EW_MUL_CVT_TRUNCATE_VALUE, 0);
    EMIT(DPU, EW_RELUX_CMP_VALUE, 0);
    EMIT(DPU, EW_OW_CFG, 0);

    /* OUT_CVT (Output Conversion) - bypass for FP16 */
    EMIT(DPU, OUT_CVT_CFG, DPU_OUT_CVT_CFG_OUT_CVT_BYPASS(1));
    EMIT(DPU, OUT_CVT_OFFSET, 0);
    EMIT(DPU, OUT_CVT_SCALE, 0);
    EMIT(DPU, OUT_CVT_SHIFT, 0);

    /* SURFACE_ADD */
    EMIT(DPU, SURFACE_ADD, 0);

    /* LUT (Lookup Table) - bypass */
    EMIT(DPU, LUT_CFG, DPU_LUT_CFG_LUT_LE_FUNCTION(1) | DPU_LUT_CFG_LUT_UFLOW_PRIORITY(1));
    EMIT(DPU, LUT_INFO, 0);
    EMIT(DPU, LUT_LE_START_LOW, 0);
    EMIT(DPU, LUT_LE_START_HIGH, 0);
    EMIT(DPU, LUT_LE_END_LOW, 0);
    EMIT(DPU, LUT_LE_END_HIGH, 0);
    EMIT(DPU, LUT_LO_START_LOW, 0);
    EMIT(DPU, LUT_LO_START_HIGH, 0);
    EMIT(DPU, LUT_LO_END_LOW, 0);
    EMIT(DPU, LUT_LO_END_HIGH, 0);
    EMIT(DPU, LUT_LE_SLOPE_SCALE, 0);
    EMIT(DPU, LUT_LE_SLOPE_SHIFT, 0);
    EMIT(DPU, LUT_LO_SLOPE_SCALE, 0);
    EMIT(DPU, LUT_LO_SLOPE_SHIFT, 0);

    /* RDMA (Read DMA) - extensive configuration */
    EMIT(DPU, RDMA_RDMA_CFG,
         DPU_RDMA_RDMA_CFG_RDMA_DISABLE(0) |
         DPU_RDMA_RDMA_CFG_RDMA_DATA_MODE(1) |
         DPU_RDMA_RDMA_CFG_RDMA_DATA_SIZE(2) |  /* FP16 */
         DPU_RDMA_RDMA_CFG_RDMA_DATA_USE(1));
    EMIT(DPU, RDMA_RDMA_DATA_CUBE_WIDTH, 0);
    EMIT(DPU, RDMA_RDMA_DATA_CUBE_HEIGHT, M - 1);
    EMIT(DPU, RDMA_RDMA_DATA_CUBE_CHANNEL, N - 1);
    EMIT(DPU, RDMA_RDMA_BASE_ADDR, params->output_dma & 0xFFFFFFFF);  /* Bias address (none for now) */
    EMIT(DPU, RDMA_RDMA_SURF_STRIDE, M * N * 2);
    EMIT(DPU, RDMA_RDMA_SURF_NOTCH, 0);
    EMIT(DPU, RDMA_RDMA_PAD_CFG, 0);
    EMIT(DPU, RDMA_RDMA_WEIGHT,
         DPU_RDMA_RDMA_WEIGHT_E_WEIGHT(1) |
         DPU_RDMA_RDMA_WEIGHT_N_WEIGHT(1) |
         DPU_RDMA_RDMA_WEIGHT_B_WEIGHT(1) |
         DPU_RDMA_RDMA_WEIGHT_M_WEIGHT(1));
    EMIT(DPU, RDMA_RDMA_EW_SURF_NOTCH, 0);

    /* PC Configuration and Enable Sequence */
    EMIT(PC, BASE_ADDRESS, 0);
    EMIT(PC, REGISTER_AMOUNTS, 0);

    /* CRITICAL: Magic value before enable (from TRM) */
    regcmd[idx++] = 0x0041000000000000ULL;

    /* CRITICAL: Enable with special target 0x81 and RESERVED_0(14) */
    EMIT_RAW(0x81, REG_PC_OPERATION_ENABLE,
             PC_OPERATION_ENABLE_RESERVED_0(14) | PC_OPERATION_ENABLE_OP_EN(1));

    return idx;
}

/* Stub implementations for compatibility */
int gen_matmul_int8(matmul_params_t *params)
{
    /* TODO: Implement INT8 version */
    return -1;
}

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

