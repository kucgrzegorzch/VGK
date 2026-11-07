.section .text
.code64
.global vgk_kernel_entry
.type vgk_kernel_entry, @function
.extern vgk_kernel_main

/*
 * ABI wejscia VGK:
 * RDI wskazuje na VGK_BOOT_INFO, przekazany przez wlasny ladowacz.
 * Ten punkt wejscia nie jest zgodny z Multiboot2 ani GRUB.
 */
vgk_kernel_entry:
    cli
    cld
    lea vgk_boot_stack_top(%rip), %rsp
    andq $-16, %rsp
    call vgk_kernel_main

1:
    cli
    hlt
    jmp 1b

.size vgk_kernel_entry, .-vgk_kernel_entry

.section .bss
.align 16
vgk_boot_stack:
    .skip 16384
vgk_boot_stack_top:

.section .note.GNU-stack,"",@progbits
