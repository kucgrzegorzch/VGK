.section .text
.code64

/*
 * Tymczasowe procedury awaryjne dla wektorow 0..31.
 * Nie sa jeszcze podlaczone do IDT. Nie wracaja do kodu po wyjatku.
 * Wspolny punkt awaryjny zachowuje rejestry ogolnego przeznaczenia,
 * ale nie buduje jeszcze VGK_EXCEPTION_CONTEXT ani nie wywoluje C.
 */
.macro VGK_EXCEPTION_NO_ERROR number
.global vgk_exception_stub_\number
.type vgk_exception_stub_\number, @function
vgk_exception_stub_\number:
    pushq $0
    pushq $\number
    jmp vgk_exception_fatal_entry
.size vgk_exception_stub_\number, .-vgk_exception_stub_\number
.endm

.macro VGK_EXCEPTION_WITH_ERROR number
.global vgk_exception_stub_\number
.type vgk_exception_stub_\number, @function
vgk_exception_stub_\number:
    /* Procesor umiescil juz kod bledu na stosie. */
    pushq $\number
    jmp vgk_exception_fatal_entry
.size vgk_exception_stub_\number, .-vgk_exception_stub_\number
.endm

VGK_EXCEPTION_NO_ERROR 0
VGK_EXCEPTION_NO_ERROR 1
VGK_EXCEPTION_NO_ERROR 2
VGK_EXCEPTION_NO_ERROR 3
VGK_EXCEPTION_NO_ERROR 4
VGK_EXCEPTION_NO_ERROR 5
VGK_EXCEPTION_NO_ERROR 6
VGK_EXCEPTION_NO_ERROR 7
VGK_EXCEPTION_WITH_ERROR 8
VGK_EXCEPTION_NO_ERROR 9
VGK_EXCEPTION_WITH_ERROR 10
VGK_EXCEPTION_WITH_ERROR 11
VGK_EXCEPTION_WITH_ERROR 12
VGK_EXCEPTION_WITH_ERROR 13

/*
 * #PF has a dedicated early-capture path. The CPU has already placed its
 * error code and return frame on the mapped IST4 stack. Save the interrupted
 * RAX, read CR2 before adding the vector or saving the remaining GPRs, then
 * exchange CR2 into a private stack slot while restoring the original RAX.
 *
 * Resulting low-to-high stack order after the common GPR saves:
 * R15..R8, RDI, RSI, RBP, RDX, RCX, RBX, RAX,
 * vector, captured CR2, CPU error code, RIP, CS, RFLAGS, RSP, SS.
 *
 * The private CR2 slot is intentionally distinct from the CPU error code.
 * This is an emergency/fatal path and does not yet build the normalized
 * VGK_EXCEPTION_CONTEXT in the normal kernel; only the isolated QEMU test
 * build calls a dedicated C observer.
 */
.global vgk_exception_stub_14
.type vgk_exception_stub_14, @function
vgk_exception_stub_14:
    pushq %rax
    movq %cr2, %rax
    xchgq %rax, (%rsp)
    pushq $14
    jmp vgk_exception_fatal_entry
.size vgk_exception_stub_14, .-vgk_exception_stub_14

VGK_EXCEPTION_NO_ERROR 15
VGK_EXCEPTION_NO_ERROR 16
VGK_EXCEPTION_WITH_ERROR 17
VGK_EXCEPTION_NO_ERROR 18
VGK_EXCEPTION_NO_ERROR 19
VGK_EXCEPTION_NO_ERROR 20
VGK_EXCEPTION_WITH_ERROR 21
VGK_EXCEPTION_NO_ERROR 22
VGK_EXCEPTION_NO_ERROR 23
VGK_EXCEPTION_NO_ERROR 24
VGK_EXCEPTION_NO_ERROR 25
VGK_EXCEPTION_NO_ERROR 26
VGK_EXCEPTION_NO_ERROR 27
VGK_EXCEPTION_NO_ERROR 28
VGK_EXCEPTION_WITH_ERROR 29
VGK_EXCEPTION_WITH_ERROR 30
VGK_EXCEPTION_NO_ERROR 31

/*
 * Separate #DF capture stub for the isolated test path.
 * It is deliberately NOT installed in vgk_exception_stub_table yet.
 * The CPU has already pushed the #DF error code and return frame on IST1.
 * Unlike #PF, this frame has no CR2 slot: 15 GPRs + vector + error code
 * + RIP + CS + RFLAGS + RSP + SS = 22 qwords.
 */
