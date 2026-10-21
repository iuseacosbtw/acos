CC = clang
AS = nasm
LD = ld
OBJCOPY = objcopy

CFLAGS = -I./include -Os -Wno-pointer-sign --target=i686-elf -march=i686 -mno-sse -mno-sse2 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld

BUILD = build
SRC = src

all: $(BUILD)/os.img

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/os.img: $(BUILD)/os.bin $(BUILD)/filesystem.bin
	cat $^ > $@
	truncate -s 80K $@

$(BUILD)/os.bin: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $^ > $@
	truncate -s 40K $@

$(BUILD)/filesystem.bin: $(SRC)/filesystem.asm
	nasm -f bin $^ -o $@

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $(BUILD)/kernel.elf $(BUILD)/kernel.bin

$(BUILD)/kernel.elf: $(BUILD)/kernel_asm.o $(BUILD)/kernel.o $(BUILD)/ahci.o $(BUILD)/math.o $(BUILD)/file.o linker.ld
	$(LD) $(LDFLAGS) -o $(BUILD)/kernel.elf $(BUILD)/kernel_asm.o $(BUILD)/kernel.o $(BUILD)/ahci.o $(BUILD)/math.o $(BUILD)/file.o

$(BUILD)/boot.bin: $(SRC)/boot.asm | $(BUILD)
	nasm -f bin $(SRC)/boot.asm -o $(BUILD)/boot.bin

$(BUILD)/kernel_asm.o: $(SRC)/kernel.asm | $(BUILD)
	nasm -f elf32 $(SRC)/kernel.asm -o $(BUILD)/kernel_asm.o

$(BUILD)/ahci.o: $(SRC)/ahci.c | $(BUILD)
	$(CC) $(CFLAGS) \
		-c $(SRC)/ahci.c -o $(BUILD)/ahci.o

$(BUILD)/math.o: $(SRC)/math.c | $(BUILD)
	$(CC) $(CFLAGS) \
		-c $(SRC)/math.c -o $(BUILD)/math.o

$(BUILD)/file.o: $(SRC)/file.c | $(BUILD)
	$(CC) $(CFLAGS) \
		-c $(SRC)/file.c -o $(BUILD)/file.o

$(BUILD)/kernel.o: $(SRC)/kernel.c | $(BUILD)
	$(CC) $(CFLAGS) \
		-c $(SRC)/kernel.c -o $(BUILD)/kernel.o

clean:
	rm -rf $(BUILD)
