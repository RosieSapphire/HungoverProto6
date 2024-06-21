DEBUG := 1
BUILD_DIR := build

include $(N64_INST)/include/n64.mk
include $(T3D_INST)/t3d.mk

MKSPRITE_FLAGS ?= -c 2
AUDIOCONV_FLAGS ?= --wav-compress 1

TARGET := hungover_proto6
ROM := $(TARGET).z64
ELF := $(BUILD_DIR)/$(TARGET).elf
DFS := $(BUILD_DIR)/$(TARGET).dfs

INC_DIRS := include include/engine include/game
SRC_DIRS := src src/engine src/game
H_FILES := $(foreach dir,$(INC_DIRS),$(wildcard $(dir)/*.h))
C_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
O_FILES := $(C_FILES:%.c=$(BUILD_DIR)/%.o)
DEP_FILES := $(O_FILES:%.o=%.d)

ASSETS_PNG := $(wildcard assets/*.png)
ASSETS_WAV := $(wildcard assets/*.wav)
ASSETS_GLB := $(wildcard assets/*.glb)
ASSETS_CONV := $(ASSETS_PNG:assets/%.png=filesystem/%.sprite) \
	       $(ASSETS_WAV:assets/%.wav=filesystem/%.wav64) \
	       $(ASSETS_GLB:assets/%.glb=filesystem/%.t3dm)

N64_CFLAGS += $(INC_DIRS:%=-I%)
ifeq ($(DEBUG), 1)
N64_CFLAGS += -O0 -g -DDEBUG -DDEBUG_MODE=1
N64_LDFLAGS += -g
else
N64_CFLAGS += -Os -DDEBUG_MODE=0
endif

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o filesystem "$<"

filesystem/%.wav64: assets/%.wav
	@mkdir -p $(dir $@)
	@echo "    [SAMPLE] $@"
	$(N64_AUDIOCONV) $(AUDIOCONV_FLAGS) -o filesystem "$<"

filesystem/%.t3dm: assets/%.glb
	@mkdir -p $(dir $@)
	@echo "    [T3D-MODEL] $@"
	$(T3D_GLTF_TO_3D) "$<" $@
	$(N64_BINDIR)/mkasset -c 2 -o filesystem $@

$(ROM): N64_ROM_TITLE="Hungover (Proto 6)"
$(ROM): $(DFS)
$(ROM): $(ELF)
$(ELF): $(O_FILES)
$(DFS): $(ASSETS_CONV)

clean:
	rm -rf $(ROM) $(BUILD_DIR)/ filesystem/ compile_commands.json \
		.cache/ .checkpatch*

-include $(DEP_FILES)

FORMAT_SCAN := $(H_FILES) $(C_FILES)

format:
	clang-format -i --style=file $(FORMAT_SCAN)
