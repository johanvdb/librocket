# Rocket Matmul Testing Guide

## Overview

This is a port of [mtx512/rk3588-npu](https://github.com/mtx512/rk3588-npu) to use the 
mainline Rocket driver instead of the vendor RKNPU driver.

## Building

### Cross-compile for aarch64 (recommended)
```bash
make CROSS=aarch64-linux-gnu- clean all
```

### Native build on target
```bash
make clean all
```

## Prerequisites

1. **Kernel 6.19+ with Rocket driver enabled**
   - Edge kernel already has `CONFIG_DRM_ACCEL_ROCKET=m` via `rocket-npu.cfg`
   - The Rocket module must be loaded: `modprobe rocket`

2. **Device nodes present**
   - `/dev/accel/accel0` - Rocket NPU device

## Testing on Edge Kernel Nodes

### Option 1: Deploy Binary Only (No Kernel Changes)

The Rocket driver is already in the edge kernel. Just deploy the test binary:

```bash
# From build machine
scp matmul_fp16_test root@opiplus2-edge1:/tmp/

# On target
ssh root@opiplus2-edge1
modprobe rocket
ls -la /dev/accel/  # Should show accel0
/tmp/matmul_fp16_test 4 32 16
```

### Option 2: Include in Yocto Image

Add to a recipe in `meta-orangepi-cluster`:

```bitbake
# Example: recipes-rknpu/rocket-matmul/rocket-matmul.bb
DESCRIPTION = "Rocket NPU Matmul Library and Tests"
LICENSE = "GPL-3.0-or-later"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-3.0-or-later;md5=..."

SRC_URI = "file://rocket-matmul"
S = "${WORKDIR}/rocket-matmul"

inherit pkgconfig

do_compile() {
    oe_runmake
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 matmul_fp16_test ${D}${bindir}/
    install -d ${D}${libdir}
    install -m 0644 librocket_matmul.a ${D}${libdir}/
}
```

## Test Dimensions

The NPU has specific alignment requirements:
- **M** (rows of A): multiple of 4, or 1, max 384
- **K** (cols of A): multiple of 32, max 4096
- **N** (cols of B): multiple of 16, max 4096

Example test cases:
```bash
./matmul_fp16_test 4 32 16    # Minimum valid dimensions
./matmul_fp16_test 8 64 32    # Small test
./matmul_fp16_test 128 512 256  # Medium test
./matmul_fp16_test 384 4096 4096  # Maximum test
```

## Debugging

### Check if Rocket driver is loaded
```bash
lsmod | grep rocket
cat /sys/class/accel/accel0/device/uevent
```

### Check DMA allocations
```bash
cat /sys/kernel/debug/dma_buf/bufinfo
```

### Kernel logs
```bash
dmesg | grep -i rocket
```

## Known Issues

1. **mmap offset handling**: The current implementation mmaps immediately in `rocket_bo_create`. 
   If this fails, check kernel logs for DMA allocation errors.

2. **Register command format**: The regcmd_count passed to submit excludes PC header/footer 
   commands. If NPU hangs, verify the command count calculation.

3. **Cache coherency**: The `rocket_bo_fini` and `rocket_bo_prep` calls handle CPU→device 
   and device→CPU sync. Missing these calls will cause incorrect results.

## Next Steps

1. Test on actual edge kernel node
2. Fix any runtime issues
3. Add INT8 matmul test
4. Profile performance vs vendor RKLLM
5. Integrate with ggml-rocket backend

