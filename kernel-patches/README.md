# Kernel Patches Required for librocket

## Overview

The mainline Linux kernel Rocket driver (introduced in 6.18) has a critical bug that causes kernel crashes when using librocket. This directory contains patches that **must be applied** to your kernel for librocket to work.

## Required Patches

### 1. Fix NULL Pointer Dereference in IOMMU Domain Cleanup

**File**: `0001-rocket-fix-null-pointer-dereference-in-iommu-domain.patch`

**Kernel Versions Affected**: Linux 6.18, 6.19, 6.20 (and possibly later)

**Symptom**: Kernel crash on first NPU operation:
```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000008
pc : rocket_iommu_domain_put+0x20/0xa8 [rocket]
lr : rocket_job_cleanup+0x20/0x254 [rocket]
```

**Root Cause**: The `rocket_job_cleanup()` function calls `rocket_iommu_domain_put()` without checking if `job->domain` is NULL. If IOMMU domain allocation fails, the cleanup crashes.

**Fix**: Add NULL pointer check before calling `rocket_iommu_domain_put()`.

**Status**: 
- ✅ Tested on RK3588 (Orange Pi 5 Plus)
- ✅ Fixes kernel crash
- ✅ Enables successful NPU job submission
- ⏳ Pending upstream submission to LKML

## How to Apply Patches

### Method 1: Manual Patch Application

```bash
# Navigate to your kernel source directory
cd /path/to/linux-source

# Apply the patch
patch -p1 < /path/to/librocket/kernel-patches/0001-rocket-fix-null-pointer-dereference-in-iommu-domain.patch

# Rebuild the Rocket module
make M=drivers/accel/rocket modules

# Install the module
sudo rmmod rocket
sudo insmod drivers/accel/rocket/rocket.ko
```

### Method 2: Rebuild Entire Kernel

```bash
# Apply patch as above
patch -p1 < /path/to/librocket/kernel-patches/0001-rocket-fix-null-pointer-dereference-in-iommu-domain.patch

# Rebuild kernel
make -j$(nproc)
make modules_install
make install

# Reboot
sudo reboot
```

### Method 3: Yocto/OpenEmbedded

If using Yocto/OpenEmbedded, add the patch to your kernel recipe:

```bitbake
SRC_URI += "file://0001-rocket-fix-null-pointer-dereference-in-iommu-domain.patch"
```

See the [orangepi-cluster-playsafetest](https://github.com/johanvdb/orangepi-cluster-playsafetest) project for a complete Yocto example.

## Verification

After applying the patch and loading the module, verify it works:

```bash
# Check module is loaded
lsmod | grep rocket

# Check NPU devices are available
ls -la /dev/accel*

# Run librocket test (if built)
./matmul_fp16_test 4 32 16
```

Expected output:
- No kernel crash
- Job submission successful
- No "Unable to handle kernel NULL pointer dereference" in dmesg

## Upstream Status

**Status**: Patch pending submission to Linux Kernel Mailing List (LKML)

**Maintainers**:
- DRM subsystem maintainers
- Rockchip maintainers

**Tracking**: This patch will be submitted upstream once fully validated on hardware.

## Hardware Tested

- ✅ Orange Pi 5 Plus (RK3588)
- ✅ Kernel 6.19.6
- ✅ 3 NPU cores detected and working

## Additional Information

For more details about this bug and the fix, see:
- Bug report: [ROCKET_BUG_REPORT.md](../ROCKET_BUG_REPORT.md) (if available in your clone)
- Progress report: [ROCKET_PROGRESS_REPORT.md](../ROCKET_PROGRESS_REPORT.md) (if available)

## Contributing

If you encounter issues with these patches or have improvements:
1. Open an issue on the librocket GitHub repository
2. Submit a pull request with fixes
3. Report kernel version compatibility

## License

These patches are provided under the same license as the Linux kernel (GPL-2.0).

