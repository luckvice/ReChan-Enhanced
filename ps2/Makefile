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

PS2DEV ?= /usr/local/ps2dev
export PATH := $(PS2DEV)/ee/bin:$(PATH)

EE_BIN = $(TARGET).elf

# The step-1 synthetic harness (ps2/test_pddi_main.cpp) is kept as a regression
# entry for the GS backend — ps2/tools/verify-render.py checks its colour
# pattern — but it is not part of the game build. Select it instead of the engine
# with:
#   RECHAN_PS2_TARGET=rechan-pddi bash ps2/build-remote.sh
ifeq ($(TARGET),rechan-pddi)
SOURCES := $(ROOT)/ps2/test_pddi_main.cpp \
	$(ROOT)/vendor/libp3d/pddi/ps2/ps2render.cpp \
	$(ROOT)/vendor/libp3d/pddi/ps2/ps2gs.cpp
else

SRC_DIRS = \
	$(ROOT)/src/ai \
	$(ROOT)/src/fe \
	$(ROOT)/src/gen \
	$(ROOT)/src/radlib \
	$(ROOT)/src/snd \
	$(ROOT)/src/xclib \
	$(ROOT)/vendor/libp3d/p3d

SOURCES = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))
SOURCES += $(ROOT)/src/extra/cheats.cpp \
	$(ROOT)/src/extra/colorpulse.cpp \
	$(ROOT)/src/extra/customhudmgr.cpp \
	$(ROOT)/src/extra/customtext.cpp \
	$(ROOT)/src/extra/dialogpreview.cpp \
	$(ROOT)/src/extra/fecustommenumgr.cpp \
	$(ROOT)/src/extra/hudwsfix.cpp \
	$(ROOT)/src/extra/jsondataloader.cpp \
	$(ROOT)/src/extra/shadowcsm.cpp \
	$(ROOT)/src/extra/wavloader.cpp
SOURCES += $(ROOT)/src/pc/autotest.cpp \
	$(ROOT)/src/pc/coldebug.cpp \
	$(ROOT)/src/pc/controllerlight.cpp \
	$(ROOT)/src/pc/inputaction.cpp \
	$(ROOT)/src/pc/textbackend.cpp \
	$(ROOT)/src/pc/textmgr.cpp \
	$(ROOT)/src/pc/tim.cpp
SOURCES += $(ROOT)/src/ps2/ps2_audio.cpp \
	$(ROOT)/src/ps2/ps2_fileio.cpp \
	$(ROOT)/src/ps2/ps2_log.cpp \
	$(ROOT)/src/ps2/ps2_stubs.cpp \
	$(ROOT)/vendor/libp3d/pddi/ps2/ps2render.cpp \
	$(ROOT)/vendor/libp3d/pddi/ps2/ps2gs.cpp

# Filter out desktop-only and standalone test implementations
SOURCES := $(filter-out \
	$(ROOT)/vendor/libp3d/p3d/fileio.cpp \
	$(ROOT)/src/extra/psxdiscextractor.cpp \
	$(ROOT)/src/pc/audio.cpp \
	$(ROOT)/src/pc/crashreporter.cpp \
	$(ROOT)/src/pc/debugui.cpp \
	$(ROOT)/src/pc/log.cpp \
	$(ROOT)/src/pc/miniaudio_impl.cpp \
	$(ROOT)/src/pc/settings.cpp \
	$(ROOT)/src/radmovie/movieplayer.cpp \
	$(ROOT)/src/extra/assetexporter.cpp \
	$(ROOT)/src/extra/assetexporter_ui.cpp \
	$(ROOT)/src/extra/autoupdater.cpp \
	$(ROOT)/src/extra/gltfloader.cpp \
	$(ROOT)/src/extra/modloader.cpp \
	$(ROOT)/src/extra/parameterdata.cpp \
	$(ROOT)/src/extra/realtexture.cpp,$(SOURCES))

endif

EE_OBJS = $(SOURCES:.cpp=.o)

.DEFAULT_GOAL := $(EE_BIN)

GSKIT_DIR ?= $(PS2DEV)/gsKit
EE_INCS += -I$(ROOT) \
           -I$(ROOT)/src \
           -I$(ROOT)/vendor/libp3d \
           -I$(ROOT)/vendor/stb \
           -I$(GSKIT_DIR)/include
EE_LDFLAGS += -L$(GSKIT_DIR)/lib

EE_LIBS += -lgskit -ldmakit -lpad -ldebug -lstdc++ -lm

EE_CFLAGS += -DRC_PLATFORM_PS2 -DPS2 -DIMGUI_DISABLE -O2 -G0 -Wall
EE_CXXFLAGS += -DRC_PLATFORM_PS2 -DPS2 -DIMGUI_DISABLE -O2 -G0 -Wall \
               -Wno-sign-compare -Wno-unused-variable -Wno-unused-function \
               -Wno-strict-aliasing

EE_LDFLAGS += -Wl,-zmax-page-size=128

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
