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

/* CNA (Convolution Neural Accelerator) registers - subset for matmul */
#define REG_CNA_OPERATION_ENABLE   0x00001008
#define REG_CNA_S_POINTER          0x00001004

/* CORE registers */
#define REG_CORE_OPERATION_ENABLE  0x00008008

/* Generate FP16 matrix multiplication commands
 * A: [M x K] matrix
 * B: [K x N] matrix
 * C: [M x N] result matrix
 */
int gen_matmul_fp16(matmul_params_t *params)
{
    uint64_t *regcmd = params->tasks;
    int idx = 0;

    /* For now, create a minimal command sequence that:
     * 1. Initializes PC (Program Counter)
     * 2. Enables operation
     * 3. Signals completion
     *
     * TODO: Implement actual matmul register sequence based on Mesa's
     * convolution implementation adapted for matrix multiplication
     */

    /* Set PC base address */
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_BASE_ADDRESS);

    /* Set register amount to minimal */
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_REGISTER_AMOUNTS);

    /* Enable PC operation */
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 1, REG_PC_OPERATION_ENABLE);

    /* TODO: Add CNA configuration for actual matmul:
     * - Configure input/output dimensions (params->m, params->k, params->n)
     * - Set DMA addresses (params->input_dma, params->weights_dma, params->output_dma)
     * - Configure data format (FP16)
     * - Set convolution mode to matmul (if supported)
     * - Enable CNA operation
     */

    /* For now, just complete the operation */
    /* Disable PC operation (signals completion) */
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

