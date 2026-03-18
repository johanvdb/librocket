# librocket

Userspace library for RK3588 NPU via mainline Rocket driver.

Ported from [mtx512/rk3588-npu](https://github.com/mtx512/rk3588-npu) (GPL-3.0).

## Features

- FP16 and INT8 matrix multiplication
- Direct DRM/accel interface to Rocket kernel driver
- No kernel patches required
- Works with mainline kernel 6.19+

## Building

### Prerequisites

- aarch64 cross-compiler (e.g., `aarch64-linux-gnu-gcc`)
- Make

### Build

```bash
make CROSS=aarch64-linux-gnu-
```

### Native build on target

```bash
make
```

## Testing

```bash
./matmul_fp16_test 4 32 16
```

Test dimensions must follow NPU alignment requirements:
- M (rows): multiple of 4, or 1
- K (cols of A): multiple of 32
- N (cols of B): multiple of 16

## License

GPL-3.0-or-later

