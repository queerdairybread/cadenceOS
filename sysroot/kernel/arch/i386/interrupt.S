.section .text
.align 4
.global common_interrupt_handler
.extern interrupt_handler

common_interrupt_handler:
    # 1. Save all registers
    pushal
    push %ds
    push %es
    push %fs
    push %gs

    # 2. Load Kernel Data Segment (0x10)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es

    # 3. Call the C code
    call interrupt_handler

    # 4. Restore all registers
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popal

    # 5. Return from interrupt
    iret