.global vgk_double_fault_test_stub
.type vgk_double_fault_test_stub, @function
vgk_double_fault_test_stub:
    pushq $8
    jmp vgk_double_fault_capture_entry
.size vgk_double_fault_test_stub, .-vgk_double_fault_test_stub

/*
 * Capture-only #DF entry. Save every GPR in the same normalized order as
 * the #PF path, then stop. This is intentionally not a C observer yet:
 * the next step will add IST1 range validation and the 22-qword mapper
 * before any controlled #DF is generated.
 */
.global vgk_double_fault_capture_entry
.type vgk_double_fault_capture_entry, @function
vgk_double_fault_capture_entry:
    cli
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
#ifdef VGK_QEMU_DOUBLE_FAULT_TEST
    /*
     * #DF frame: 6 CPU qwords + vector + 15 GPRs = 22 qwords.
     * With an aligned IST1 top, RSP is 16-byte aligned here; SysV
     * requires that alignment immediately before CALL.
     */
    movq %rsp, %rdi
    call vgk_qemu_double_fault_observe
#endif
1:
    hlt
    jmp 1b
.size vgk_double_fault_capture_entry, .-vgk_double_fault_capture_entry

/*
 * Punkt awaryjny: najpierw wylaczamy maskowalne IRQ, potem zachowujemy
 * wszystkie rejestry ogolnego przeznaczenia (RSP nie jest pushowany).
 *
 * Od najnizszego adresu stosu po tych instrukcjach:
 * R15..R8, RDI, RSI, RBP, RDX, RCX, RBX, RAX,
 * wektor, kod bledu, RIP, CS, RFLAGS, RSP, SS.
 * Wyjatek: #PF ma dodatkowy slot CR2 pomiedzy wektorem a kodem bledu.
 * W trybie 64-bitowym procesor zawsze zapisuje RSP i SS w ramce,
 * rowniez bez zmiany CPL i bez uzycia IST.
 */
.global vgk_exception_fatal_entry
.type vgk_exception_fatal_entry, @function
vgk_exception_fatal_entry:
    cli
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
#ifdef VGK_QEMU_PAGE_FAULT_TEST
    /*
     * The raw 23-qword frame starts at RSP. The IST top is 16-byte
     * aligned; after the CPU/stub/GPR pushes RSP is 8 mod 16.
     * Align the caller stack before entering the SysV C observer.
     */
    movq %rsp, %rdi
    subq $8, %rsp
    call vgk_qemu_page_fault_observe
    ud2
#endif
1:
    hlt
    jmp 1b
.size vgk_exception_fatal_entry, .-vgk_exception_fatal_entry

.section .rodata
#ifdef VGK_QEMU_PAGE_FAULT_TEST
.Lvgk_qemu_pf_pass:
    .asciz "VGK: QEMU PAGE FAULT PASS\r\n"
.Lvgk_qemu_pf_fail:
    .asciz "VGK: QEMU PAGE FAULT FAIL\r\n"
#endif
.align 8
.global vgk_exception_stub_table
.type vgk_exception_stub_table, @object
vgk_exception_stub_table:
    .quad vgk_exception_stub_0
    .quad vgk_exception_stub_1
    .quad vgk_exception_stub_2
    .quad vgk_exception_stub_3
    .quad vgk_exception_stub_4
    .quad vgk_exception_stub_5
    .quad vgk_exception_stub_6
    .quad vgk_exception_stub_7
    .quad vgk_exception_stub_8
    .quad vgk_exception_stub_9
    .quad vgk_exception_stub_10
    .quad vgk_exception_stub_11
    .quad vgk_exception_stub_12
    .quad vgk_exception_stub_13
    .quad vgk_exception_stub_14
    .quad vgk_exception_stub_15
    .quad vgk_exception_stub_16
    .quad vgk_exception_stub_17
    .quad vgk_exception_stub_18
    .quad vgk_exception_stub_19
    .quad vgk_exception_stub_20
    .quad vgk_exception_stub_21
    .quad vgk_exception_stub_22
    .quad vgk_exception_stub_23
    .quad vgk_exception_stub_24
    .quad vgk_exception_stub_25
    .quad vgk_exception_stub_26
    .quad vgk_exception_stub_27
    .quad vgk_exception_stub_28
    .quad vgk_exception_stub_29
    .quad vgk_exception_stub_30
    .quad vgk_exception_stub_31
.size vgk_exception_stub_table, .-vgk_exception_stub_table

.section .note.GNU-stack,"",@progbits
