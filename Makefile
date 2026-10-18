TOOLCHAIN ?= /workspace/gba-tools/sysroot/usr/bin/arm-none-eabi-
CC := $(TOOLCHAIN)gcc
OBJCOPY := $(TOOLCHAIN)objcopy
CFLAGS := -mcpu=arm7tdmi -mthumb -O2 -ffreestanding -fno-builtin -fno-strict-aliasing -fdata-sections -ffunction-sections -Wall -Wextra -Wno-misleading-indentation -Ibuild
LDFLAGS := -nostdlib -Wl,-T,src/gba.ld,--gc-sections,-Map,build/bramblebound.map

.PHONY: all clean test
all: build/Bramblebound.gba

build/assets.h: tools/assets.py tools/art_overhaul.py
	mkdir -p build
	python3 tools/assets.py

build/start.o: src/start.s
	mkdir -p build
	$(CC) -mcpu=arm7tdmi -c $< -o $@

build/game.o: src/game.c src/world_render.inc src/ui_render.inc build/assets.h
	$(CC) $(CFLAGS) -c $< -o $@

build/bramblebound.elf: build/start.o build/game.o src/gba.ld
	$(CC) -mcpu=arm7tdmi -mthumb $(LDFLAGS) build/start.o build/game.o -lgcc -o $@

build/Bramblebound.gba: build/bramblebound.elf tools/fix_rom.py
	$(OBJCOPY) -O binary $< $@
	python3 tools/fix_rom.py $@

test: all
	GBA_TOOLCHAIN="$(TOOLCHAIN)" python3 tests/playthrough.py

clean:
	rm -rf build
