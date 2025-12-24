BOOT = build/boot.img
KERNEL = build/main.bin
IMG = build/os.img

all: $(IMG)

# Ensure build directory exists
build:
	mkdir -p build

# Stage 1 bootloader
$(BOOT): src/boot.asm | build
	nasm -f bin $< -o $@

# Assemble NASM BIOS routines
build/bios.o: src/asm_funcs/bios.asm | build
	nasm -f elf32 $< -o $@

# Compile stage 2 C code
build/main.o: src/main.c | build
	gcc -ffreestanding -m32 -nostdlib -fno-pic -Iinclude -c $< -o $@

# Link stage 2 kernel
$(KERNEL): build/main.o build/bios.o | build
	ld -m elf_i386 -T link.ld build/main.o build/bios.o -o build/main.bin
# Final disk image
$(IMG): $(BOOT) $(KERNEL)
	cat $(BOOT) $(KERNEL) > $@

clean:
	rm -rf build/*
