#include <stdint.h>

void pic_remap(void) {
    // ICW1 - Initialize
    __asm__ volatile("outb %%al, $0x20" : : "a"(0x11));
    __asm__ volatile("outb %%al, $0xA0" : : "a"(0x11));

    // ICW2 - Remap offset (Master to 0x20, Slave to 0x28)
    __asm__ volatile("outb %%al, $0x21" : : "a"(0x20));
    __asm__ volatile("outb %%al, $0xA1" : : "a"(0x28));

    // ICW3 - Cascading
    __asm__ volatile("outb %%al, $0x21" : : "a"(0x04));
    __asm__ volatile("outb %%al, $0xA1" : : "a"(0x02));

    // ICW4 - Environment info
    __asm__ volatile("outb %%al, $0x21" : : "a"(0x01));
    __asm__ volatile("outb %%al, $0xA1" : : "a"(0x01));

    // Mask all interrupts except Keyboard (IRQ 1)
    __asm__ volatile("outb %%al, $0x21" : : "a"(0xFD)); // 1111 1101 (Only IRQ 1)
    __asm__ volatile("outb %%al, $0xA1" : : "a"(0xFF));
}