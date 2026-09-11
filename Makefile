CC = clang
AS = nasm
LD = ld
OBJCOPY = objcopy

CFLAGS = -mrtd --target=i686-elf -ffreestanding -fno-pie -fno-stack-protector
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld

BUILD = build
SRC = src

all: $(BUILD)/os.img

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/os.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $^ > $@
	truncate -s 10K $@
	# just align to disk sectors

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $(BUILD)/kernel.elf $(BUILD)/kernel.bin

$(BUILD)/kernel.elf: $(BUILD)/kernel_asm.o $(BUILD)/kernel.o linker.ld
	$(LD) $(LDFLAGS) -o $(BUILD)/kernel.elf $(BUILD)/kernel_asm.o $(BUILD)/kernel.o

$(BUILD)/boot.bin: $(SRC)/boot.asm | $(BUILD)
	nasm -f bin $(SRC)/boot.asm -o $(BUILD)/boot.bin

$(BUILD)/kernel_asm.o: $(SRC)/kernel.asm | $(BUILD)
	nasm -f elf32 $(SRC)/kernel.asm -o $(BUILD)/kernel_asm.o

$(BUILD)/kernel.o: $(SRC)/kernel.c | $(BUILD)
	$(CC) $(CFLAGS) \
		-c $(SRC)/kernel.c -o $(BUILD)/kernel.o

clean:
	rm -rf $(BUILD)