.text
.p2align 4
.globl vgk_uefi_enter_kernel
.def vgk_uefi_enter_kernel; .scl 2; .type 32; .endef

/*
 * Microsoft x64 ABI on entry:
 *   RCX = native kernel entry address
 *   RDX = pointer to VGK_BOOT_INFO
 *
 * Native VGK entry contract:
 *   RDI = pointer to VGK_BOOT_INFO
 *
 * The kernel entry assembly installs its private stack before calling C.
 * No UEFI service may be used after this transfer.
 */
vgk_uefi_enter_kernel:
    cli
    cld
    movq %rcx, %rax
    movq %rdx, %rdi
    xorq %rbp, %rbp
    jmpq *%rax

.section .note.GNU-stack,"",@progbits
