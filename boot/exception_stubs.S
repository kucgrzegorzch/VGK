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
VGK_EXCEPTION_WITH_ERROR 14
VGK_EXCEPTION_NO_ERROR 15
VGK_EXCEPTION_NO_ERROR 16
VGK_EXCEPTION_WITH_ERROR 17
VGK_EXCEPTION_NO_ERROR 18
VGK_EXCEPTION_NO_ERROR 19
VGK_EXCEPTION_WITH_ERROR 20
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
 * Punkt awaryjny: najpierw wylaczamy maskowalne IRQ, potem zachowujemy
 * wszystkie rejestry ogolnego przeznaczenia (RSP nie jest pushowany).
 *
 * Od najnizszego adresu stosu po tych instrukcjach:
 * R15..R8, RDI, RSI, RBP, RDX, RCX, RBX, RAX,
 * wektor, kod bledu, RIP, CS, RFLAGS.
 * RSP/SS z ramki CPU wystepuja tylko przy zmianie poziomu/IST.
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
1:
    hlt
    jmp 1b
.size vgk_exception_fatal_entry, .-vgk_exception_fatal_entry

.section .rodata
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
