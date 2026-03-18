# librocket

Userspace library for RK3588 NPU via mainline Rocket driver.

**⚠️ EXPERIMENTAL - NOT YET TESTED ON HARDWARE**

This library provides a C interface to the RK3588 NPU (Neural Processing Unit) through the mainline Rocket kernel driver. It is currently in development and has not been tested on actual RK3588 hardware. Testing and contributions are welcome!

## Overview

librocket is a userspace library that enables access to the RK3588 NPU for accelerated matrix operations. It is designed to work with the mainline Linux kernel's Rocket driver and provides the foundation for GPU-accelerated inference in GGML-based projects.

### Original Work Attribution

This library is derived from [mtx512/rk3588-npu](https://github.com/mtx512/rk3588-npu) by Jasbir Matharu (mtx512), who performed the original reverse engineering of the RK3588 NPU interface. We are grateful for this foundational work.

## Features

- **FP16 and INT8 matrix multiplication** - Support for both floating-point and integer quantized operations
- **Direct DRM/accel interface** - Uses the mainline Rocket kernel driver without requiring custom patches
- **No kernel modifications** - Works with standard mainline kernel 6.19+
- **pkg-config support** - Easy integration with CMake and other build systems
- **Silent device probing** - Non-intrusive device detection for graceful fallback

## Project Status

### Current State
- ✅ Backend skeleton with device enumeration
- ✅ GGML integration framework
- ✅ Operation dispatch framework
- ✅ Matmul operation validation
- ⏳ **Hardware testing pending** - Awaiting RK3588 device access
- ⏳ Actual NPU execution implementation
- ⏳ Performance optimization

### Known Limitations
- Not yet tested on RK3588 hardware
- CPU fallback path is functional but not optimized
- Dequantization functions are framework placeholders
- Matmul execution is framework placeholder

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

MIT License - See [LICENSE](LICENSE) file for details.

This project includes code derived from mtx512/rk3588-npu, which is licensed under GPL-3.0-or-later.

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

