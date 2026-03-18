/*
 * Minimal NPU test - Just enable NPU and submit empty job
 * This tests if the NPU hardware path is working without complex operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rocket_interface.h"
#include "npu_hw.h"

int main(int argc, char **argv)
{
    struct rocket_ctx ctx = {0};
    struct rocket_bo regcmd_bo = {0};
    uint64_t *regcmd;
    int ret;

    printf("=== Minimal NPU Test ===\n");
    printf("Testing basic NPU enable/disable cycle\n\n");

    /* Open device */
    ret = rocket_open(&ctx);
    if (ret < 0) {
        fprintf(stderr, "Failed to open /dev/accel/accel0: %d\n", ret);
        return 1;
    }
    printf("✓ Opened /dev/accel/accel0 successfully\n");

    /* Allocate minimal register command buffer */
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

    /* Create minimal command sequence:
     * 1. Set PC base address to 0
     * 2. Set register amount to 0 (no registers to process)
     * 3. Enable PC block
     * 4. Disable PC block (complete operation)
     */
    int cmd_idx = 0;

    /* Set PC base address to 0 */
    regcmd[cmd_idx++] = NPUOP(OP_REG_PC, 0, PC_BASE_ADDRESS);

    /* Set register amount to 0 (minimal operation) */
    regcmd[cmd_idx++] = NPUOP(OP_REG_PC, 0, PC_REGISTER_AMOUNTS);

    /* Enable PC block */
    regcmd[cmd_idx++] = NPUOP(OP_ENABLE, PC_ENABLE, PC_OPERATION_ENABLE);

    /* Disable PC block (complete) */
    regcmd[cmd_idx++] = NPUOP(OP_NONE, 0, PC_OPERATION_ENABLE);

    printf("✓ Generated %d register commands\n", cmd_idx);

    /* Sync buffer to device */
    ret = rocket_bo_fini(&ctx, &regcmd_bo);
    if (ret < 0) {
        fprintf(stderr, "Failed to sync buffer: %d\n", ret);
        rocket_bo_destroy(&ctx, &regcmd_bo);
        rocket_close(&ctx);
        return 1;
    }

    /* Submit job */
    printf("Submitting minimal NPU job...\n");
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
    ret = rocket_bo_prep(&ctx, &regcmd_bo, -1);  /* -1 = infinite timeout */
    if (ret < 0) {
        fprintf(stderr, "✗ NPU job wait failed: %d\n", ret);
        rocket_bo_destroy(&ctx, &regcmd_bo);
        rocket_close(&ctx);
        return 1;
    }
    printf("✓ NPU job completed successfully!\n");

    /* Cleanup */
    rocket_bo_destroy(&ctx, &regcmd_bo);
    rocket_close(&ctx);

    printf("\n=== Test PASSED ===\n");
    printf("NPU hardware path is working!\n");

    return 0;
}

