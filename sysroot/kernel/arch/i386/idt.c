#include "../../include/idt.h"

static struct idt_entry idt[256];
static struct idt_ptr idt_reg;

extern void common_interrupt_handler(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void idt_init(void) {
    idt_reg.limit = (sizeof(struct idt_entry) * 256) - 1;
    idt_reg.base = (uint32_t)&idt;

    // Clear the IDT
    for(int i = 0; i < 256; i++) idt_set_gate(i, 0, 0, 0);

    // 0x8E = Present, Ring 0, 32-bit Interrupt Gate
    // 33 is Keyboard (IRQ1), 128 is our custom test (0x80)
    idt_set_gate(33, (uint32_t)common_interrupt_handler, 0x08, 0x8E);
    idt_set_gate(128, (uint32_t)common_interrupt_handler, 0x08, 0x8E);

    __asm__ volatile("lidt (%0)" : : "r"(&idt_reg));
}