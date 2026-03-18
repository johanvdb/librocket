#!/bin/bash
set -e

CROSS=${CROSS:-aarch64-linux-gnu-}

echo "Building librocket..."
make CROSS=$CROSS clean all

echo "Build complete!"
echo "Binary: matmul_fp16_test"

