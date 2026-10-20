NDS_TOOLS_ROOT ?= /workspace/nds-tools
NDS_CC := $(shell cat $(NDS_TOOLS_ROOT)/compiler.path 2>/dev/null || printf /workspace/gba-tools/sysroot/usr/bin/arm-none-eabi-gcc)
NDS_SDK := $(NDS_TOOLS_ROOT)/src/libnds
NDS_CRT := $(NDS_TOOLS_ROOT)/src/devkitarm-crtls
NEWLIB := $(NDS_TOOLS_ROOT)/sysroot/usr/lib/arm-none-eabi/newlib/thumb/nofp
NDSTOOL := $(NDS_TOOLS_ROOT)/bin/ndstool
INCLUDES := -Isrc -Iplatform -Ibuild -I$(NDS_SDK)/include -I$(NDS_SDK)/source/common -I$(NDS_TOOLS_ROOT)/sysroot/usr/include/newlib
COMMON := -g -O2 -ffunction-sections -fdata-sections -fno-strict-aliasing -mthumb -mthumb-interwork $(INCLUDES)
CFLAGS := $(COMMON) -marm -O3 -mcpu=arm946e-s -DARM9 -Wall -Wextra
ARM7_FLAGS := $(COMMON) -mcpu=arm7tdmi -DARM7
LINK := -nostartfiles -L$(NEWLIB) -L$(NDS_CRT) -Wl,--gc-sections,--no-warn-rwx-segments
ARM9_OBJECTS := $(patsubst src/%.c,build/%.o,$(wildcard src/*.c)) build/assets.o build/platform.o build/runtime9.o build/start9.o

.PHONY: all setup assets clean test
.DEFAULT_GOAL := all
all: build/Coastline.nds

setup:
	bash tools/setup_nds.sh

build:
	mkdir -p build

assets: build/assets.c build/assets.h build/vehicle_geometry.h
build/assets.c build/assets.h build/vehicle_geometry.h &: tools/build_assets.py tools/vehicle_details.py tools/city_art.py tools/city_environment.py tools/city_mesh_assets.py $(wildcard art/city/*.png) tools/overhead_assets.py tools/overhead_materials.py $(wildcard art/overhead/*.png) art/overhead/materials.json art/materials/vehicle-materials.png art/materials/vehicle-materials.json $(wildcard art/*.png) | build
	python3 tools/build_assets.py

build/%.o: src/%.c build/assets.h build/vehicle_geometry.h $(wildcard src/*.h) platform/platform.h | build
	$(NDS_CC) $(CFLAGS) -MMD -MP -c $< -o $@
build/assets.o: build/assets.c build/assets.h | build
	$(NDS_CC) $(CFLAGS) -c $< -o $@
build/platform.o: platform/platform.c platform/platform.h | build
	$(NDS_CC) $(CFLAGS) -c $< -o $@
build/runtime9.o: platform/runtime.c | build
	$(NDS_CC) $(CFLAGS) -c $< -o $@
build/runtime7.o: platform/runtime.c | build
	$(NDS_CC) $(ARM7_FLAGS) -c $< -o $@
build/arm7.o: platform/arm7.c | build
	$(NDS_CC) $(ARM7_FLAGS) -c $< -o $@
build/start9.o: $(NDS_CRT)/ds_arm9_crt0.s | build
	$(NDS_CC) $(CFLAGS) -marm -x assembler-with-cpp -c $< -o $@
build/start7.o: $(NDS_CRT)/ds_arm7_crt0.s | build
	$(NDS_CC) $(ARM7_FLAGS) -marm -x assembler-with-cpp -c $< -o $@

build/arm9.elf: $(ARM9_OBJECTS)
	$(NDS_CC) -mthumb -mcpu=arm946e-s $(LINK) -T $(NDS_CRT)/ds_arm9.mem -T $(NDS_CRT)/ds_arm9.ld -Wl,-Map,build/arm9.map -o $@ $^ -Wl,--start-group $(NDS_TOOLS_ROOT)/lib/libnds9.a -lc -lm -lgcc -Wl,--end-group
build/arm7.elf: build/start7.o build/runtime7.o build/arm7.o
	$(NDS_CC) -mthumb -mcpu=arm7tdmi $(LINK) -T $(NDS_CRT)/ds_arm7.ld -Wl,-Map,build/arm7.map -o $@ $^ -Wl,--start-group $(NDS_TOOLS_ROOT)/lib/libnds7.a -lc -lgcc -Wl,--end-group
build/Coastline.nds: build/arm9.elf build/arm7.elf platform/finalize_rom.py
	$(NDSTOOL) -c $@ -9 build/arm9.elf -7 build/arm7.elf -g KCWE 00 COASTLINE
	python3 platform/finalize_rom.py $@

test: all
	python3 tests/playthrough.py

clean:
	rm -rf build

-include $(wildcard build/*.d)
