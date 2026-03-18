/*
 * Minimal NPU test using Mesa/Teflon register format
 * Based on Mesa's Rocket driver register definitions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rocket_interface.h"

/* Mesa-style register command format */
#define MESA_REGCMD(target, value, reg) \
    ((((uint64_t)(target)) << 48) | (((uint64_t)(value)) << 16) | ((uint64_t)(reg)))

/* Target domains from Mesa registers.xml */
#define TARGET_PC       0x100
#define TARGET_CNA      0x200
#define TARGET_CORE     0x800
#define TARGET_DPU      0x1000

/* PC registers from Mesa */
#define REG_PC_OPERATION_ENABLE    0x00000008
#define REG_PC_BASE_ADDRESS        0x00000010
#define REG_PC_REGISTER_AMOUNTS    0x00000014

int main(int argc, char **argv)
{
    struct rocket_ctx ctx = {0};
    struct rocket_bo regcmd_bo = {0};
    uint64_t *regcmd;
    int ret;

    printf("=== Minimal NPU Test (Mesa Format) ===\n");
    printf("Using Mesa/Teflon register command format\n\n");

    /* Open device */
    ret = rocket_open(&ctx);
    if (ret < 0) {
        fprintf(stderr, "Failed to open /dev/accel/accel0: %d\n", ret);
        return 1;
    }
    printf("✓ Opened /dev/accel/accel0 successfully\n");

    /* Allocate register command buffer */
    ret = rocket_bo_create(&ctx, &regcmd_bo, 4096);
    if (ret < 0) {
        fprintf(stderr, "Failed to allocate register command buffer: %d\n", ret);
        rocket_close(&ctx);
        return 1;
    }
    printf("✓ Allocated register command buffer\n");

    /* Map buffer */
    regcmd = (uint64_t *)rocket_bo_map(&ctx, &regcmd_bo);
    if (!regcmd) {
        fprintf(stderr, "Failed to map register command buffer\n");
        rocket_bo_destroy(&ctx, &regcmd_bo);
        rocket_close(&ctx);
        return 1;
    }

    memset(regcmd, 0, 4096);

    /* Create minimal command sequence using Mesa format:
     * 1. Set PC base address to 0
     * 2. Set register amount to 0 (no additional registers)
     * 3. Enable PC operation
     */
    int cmd_idx = 0;

    /* Set PC base address to 0 */
    regcmd[cmd_idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_BASE_ADDRESS);
    
    /* Set register amount to 0 */
    regcmd[cmd_idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_REGISTER_AMOUNTS);
    
    /* Enable PC operation (OP_EN = 1) */
    regcmd[cmd_idx++] = MESA_REGCMD(TARGET_PC, 1, REG_PC_OPERATION_ENABLE);

    printf("✓ Generated %d register commands (Mesa format)\n", cmd_idx);
    printf("  Command 0: target=0x%03x value=0x%08x reg=0x%08x\n", 
           TARGET_PC, 0, REG_PC_BASE_ADDRESS);
    printf("  Command 1: target=0x%03x value=0x%08x reg=0x%08x\n", 
           TARGET_PC, 0, REG_PC_REGISTER_AMOUNTS);
    printf("  Command 2: target=0x%03x value=0x%08x reg=0x%08x\n", 
           TARGET_PC, 1, REG_PC_OPERATION_ENABLE);

    /* Sync buffer to device */
    ret = rocket_bo_fini(&ctx, &regcmd_bo);
    if (ret < 0) {
        fprintf(stderr, "Failed to sync buffer: %d\n", ret);
        rocket_bo_destroy(&ctx, &regcmd_bo);
        rocket_close(&ctx);
        return 1;
    }

    /* Submit job */
    printf("\nSubmitting NPU job...\n");
    ret = rocket_submit(&ctx, &regcmd_bo, cmd_idx, NULL, 0, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "✗ NPU job submission failed: %d\n", ret);
        rocket_bo_destroy(&ctx, &regcmd_bo);
        rocket_close(&ctx);
        return 1;
    }
    printf("✓ NPU job submitted successfully\n");

    /* Wait for completion */
    printf("Waiting for NPU job completion...\n");
    ret = rocket_bo_prep(&ctx, &regcmd_bo, 5000);  /* 5 second timeout */
    if (ret < 0) {
        fprintf(stderr, "✗ NPU job wait failed: %d (timeout or error)\n", ret);
        rocket_bo_destroy(&ctx, &regcmd_bo);
        rocket_close(&ctx);
        return 1;
    }
    printf("✓ NPU job completed successfully!\n");

    /* Cleanup */
    rocket_bo_destroy(&ctx, &regcmd_bo);
    rocket_close(&ctx);

    printf("\n=== Test PASSED ===\n");
    printf("NPU hardware path is working with Mesa format!\n");
    
    return 0;
}

