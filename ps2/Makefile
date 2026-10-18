# ReChan — PlayStation 2 (Emotion Engine + Graphics Synthesizer) target.
#
# Build inside the official container (the image has no make; install it first):
#   docker run --rm -v "$PWD:/work" -w /work/ps2 ghcr.io/ps2dev/ps2dev:latest \
#     sh -c 'apk add --no-cache make >/dev/null && make'
#
# The EE toolchain is mips64r5900el-ps2-elf-; gsKit (the GS abstraction) lives
# in $PS2DEV/gsKit, outside ps2sdk.

TARGET ?= rechan

ROOT := $(abspath $(CURDIR)/..)

# The ps2dev container ships the toolchain in $PS2DEV/ee/bin but does not put it
# on PATH; without this, Makefile.pref cannot find mips64r5900el-ps2-elf-gcc.
PS2DEV ?= /usr/local/ps2dev
export PATH := $(PS2DEV)/ee/bin:$(PATH)

EE_BIN = $(TARGET).elf
EE_OBJS = main.o ps2render.o ps2gs.o

# The first rule below is the explicit ps2render.o rule and Makefile.eeglobal
# (included at the bottom) has no catch-all "all" target, so the default goal —
# the ELF — has to be stated here, after EE_BIN exists.
.DEFAULT_GOAL := $(EE_BIN)

# ps2sdk headers, gsKit and libp3d (the pddi interface the backend implements).
GSKIT_DIR ?= $(PS2DEV)/gsKit
EE_INCS += -I$(ROOT)/vendor/libp3d -I$(GSKIT_DIR)/include
EE_LDFLAGS += -L$(GSKIT_DIR)/lib

# gsKit + DMA + debug (scr_printf/TTY), the pad, and libstdc++ for the backend's
# std::vector/std::string/dynamic_cast.
EE_LIBS += -lgskit -ldmakit -lpad -ldebug -lstdc++

EE_CFLAGS += -O2 -G0 -Wall
EE_CXXFLAGS += -O2 -G0 -Wall
EE_LDFLAGS += -Wl,-zmax-page-size=128

# The pddi backend lives under vendor/, outside this directory, so it needs an
# explicit rule (the implicit %.o rules only cover sources next to the Makefile).
# ps2render.cpp is deliberately free of gsKit; ps2gs.cpp is where ps2sdk lives.
ps2render.o: $(ROOT)/vendor/libp3d/pddi/ps2/ps2render.cpp $(ROOT)/vendor/libp3d/pddi/ps2/ps2render.h
	$(EE_CXX) $(EE_CXXFLAGS) $(EE_INCS) -c $< -o $@

ps2gs.o: $(ROOT)/vendor/libp3d/pddi/ps2/ps2gs.cpp $(ROOT)/vendor/libp3d/pddi/ps2/ps2gs.h
	$(EE_CXX) $(EE_CXXFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
