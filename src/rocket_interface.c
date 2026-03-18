/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Rocket NPU Interface - Userspace interface for Rocket DRM/accel driver
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#include "rocket_interface.h"

/* Include kernel UAPI header - copy from kernel or use local */
#include "rocket_accel.h"

/* DRM gem close ioctl */
#include <drm/drm.h>

int rocket_open(struct rocket_ctx *ctx)
{
    if (!ctx) {
        fprintf(stderr, "rocket_open: Invalid context pointer\n");
        return -EINVAL;
    }

    /* Try accel node first (mainline kernel 6.19+) */
    ctx->fd = open("/dev/accel/accel0", O_RDWR);
    if (ctx->fd < 0) {
        /* Fallback to accel1 if accel0 is used by another device */
        ctx->fd = open("/dev/accel/accel1", O_RDWR);
    }

    if (ctx->fd < 0) {
        int err = errno;

        /* Provide helpful error messages */
        if (err == ENOENT) {
            fprintf(stderr, "ERROR: Rocket NPU device not found (/dev/accel/accel*)\n");
            fprintf(stderr, "  - Is the Rocket kernel driver loaded? Try: modprobe rocket\n");
            fprintf(stderr, "  - Is this an RK3588 system? Rocket only supports RK3588 NPU\n");
            fprintf(stderr, "  - Is the kernel 6.19+? Rocket requires mainline kernel 6.19 or later\n");
        } else if (err == EACCES) {
            fprintf(stderr, "ERROR: Permission denied accessing Rocket NPU device\n");
            fprintf(stderr, "  - Try running with sudo, or add your user to the 'video' group\n");
        } else if (err == EBUSY) {
            fprintf(stderr, "ERROR: Rocket NPU device is busy (already in use)\n");
        } else {
            fprintf(stderr, "ERROR: Failed to open Rocket NPU device: %s\n", strerror(err));
        }

        return -err;
    }

    fprintf(stderr, "Rocket NPU device opened successfully (fd=%d)\n", ctx->fd);
    return 0;
}

void rocket_close(struct rocket_ctx *ctx)
{
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }
}

/* Store mmap offset in unused bits of rocket_bo - we extend the struct */
struct rocket_bo_internal {
    struct rocket_bo base;
    uint64_t mmap_offset;
};

int rocket_bo_create(struct rocket_ctx *ctx, struct rocket_bo *bo, size_t size)
{
    struct drm_rocket_create_bo create = {
        .size = size,
    };
    int ret;

    ret = ioctl(ctx->fd, DRM_IOCTL_ROCKET_CREATE_BO, &create);
    if (ret < 0) {
        perror("DRM_IOCTL_ROCKET_CREATE_BO failed");
        return -errno;
    }

    bo->fd = ctx->fd;
    bo->handle = create.handle;
    bo->dma_addr = create.dma_address;
    bo->size = size;
    bo->map = NULL;

    /* Store mmap offset - we'll use a separate tracking mechanism */
    /* For now, the offset is returned by create_bo */
    /* We'll mmap using the returned offset directly */

    /* Immediately mmap the buffer for simplicity */
    bo->map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   ctx->fd, create.offset);
    if (bo->map == MAP_FAILED) {
        struct drm_gem_close close_args = { .handle = create.handle };
        ioctl(ctx->fd, DRM_IOCTL_GEM_CLOSE, &close_args);
        bo->map = NULL;
        return -errno;
    }

    return 0;
}

void rocket_bo_destroy(struct rocket_ctx *ctx, struct rocket_bo *bo)
{
    struct drm_gem_close close_args = {
        .handle = bo->handle,
    };
    
    if (bo->map) {
        rocket_bo_unmap(bo);
    }
    
    ioctl(ctx->fd, DRM_IOCTL_GEM_CLOSE, &close_args);
    
    bo->handle = 0;
    bo->dma_addr = 0;
    bo->size = 0;
}

void *rocket_bo_map(struct rocket_ctx *ctx, struct rocket_bo *bo)
{
    /* Buffer is already mapped in rocket_bo_create */
    (void)ctx;
    return bo->map;
}

void rocket_bo_unmap(struct rocket_bo *bo)
{
    if (bo->map) {
        munmap(bo->map, bo->size);
        bo->map = NULL;
    }
}

int rocket_bo_prep(struct rocket_ctx *ctx, struct rocket_bo *bo, int64_t timeout_ns)
{
    struct drm_rocket_prep_bo prep = {
        .handle = bo->handle,
        .timeout_ns = timeout_ns,
    };
    int ret;
    
    ret = ioctl(ctx->fd, DRM_IOCTL_ROCKET_PREP_BO, &prep);
    if (ret < 0) {
        return -errno;
    }
    
    return 0;
}

int rocket_bo_fini(struct rocket_ctx *ctx, struct rocket_bo *bo)
{
    struct drm_rocket_fini_bo fini = {
        .handle = bo->handle,
    };
    int ret;

    ret = ioctl(ctx->fd, DRM_IOCTL_ROCKET_FINI_BO, &fini);
    if (ret < 0) {
        return -errno;
    }

    return 0;
}

int rocket_submit(struct rocket_ctx *ctx,
                  struct rocket_bo *regcmd_bo, uint32_t regcmd_count,
                  struct rocket_bo **in_bos, uint32_t in_bo_count,
                  struct rocket_bo **out_bos, uint32_t out_bo_count)
{
    /* Build the handle arrays */
    uint32_t *in_handles = NULL;
    uint32_t *out_handles = NULL;
    int ret;

    if (in_bo_count > 0) {
        in_handles = calloc(in_bo_count, sizeof(uint32_t));
        if (!in_handles) return -ENOMEM;
        for (uint32_t i = 0; i < in_bo_count; i++) {
            in_handles[i] = in_bos[i]->handle;
        }
    }

    if (out_bo_count > 0) {
        out_handles = calloc(out_bo_count, sizeof(uint32_t));
        if (!out_handles) {
            free(in_handles);
            return -ENOMEM;
        }
        for (uint32_t i = 0; i < out_bo_count; i++) {
            out_handles[i] = out_bos[i]->handle;
        }
    }

    /* Build the task - one task pointing to our register commands */
    struct drm_rocket_task task = {
        .regcmd = regcmd_bo->dma_addr,
        .regcmd_count = regcmd_count,
    };

    /* Build the job */
    struct drm_rocket_job job = {
        .tasks = (uintptr_t)&task,
        .in_bo_handles = (uintptr_t)in_handles,
        .out_bo_handles = (uintptr_t)out_handles,
        .task_count = 1,
        .in_bo_handle_count = in_bo_count,
        .out_bo_handle_count = out_bo_count,
    };

    /* Build the submit */
    struct drm_rocket_submit submit = {
        .jobs = (uintptr_t)&job,
        .job_count = 1,
        .job_struct_size = sizeof(struct drm_rocket_job),
    };

    ret = ioctl(ctx->fd, DRM_IOCTL_ROCKET_SUBMIT, &submit);

    free(in_handles);
    free(out_handles);

    if (ret < 0) {
        perror("DRM_IOCTL_ROCKET_SUBMIT failed");
        return -errno;
    }

    return 0;
}

