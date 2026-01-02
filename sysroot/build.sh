#!/bin/bash

# 1. Path to your i686 cross-compiler
PREFIX="/home/maxine/opt/cross/bin"
CC="$PREFIX/i686-elf-gcc"
# We use the cross-compiler for the app too!
OBJCOPY="$PREFIX/i686-elf-objcopy"

echo "--- Recompiling Cadence OS ---"

# 2. Clean up old files
rm -f *.o cadence.kernel

# 3. Assemble Architecture logic
echo "Assembling..."
$CC -c kernel/arch/i386/boot.S -o boot.o
$CC -c kernel/arch/i386/interrupt.S -o interrupt_asm.o

# 4. Compile C Source Files
echo "Compiling C files..."
$CC -c kernel/arch/i386/idt.c -o idt.o -std=gnu11 -ffreestanding -O2 -Ikernel/include
$CC -c kernel/arch/i386/pic.c -o pic.o -std=gnu11 -ffreestanding -O2 -Ikernel/include
$CC -c kernel/kernel/interrupt_handler.c -o interrupt_handler.o -std=gnu11 -ffreestanding -O2 -Ikernel/include
$CC -c kernel/kernel.c -o kernel.o -std=gnu11 -ffreestanding -O2 -Ikernel/include

echo "Compiling Snake App..."
$CC -c kernel/apps/snake.c -o snake.o -std=gnu11 -ffreestanding -O2 -fno-pic
$CC -T kernel/apps/apps.ld -o snake.elf -ffreestanding -nostdlib snake.o -lgcc
$OBJCOPY -O binary snake.elf snake.link

# 5. Link with libgcc
echo "Linking Kernel..."
$CC -T kernel/arch/i386/linker.ld -o cadence.kernel -ffreestanding -O2 -nostdlib \
boot.o interrupt_asm.o idt.o pic.o interrupt_handler.o kernel.o -lgcc
# Inject snake.link into the disk image
if [ -f snake.link ]; then
    echo "Registering and Injecting snake.link via build_disk.py..."
    # Ensure this script points to cadence_disk.img and snake.link
    python3 build_disk.py
else
    echo "Build Error: snake.link not found."
fi

# 6. Launch with existing disk image
if [ -f cadence.kernel ]; then
    echo "--- Build Successful: Launching ---"
    qemu-system-i386 -kernel cadence.kernel -hda cadence_disk.img
else
    echo "Build FAILED. Check the errors above."
fi
