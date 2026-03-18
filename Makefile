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

# Installation prefix
PREFIX ?= /usr/local
VERSION ?= 1.0.0

# Sources
LIB_SRCS = src/rocket_interface.c src/npu_matmul.c
TEST_SRCS = tests/matmul_fp16_rocket.c
MINIMAL_TEST_SRCS = tests/minimal_npu_test.c

# Objects
LIB_OBJS = $(LIB_SRCS:.c=.o)
TEST_OBJS = $(TEST_SRCS:.c=.o)
MINIMAL_TEST_OBJS = $(MINIMAL_TEST_SRCS:.c=.o)

# Targets
LIB = librocket.a
TEST = matmul_fp16_test
MINIMAL_TEST = minimal_npu_test
PKG_CONFIG = rocket.pc

.PHONY: all clean install

all: $(LIB) $(TEST) $(MINIMAL_TEST) $(PKG_CONFIG)

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(TEST): $(TEST_OBJS) $(LIB)
	$(CC) -o $@ $^ $(LDFLAGS)

$(MINIMAL_TEST): $(MINIMAL_TEST_OBJS) $(LIB)
	$(CC) -o $@ $^ $(LDFLAGS)

$(PKG_CONFIG): rocket.pc.in
	sed -e 's|@PREFIX@|$(PREFIX)|g' -e 's|@VERSION@|$(VERSION)|g' $< > $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(LIB_OBJS) $(TEST_OBJS) $(MINIMAL_TEST_OBJS) $(LIB) $(TEST) $(MINIMAL_TEST) $(PKG_CONFIG)

install: $(LIB) $(PKG_CONFIG)
	install -d $(PREFIX)/lib
	install -d $(PREFIX)/include
	install -d $(PREFIX)/lib/pkgconfig
	install -m 0644 $(LIB) $(PREFIX)/lib/
	install -m 0644 include/rocket_interface.h $(PREFIX)/include/
	install -m 0644 include/npu_matmul.h $(PREFIX)/include/
	install -m 0644 include/npu_hw.h $(PREFIX)/include/
	install -m 0644 include/npu_cna.h $(PREFIX)/include/
	install -m 0644 include/npu_dpu.h $(PREFIX)/include/
	install -m 0644 $(PKG_CONFIG) $(PREFIX)/lib/pkgconfig/
	ldconfig

# Dependencies
src/rocket_interface.o: include/rocket_interface.h include/rocket_accel.h
src/npu_matmul.o: include/npu_matmul.h include/npu_hw.h include/npu_cna.h include/npu_dpu.h
tests/matmul_fp16_rocket.o: include/rocket_interface.h include/npu_matmul.h
tests/minimal_npu_test.o: include/rocket_interface.h include/npu_hw.h

