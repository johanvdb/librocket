/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Rocket NPU Interface - Userspace interface for Rocket DRM/accel driver
 * 
 * Ported from mtx512/rk3588-npu to use mainline Rocket driver
 * Original: https://github.com/mtx512/rk3588-npu (GPL-3.0)
 * 
 * Copyright (C) 2024 Jasbir Matharu (original RKNPU version)
 * Copyright (C) 2026 Orange Pi Cluster Project (Rocket port)
 */

#ifndef ROCKET_INTERFACE_H
#define ROCKET_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Buffer object - wraps a Rocket GEM buffer */
struct rocket_bo {
    int fd;              /* DRM fd */
    uint32_t handle;     /* GEM handle */
    uint64_t dma_addr;   /* DMA address for NPU */
    uint64_t size;       /* Size in bytes */
    void *map;           /* CPU mapping */
};

/* NPU context */
struct rocket_ctx {
    int fd;              /* DRM fd for /dev/accel/accel0 */
};

/**
 * rocket_open - Open the Rocket NPU device
 * @ctx: Context to initialize
 * 
 * Returns 0 on success, negative error code on failure
 */
int rocket_open(struct rocket_ctx *ctx);

/**
 * rocket_close - Close the Rocket NPU device
 * @ctx: Context to close
 */
void rocket_close(struct rocket_ctx *ctx);

/**
 * rocket_bo_create - Create a buffer object
 * @ctx: NPU context
 * @bo: Buffer object to initialize
 * @size: Size in bytes
 * 
 * Returns 0 on success, negative error code on failure
 */
int rocket_bo_create(struct rocket_ctx *ctx, struct rocket_bo *bo, size_t size);

/**
 * rocket_bo_destroy - Destroy a buffer object
 * @ctx: NPU context
 * @bo: Buffer object to destroy
 */
void rocket_bo_destroy(struct rocket_ctx *ctx, struct rocket_bo *bo);

/**
 * rocket_bo_map - Map a buffer object to CPU
 * @ctx: NPU context
 * @bo: Buffer object to map
 * 
 * Returns pointer to mapped memory, NULL on failure
 */
void *rocket_bo_map(struct rocket_ctx *ctx, struct rocket_bo *bo);

/**
 * rocket_bo_unmap - Unmap a buffer object from CPU
 * @bo: Buffer object to unmap
 */
void rocket_bo_unmap(struct rocket_bo *bo);

/**
 * rocket_bo_prep - Prepare buffer for CPU access (sync from device)
 * @ctx: NPU context
 * @bo: Buffer object
 * @timeout_ns: Timeout in nanoseconds (-1 for infinite)
 * 
 * Returns 0 on success, negative error code on failure
 */
int rocket_bo_prep(struct rocket_ctx *ctx, struct rocket_bo *bo, int64_t timeout_ns);

/**
 * rocket_bo_fini - Finish CPU access (sync to device)
 * @ctx: NPU context  
 * @bo: Buffer object
 * 
 * Returns 0 on success, negative error code on failure
 */
int rocket_bo_fini(struct rocket_ctx *ctx, struct rocket_bo *bo);

/**
 * rocket_submit - Submit a job to the NPU
 * @ctx: NPU context
 * @regcmd_bo: Buffer containing register commands
 * @regcmd_count: Number of 64-bit register commands
 * @in_bos: Array of input buffer objects
 * @in_bo_count: Number of input buffers
 * @out_bos: Array of output buffer objects
 * @out_bo_count: Number of output buffers
 * 
 * Returns 0 on success, negative error code on failure
 */
int rocket_submit(struct rocket_ctx *ctx,
                  struct rocket_bo *regcmd_bo, uint32_t regcmd_count,
                  struct rocket_bo **in_bos, uint32_t in_bo_count,
                  struct rocket_bo **out_bos, uint32_t out_bo_count);

#ifdef __cplusplus
}
#endif

#endif /* ROCKET_INTERFACE_H */

