# ReChan — PlayStation 2 (Emotion Engine + Graphics Synthesizer) target.
#
# Build inside the official container (the image has no make; install it first):
#   docker run --rm -v "$PWD:/work" -w /work/ps2 ghcr.io/ps2dev/ps2dev:latest \
#     sh -c 'apk add --no-cache make >/dev/null && make'
#
# The EE toolchain is mips64r5900el-ps2-elf-; gsKit (the GS abstraction) lives
# in $PS2DEV/gsKit, outside ps2sdk.

TARGET ?= rechan

# The ps2dev container ships the toolchain in $PS2DEV/ee/bin but does not put it
# on PATH; without this, Makefile.pref cannot find mips64r5900el-ps2-elf-gcc.
PS2DEV ?= /usr/local/ps2dev
export PATH := $(PS2DEV)/ee/bin:$(PATH)

EE_BIN = $(TARGET).elf
EE_OBJS = main.o

# gsKit headers/libs.
GSKIT_DIR ?= $(PS2DEV)/gsKit
EE_INCS += -I$(GSKIT_DIR)/include
EE_LDFLAGS += -L$(GSKIT_DIR)/lib

# gsKit + DMA + debug (scr_printf/TTY). Order matters: gskit before dmakit.
EE_LIBS += -lgskit -ldmakit -ldebug

EE_CFLAGS += -O2 -G0 -Wall
EE_CXXFLAGS += -O2 -G0 -Wall
EE_LDFLAGS += -Wl,-zmax-page-size=128

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
