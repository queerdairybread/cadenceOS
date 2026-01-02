#include <stdint.h>

extern void idt_init();
extern void pic_remap();
extern void update_cursor(int x, int y);
extern void clear_screen();
extern void draw_boot_splash();
extern void shell_print(const char* str, uint8_t color);

// Add this line to the top of kernel.c
extern void draw_boot_splash();

void kernel_main(void) {
    // 1. Core Hardware Setup
    idt_init();
    
    // Remap the Programmable Interrupt Controller (PIC)
    pic_remap();

    // 2. Visual Setup
    clear_screen();
    draw_boot_splash();

    // 3. System Status
    shell_print("Interrupts Active...\n", 0x0A); // Green
    shell_print("PIC Remapped...\n", 0x0A);
    
    // 4. Enable Hardware Interrupts
    __asm__ volatile("sti");
    shell_print("Keyboard Enabled. System Ready.\n\n", 0x0B); // Cyan

    // 5. Initial Prompt
    shell_print("> ", 0x0E); // Yellow prompt

    // 6. The Halt Loop
    while (1) {
        __asm__ volatile("hlt");
    }
}