# SPDX-License-Identifier: GPL-3.0-or-later
# Rocket NPU Matmul Library - Build for aarch64
#
# Usage:
#   make                 - Build all (native)
#   make CROSS=aarch64-linux-gnu-  - Cross-compile for aarch64
#   make clean           - Clean build artifacts

CROSS ?=
CC = $(CROSS)gcc
AR = $(CROSS)ar

CFLAGS = -Wall -Wextra -O2 -g -fPIC -I./include
LDFLAGS = -lm

# Sources
LIB_SRCS = src/rocket_interface.c src/npu_matmul.c
TEST_SRCS = tests/matmul_fp16_rocket.c

# Objects
LIB_OBJS = $(LIB_SRCS:.c=.o)
TEST_OBJS = $(TEST_SRCS:.c=.o)

# Targets
LIB = librocket.a
TEST = matmul_fp16_test

.PHONY: all clean

all: $(LIB) $(TEST)

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(TEST): $(TEST_OBJS) $(LIB)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(LIB_OBJS) $(TEST_OBJS) $(LIB) $(TEST)

install: $(LIB)
	install -d /usr/local/lib
	install -d /usr/local/include
	install -m 0644 $(LIB) /usr/local/lib/
	install -m 0644 include/rocket_interface.h /usr/local/include/
	install -m 0644 include/npu_matmul.h /usr/local/include/
	install -m 0644 include/npu_hw.h /usr/local/include/
	install -m 0644 include/npu_cna.h /usr/local/include/
	install -m 0644 include/npu_dpu.h /usr/local/include/
	ldconfig

# Dependencies
src/rocket_interface.o: include/rocket_interface.h include/rocket_accel.h
src/npu_matmul.o: include/npu_matmul.h include/npu_hw.h include/npu_cna.h include/npu_dpu.h
tests/matmul_fp16_rocket.o: include/rocket_interface.h include/npu_matmul.h

