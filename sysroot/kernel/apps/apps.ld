SECTIONS
{
    /* The application is loaded at 0x20000 */
    . = 0x20000;

    .text : {
        /* Force the prologue section to be the first bytes of the file */
        *(.text.prologue)
        *(.text)
    }

    .data : { *(.data) }
    .bss  : { *(.bss)  }
}