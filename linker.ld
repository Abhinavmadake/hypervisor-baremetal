ENTRY(_start)
SECTIONS
{
    /* In our VM, we mapped memory starting at 0x80000000 */
    . = 0x80000000;
    
    .text : {
        *(.text)
    }
    .data : {
        *(.data)
    }
    .bss : {
        *(.bss COMMON)
    }
}
