OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(vgk_kernel_entry)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x200000;

    .text : ALIGN(0x1000)
    {
        KEEP(*(.text))
        *(.text*)
    } :text

    .rodata : ALIGN(0x1000)
    {
        *(.rodata*)
    } :text

    .data : ALIGN(0x1000)
    {
        *(.data*)
    } :data

    .bss (NOLOAD) : ALIGN(0x1000)
    {
        *(COMMON)
        *(.bss*)
    } :data

    /DISCARD/ :
    {
        *(.eh_frame*)
        *(.comment)
        *(.note*)
    }
}
