CC = clang
LD = ld
OBJCOPY = objcopy

CFLAGS = -I./include -Os -Wframe-larger-than=65536 --target=i686-elf -mno-sse -mno-sse2 -ffreestanding -fno-pie -fno-stack-protector -ffunction-sections # -mllvm -disable-loop-idiom-all
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

BUILD = build
SRC = src

all: $(BUILD)/program.ace

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/program.ace: $(BUILD)/program.elf
	$(OBJCOPY) -O binary $(BUILD)/program.elf $(BUILD)/program.ace
	sed -i '1s/^/ACOS/' $(BUILD)/program.ace

$(BUILD)/program.elf: $(BUILD)/program.o | $(BUILD)
	$(LD) $(LDFLAGS) $(BUILD)/program.o -o $(BUILD)/program.elf

$(BUILD)/program.o: $(SRC)/program.c | $(BUILD)
	$(CC) $(CFLAGS) -c $(SRC)/program.c -o $(BUILD)/program.o

clean:
	rm -rf $(BUILD)
