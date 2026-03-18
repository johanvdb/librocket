# librocket

Userspace library for RK3588 NPU via mainline Rocket driver.

**⚠️ EXPERIMENTAL - HARDWARE TESTING IN PROGRESS**

**🔧 KERNEL PATCH REQUIRED**: The mainline Rocket driver has a critical bug. You **must** apply the kernel patch in `kernel-patches/` before using librocket. See [kernel-patches/README.md](kernel-patches/README.md) for details.

This library provides a C interface to the RK3588 NPU (Neural Processing Unit) through the mainline Rocket kernel driver. Hardware testing is currently in progress on Orange Pi 5 Plus (RK3588). Contributions are welcome!

## Overview

librocket is a userspace library that enables access to the RK3588 NPU for accelerated matrix operations. It is designed to work with the mainline Linux kernel's Rocket driver and provides the foundation for GPU-accelerated inference in GGML-based projects.

### Original Work Attribution

This library is derived from [mtx512/rk3588-npu](https://github.com/mtx512/rk3588-npu) by Jasbir Matharu (mtx512), who performed the original reverse engineering of the RK3588 NPU interface. We are grateful for this foundational work.

## ⚠️ Prerequisites

### Kernel Patch Required

The mainline Linux Rocket driver (kernel 6.18+) has a critical NULL pointer dereference bug that causes kernel crashes. You **must** apply the patch before using librocket:

1. See [kernel-patches/README.md](kernel-patches/README.md) for detailed instructions
2. Apply `kernel-patches/0001-rocket-fix-null-pointer-dereference-in-iommu-domain.patch`
3. Rebuild and install the Rocket kernel module

**Without this patch, your kernel will crash on the first NPU operation.**

## Features

- **FP16 and INT8 matrix multiplication** - Support for both floating-point and integer quantized operations
- **Direct DRM/accel interface** - Uses the mainline Rocket kernel driver
- **Kernel patch provided** - Fix for mainline kernel bug included
- **pkg-config support** - Easy integration with CMake and other build systems
- **Silent device probing** - Non-intrusive device detection for graceful fallback

## Project Status

### Current State
- ✅ Backend skeleton with device enumeration
- ✅ GGML integration framework
- ✅ Operation dispatch framework
- ✅ Matmul operation validation
- ✅ **Kernel bug fixed** - NULL pointer dereference patch provided
- ✅ **Hardware testing in progress** - Orange Pi 5 Plus (RK3588)
- ✅ **NPU job submission working** - Device detection and buffer management functional
- ⏳ NPU execution implementation (returns zeros currently)
- ⏳ Performance optimization

### Known Limitations
- NPU execution returns zeros (register commands need implementation)
- CPU fallback path is functional but not optimized
- Dequantization functions are framework placeholders
- Requires kernel patch for mainline Rocket driver

## Integration with GGML and llama.cpp

librocket is integrated into the following projects:

- **GGML Backend**: [johanvdb/ggml](https://github.com/johanvdb/ggml) (branch: `rocket-backend`)
  - Provides GGML backend registration and device management
  - Implements operation dispatch for tensor operations

- **llama.cpp Integration**: [johanvdb/llama.cpp](https://github.com/johanvdb/llama.cpp) (branch: `rocket-integration`)
  - Integrates Rocket backend into llama.cpp inference engine
  - Enables device selection via `--device` flag
  - Supports layer offloading to NPU

## Building

### Prerequisites

- aarch64 cross-compiler (e.g., `aarch64-linux-gnu-gcc`)
- Make
- pkg-config (for library discovery)

### Cross-compile for RK3588

```bash
make CROSS=aarch64-linux-gnu-
```

### Native build on target

```bash
make
```

### Installation

```bash
make install PREFIX=/usr/local
```

This installs:
- `librocket.a` - Static library
- `rocket_interface.h` - Public header
- `rocket.pc` - pkg-config file

## Testing

### Unit Tests

```bash
./matmul_fp16_test 4 32 16
```

Test dimensions must follow NPU alignment requirements:
- M (rows): multiple of 4, or 1
- K (cols of A): multiple of 32
- N (cols of B): multiple of 16

### Integration Testing

See [TESTING.md](TESTING.md) for hardware testing procedures.

## License

**GPL-3.0-or-later** - See [LICENSE](LICENSE) file for details.

This project is licensed under the GNU General Public License v3 or later to maintain compatibility with the original work from mtx512/rk3588-npu, which is also licensed under GPL-3.0-or-later. This ensures that all derivative works remain free and open source.

The GPL is a strong copyleft license that promotes software freedom and community collaboration.

## Contributing

Contributions are welcome! Please feel free to:
- Report issues and bugs
- Submit pull requests with improvements
- Test on RK3588 hardware and provide feedback
- Help optimize performance

## References

- [RK3588 NPU Reverse Engineering](https://github.com/mtx512/rk3588-npu) - Original work by mtx512
- [GGML Project](https://github.com/ggerganov/ggml) - Machine learning inference framework
- [llama.cpp](https://github.com/ggerganov/llama.cpp) - LLM inference engine

