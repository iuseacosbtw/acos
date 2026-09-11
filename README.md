# ACOS

ACOS is short for "Accelerated Operating System V3" (V1 was AAOS, and V2 was ABOS)

The basic goal for ACOS is to give as much control over the computer as possible.

*This is not a serious operating system, nor does it contain any Linux code*


I don't really intend to explain how everything works yet because the kernel hasn't even reached a working state yet, but you can mess around with the code if you want to. Run `make` to create build/os.img, which you can then run with `qemu-system-x86_64 build/os.img`.

You can also flash this .img file onto a computer with legacy boot enabled, but I HIGHLY ADVISE AGAINST doing this because again, the kernel hasn't reached a working state yet.
