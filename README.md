# ACOS

ACOS is short for "Advanced Control Operating System"
The basic goal for ACOS is to give as much control over the computer as possible.

*This is a hobby project that has literally no chance of surpassing Linux :(*


For anyone who wants to contribute:

Add assembly functions in src/asm_funcs/bios.asm for now

The bootloader (first 512 bytes) is all in boot.asm

The loaded code is all in C. Note that the bootloader actually needs to load this code for it to run.

Run `make` in the main directory, and build/os.img is the output file.

You can use qemu to test and debug os.img
