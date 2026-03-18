/*
 * Test CNA enable/disable - simplest possible CNA test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rocket_interface.h"

/* Mesa-style register command format */
#define MESA_REGCMD(target, value, reg) \
    ((((uint64_t)(target)) << 48) | (((uint64_t)(value)) << 16) | ((uint64_t)(reg)))

/* Target domains */
#define TARGET_PC       0x100
#define TARGET_CNA      0x200

/* Registers */
#define REG_PC_OPERATION_ENABLE    0x00000008
#define REG_PC_BASE_ADDRESS        0x00000010
#define REG_PC_REGISTER_AMOUNTS    0x00000014
#define REG_CNA_OPERATION_ENABLE   0x00001008

int main(int argc, char **argv)
{
    struct rocket_ctx ctx = {0};
    struct rocket_bo regcmd_bo = {0};
    uint64_t *regcmd;
    int ret;

    printf("=== CNA Enable Test ===\n");

    ret = rocket_open(&ctx);
    if (ret < 0) {
        fprintf(stderr, "Failed to open device: %d\n", ret);
        return 1;
    }
    printf("✓ Opened device\n");

    ret = rocket_bo_create(&ctx, &regcmd_bo, 4096);
    if (ret < 0) {
        fprintf(stderr, "Failed to create buffer: %d\n", ret);
        rocket_close(&ctx);
        return 1;
    }

    regcmd = (uint64_t *)rocket_bo_map(&ctx, &regcmd_bo);
    if (!regcmd) {
        fprintf(stderr, "Failed to map buffer\n");
        rocket_bo_destroy(&ctx, &regcmd_bo);
        rocket_close(&ctx);
        return 1;
    }

    memset(regcmd, 0, 4096);
    int idx = 0;

    /* PC init */
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_BASE_ADDRESS);
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_REGISTER_AMOUNTS);
    
    /* Enable PC */
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 1, REG_PC_OPERATION_ENABLE);
    
    /* Enable CNA */
    regcmd[idx++] = MESA_REGCMD(TARGET_CNA, 1, REG_CNA_OPERATION_ENABLE);
    
    /* Disable CNA */
    regcmd[idx++] = MESA_REGCMD(TARGET_CNA, 0, REG_CNA_OPERATION_ENABLE);
    
    /* Disable PC */
    regcmd[idx++] = MESA_REGCMD(TARGET_PC, 0, REG_PC_OPERATION_ENABLE);

    printf("✓ Generated %d commands\n", idx);

    ret = rocket_bo_fini(&ctx, &regcmd_bo);
    if (ret < 0) {
        fprintf(stderr, "Failed to sync buffer: %d\n", ret);
        rocket_bo_destroy(&ctx, &regcmd_bo);
        rocket_close(&ctx);
        return 1;
    }

    printf("Submitting job...\n");
    ret = rocket_submit(&ctx, &regcmd_bo, idx, NULL, 0, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "✗ Submit failed: %d\n", ret);
        rocket_bo_destroy(&ctx, &regcmd_bo);
        rocket_close(&ctx);
        return 1;
    }
    printf("✓ Job submitted\n");

    printf("Waiting for completion...\n");
    ret = rocket_bo_prep(&ctx, &regcmd_bo, 5000);
    if (ret < 0) {
        fprintf(stderr, "✗ Wait failed: %d\n", ret);
        rocket_bo_destroy(&ctx, &regcmd_bo);
        rocket_close(&ctx);
        return 1;
    }
    printf("✓ Job completed!\n");

    rocket_bo_destroy(&ctx, &regcmd_bo);
    rocket_close(&ctx);

    printf("\n=== CNA Enable Test PASSED ===\n");
    return 0;
}

