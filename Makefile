# =========================================================================
# GameKit — GLES2 game framework for Raspberry Pi (Dispmanx, no Xorg)
#
#   make            build libgk.a + demo
#   make run        build and run the demo
#   make clean      remove build artifacts
#
# Cross-compiling from a PC:
#   make CROSS=arm-linux-gnueabihf-
#
# If your /opt/vc only exposes libbrcmEGL/libbrcmGLESv2:
#   make BRCM=1
# =========================================================================

CROSS     ?=
BRCM      ?=
VC_PREFIX ?= /opt/vc

ifeq ($(CROSS),)
CC ?= gcc
AR ?= ar
else
CC := $(CROSS)gcc
AR := $(CROSS)ar
endif

# Pi 1 / Zero = ARM1176JZF-S (armv6j, VFPv2, hard-float Raspbian)
MACH := $(shell $(CC) -dumpmachine 2>/dev/null)
ifneq (,$(findstring arm,$(MACH)))
  ARCH_FLAGS := -march=armv6j -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard
endif

BUILD   := build
CFLAGS  ?= -O2 -g -std=gnu99 -Wall -Wextra -Wno-unused-parameter $(ARCH_FLAGS)
CPPFLAGS := -Iinclude \
            -I$(VC_PREFIX)/include \
            -I$(VC_PREFIX)/include/interface/vcos/pthreads \
            -I$(VC_PREFIX)/include/interface/vmcs_host

LDLIBS := -lasound -lm -lpthread
ifeq ($(BRCM),1)
  LDLIBS += -lbrcmEGL -lbrcmGLESv2
else
  LDLIBS += -lEGL -lGLESv2
endif
LDLIBS += -lbcm_host
LDFLAGS := -L$(VC_PREFIX)/lib -Wl,-rpath,$(VC_PREFIX)/lib

GK_SRC   := $(wildcard src/*.c)
GK_OBJ   := $(patsubst src/%.c,$(BUILD)/%.o,$(GK_SRC))
DEMO_OBJ := $(BUILD)/demo.o

.PHONY: all demo lib run clean
all: demo

lib: $(BUILD)/libgk.a
demo: $(BUILD)/demo

$(BUILD):
        mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
        $(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD)/libgk.a: $(GK_OBJ)
        $(AR) rcs $@ $^

$(BUILD)/demo.o: examples/demo/main.c | $(BUILD)
        $(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD)/demo: $(DEMO_OBJ) $(BUILD)/libgk.a
        $(CC) $(LDFLAGS) -o $@ $(DEMO_OBJ) $(BUILD)/libgk.a $(LDLIBS)

run: demo
        ./$(BUILD)/demo

clean:
        rm -rf $(BUILD)
