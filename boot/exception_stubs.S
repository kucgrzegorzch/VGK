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
 * Vectors 32..255 have no CPU-pushed error code. Until dedicated IRQ
 * dispatchers and PIC/APIC acknowledgement exist, each vector enters the
 * fatal stop path. This is a fail-stop fallback, not an IRQ service routine.
 */
.macro VGK_INTERRUPT_NO_ERROR number
.global vgk_interrupt_stub_\number
.type vgk_interrupt_stub_\number, @function
vgk_interrupt_stub_\number:
    pushq $0
    pushq $\number
    jmp vgk_exception_fatal_entry
.size vgk_interrupt_stub_\number, .-vgk_interrupt_stub_\number
.endm

VGK_INTERRUPT_NO_ERROR 32
VGK_INTERRUPT_NO_ERROR 33
VGK_INTERRUPT_NO_ERROR 34
VGK_INTERRUPT_NO_ERROR 35
VGK_INTERRUPT_NO_ERROR 36
VGK_INTERRUPT_NO_ERROR 37
VGK_INTERRUPT_NO_ERROR 38
VGK_INTERRUPT_NO_ERROR 39
VGK_INTERRUPT_NO_ERROR 40
VGK_INTERRUPT_NO_ERROR 41
VGK_INTERRUPT_NO_ERROR 42
VGK_INTERRUPT_NO_ERROR 43
VGK_INTERRUPT_NO_ERROR 44
VGK_INTERRUPT_NO_ERROR 45
VGK_INTERRUPT_NO_ERROR 46
VGK_INTERRUPT_NO_ERROR 47
VGK_INTERRUPT_NO_ERROR 48
VGK_INTERRUPT_NO_ERROR 49
VGK_INTERRUPT_NO_ERROR 50
VGK_INTERRUPT_NO_ERROR 51
VGK_INTERRUPT_NO_ERROR 52
VGK_INTERRUPT_NO_ERROR 53
VGK_INTERRUPT_NO_ERROR 54
VGK_INTERRUPT_NO_ERROR 55
VGK_INTERRUPT_NO_ERROR 56
VGK_INTERRUPT_NO_ERROR 57
VGK_INTERRUPT_NO_ERROR 58
VGK_INTERRUPT_NO_ERROR 59
VGK_INTERRUPT_NO_ERROR 60
VGK_INTERRUPT_NO_ERROR 61
VGK_INTERRUPT_NO_ERROR 62
VGK_INTERRUPT_NO_ERROR 63
VGK_INTERRUPT_NO_ERROR 64
VGK_INTERRUPT_NO_ERROR 65
VGK_INTERRUPT_NO_ERROR 66
VGK_INTERRUPT_NO_ERROR 67
VGK_INTERRUPT_NO_ERROR 68
VGK_INTERRUPT_NO_ERROR 69
VGK_INTERRUPT_NO_ERROR 70
VGK_INTERRUPT_NO_ERROR 71
VGK_INTERRUPT_NO_ERROR 72
VGK_INTERRUPT_NO_ERROR 73
VGK_INTERRUPT_NO_ERROR 74
VGK_INTERRUPT_NO_ERROR 75
VGK_INTERRUPT_NO_ERROR 76
VGK_INTERRUPT_NO_ERROR 77
VGK_INTERRUPT_NO_ERROR 78
VGK_INTERRUPT_NO_ERROR 79
VGK_INTERRUPT_NO_ERROR 80
VGK_INTERRUPT_NO_ERROR 81
VGK_INTERRUPT_NO_ERROR 82
VGK_INTERRUPT_NO_ERROR 83
VGK_INTERRUPT_NO_ERROR 84
VGK_INTERRUPT_NO_ERROR 85
VGK_INTERRUPT_NO_ERROR 86
VGK_INTERRUPT_NO_ERROR 87
VGK_INTERRUPT_NO_ERROR 88
VGK_INTERRUPT_NO_ERROR 89
VGK_INTERRUPT_NO_ERROR 90
VGK_INTERRUPT_NO_ERROR 91
VGK_INTERRUPT_NO_ERROR 92
VGK_INTERRUPT_NO_ERROR 93
VGK_INTERRUPT_NO_ERROR 94
VGK_INTERRUPT_NO_ERROR 95
VGK_INTERRUPT_NO_ERROR 96
VGK_INTERRUPT_NO_ERROR 97
VGK_INTERRUPT_NO_ERROR 98
VGK_INTERRUPT_NO_ERROR 99
VGK_INTERRUPT_NO_ERROR 100
VGK_INTERRUPT_NO_ERROR 101
VGK_INTERRUPT_NO_ERROR 102
VGK_INTERRUPT_NO_ERROR 103
VGK_INTERRUPT_NO_ERROR 104
VGK_INTERRUPT_NO_ERROR 105
VGK_INTERRUPT_NO_ERROR 106
VGK_INTERRUPT_NO_ERROR 107
VGK_INTERRUPT_NO_ERROR 108
VGK_INTERRUPT_NO_ERROR 109
VGK_INTERRUPT_NO_ERROR 110
VGK_INTERRUPT_NO_ERROR 111
VGK_INTERRUPT_NO_ERROR 112
VGK_INTERRUPT_NO_ERROR 113
VGK_INTERRUPT_NO_ERROR 114
VGK_INTERRUPT_NO_ERROR 115
VGK_INTERRUPT_NO_ERROR 116
VGK_INTERRUPT_NO_ERROR 117
VGK_INTERRUPT_NO_ERROR 118
VGK_INTERRUPT_NO_ERROR 119
VGK_INTERRUPT_NO_ERROR 120
VGK_INTERRUPT_NO_ERROR 121
VGK_INTERRUPT_NO_ERROR 122
VGK_INTERRUPT_NO_ERROR 123
VGK_INTERRUPT_NO_ERROR 124
VGK_INTERRUPT_NO_ERROR 125
VGK_INTERRUPT_NO_ERROR 126
VGK_INTERRUPT_NO_ERROR 127
VGK_INTERRUPT_NO_ERROR 128
VGK_INTERRUPT_NO_ERROR 129
VGK_INTERRUPT_NO_ERROR 130
VGK_INTERRUPT_NO_ERROR 131
VGK_INTERRUPT_NO_ERROR 132
VGK_INTERRUPT_NO_ERROR 133
VGK_INTERRUPT_NO_ERROR 134
VGK_INTERRUPT_NO_ERROR 135
VGK_INTERRUPT_NO_ERROR 136
VGK_INTERRUPT_NO_ERROR 137
VGK_INTERRUPT_NO_ERROR 138
VGK_INTERRUPT_NO_ERROR 139
VGK_INTERRUPT_NO_ERROR 140
VGK_INTERRUPT_NO_ERROR 141
VGK_INTERRUPT_NO_ERROR 142
VGK_INTERRUPT_NO_ERROR 143
VGK_INTERRUPT_NO_ERROR 144
VGK_INTERRUPT_NO_ERROR 145
VGK_INTERRUPT_NO_ERROR 146
VGK_INTERRUPT_NO_ERROR 147
VGK_INTERRUPT_NO_ERROR 148
VGK_INTERRUPT_NO_ERROR 149
VGK_INTERRUPT_NO_ERROR 150
VGK_INTERRUPT_NO_ERROR 151
VGK_INTERRUPT_NO_ERROR 152
VGK_INTERRUPT_NO_ERROR 153
VGK_INTERRUPT_NO_ERROR 154
VGK_INTERRUPT_NO_ERROR 155
VGK_INTERRUPT_NO_ERROR 156
VGK_INTERRUPT_NO_ERROR 157
VGK_INTERRUPT_NO_ERROR 158
VGK_INTERRUPT_NO_ERROR 159
VGK_INTERRUPT_NO_ERROR 160
VGK_INTERRUPT_NO_ERROR 161
VGK_INTERRUPT_NO_ERROR 162
VGK_INTERRUPT_NO_ERROR 163
VGK_INTERRUPT_NO_ERROR 164
VGK_INTERRUPT_NO_ERROR 165
VGK_INTERRUPT_NO_ERROR 166
VGK_INTERRUPT_NO_ERROR 167
VGK_INTERRUPT_NO_ERROR 168
VGK_INTERRUPT_NO_ERROR 169
VGK_INTERRUPT_NO_ERROR 170
VGK_INTERRUPT_NO_ERROR 171
VGK_INTERRUPT_NO_ERROR 172
VGK_INTERRUPT_NO_ERROR 173
VGK_INTERRUPT_NO_ERROR 174
VGK_INTERRUPT_NO_ERROR 175
VGK_INTERRUPT_NO_ERROR 176
VGK_INTERRUPT_NO_ERROR 177
VGK_INTERRUPT_NO_ERROR 178
VGK_INTERRUPT_NO_ERROR 179
VGK_INTERRUPT_NO_ERROR 180
VGK_INTERRUPT_NO_ERROR 181
VGK_INTERRUPT_NO_ERROR 182
VGK_INTERRUPT_NO_ERROR 183
VGK_INTERRUPT_NO_ERROR 184
VGK_INTERRUPT_NO_ERROR 185
VGK_INTERRUPT_NO_ERROR 186
VGK_INTERRUPT_NO_ERROR 187
VGK_INTERRUPT_NO_ERROR 188
VGK_INTERRUPT_NO_ERROR 189
VGK_INTERRUPT_NO_ERROR 190
VGK_INTERRUPT_NO_ERROR 191
VGK_INTERRUPT_NO_ERROR 192
VGK_INTERRUPT_NO_ERROR 193
VGK_INTERRUPT_NO_ERROR 194
VGK_INTERRUPT_NO_ERROR 195
VGK_INTERRUPT_NO_ERROR 196
VGK_INTERRUPT_NO_ERROR 197
VGK_INTERRUPT_NO_ERROR 198
VGK_INTERRUPT_NO_ERROR 199
VGK_INTERRUPT_NO_ERROR 200
VGK_INTERRUPT_NO_ERROR 201
VGK_INTERRUPT_NO_ERROR 202
VGK_INTERRUPT_NO_ERROR 203
VGK_INTERRUPT_NO_ERROR 204
VGK_INTERRUPT_NO_ERROR 205
VGK_INTERRUPT_NO_ERROR 206
VGK_INTERRUPT_NO_ERROR 207
VGK_INTERRUPT_NO_ERROR 208
VGK_INTERRUPT_NO_ERROR 209
VGK_INTERRUPT_NO_ERROR 210
VGK_INTERRUPT_NO_ERROR 211
VGK_INTERRUPT_NO_ERROR 212
VGK_INTERRUPT_NO_ERROR 213
VGK_INTERRUPT_NO_ERROR 214
VGK_INTERRUPT_NO_ERROR 215
VGK_INTERRUPT_NO_ERROR 216
VGK_INTERRUPT_NO_ERROR 217
VGK_INTERRUPT_NO_ERROR 218
VGK_INTERRUPT_NO_ERROR 219
VGK_INTERRUPT_NO_ERROR 220
VGK_INTERRUPT_NO_ERROR 221
VGK_INTERRUPT_NO_ERROR 222
VGK_INTERRUPT_NO_ERROR 223
VGK_INTERRUPT_NO_ERROR 224
VGK_INTERRUPT_NO_ERROR 225
VGK_INTERRUPT_NO_ERROR 226
VGK_INTERRUPT_NO_ERROR 227
VGK_INTERRUPT_NO_ERROR 228
VGK_INTERRUPT_NO_ERROR 229
VGK_INTERRUPT_NO_ERROR 230
VGK_INTERRUPT_NO_ERROR 231
VGK_INTERRUPT_NO_ERROR 232
VGK_INTERRUPT_NO_ERROR 233
VGK_INTERRUPT_NO_ERROR 234
VGK_INTERRUPT_NO_ERROR 235
VGK_INTERRUPT_NO_ERROR 236
VGK_INTERRUPT_NO_ERROR 237
VGK_INTERRUPT_NO_ERROR 238
VGK_INTERRUPT_NO_ERROR 239
VGK_INTERRUPT_NO_ERROR 240
VGK_INTERRUPT_NO_ERROR 241
VGK_INTERRUPT_NO_ERROR 242
VGK_INTERRUPT_NO_ERROR 243
VGK_INTERRUPT_NO_ERROR 244
VGK_INTERRUPT_NO_ERROR 245
VGK_INTERRUPT_NO_ERROR 246
VGK_INTERRUPT_NO_ERROR 247
VGK_INTERRUPT_NO_ERROR 248
VGK_INTERRUPT_NO_ERROR 249
VGK_INTERRUPT_NO_ERROR 250
VGK_INTERRUPT_NO_ERROR 251
VGK_INTERRUPT_NO_ERROR 252
VGK_INTERRUPT_NO_ERROR 253
VGK_INTERRUPT_NO_ERROR 254
VGK_INTERRUPT_NO_ERROR 255

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
#ifdef VGK_QEMU_PAGE_FAULT_TEST
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

.align 8
.global vgk_vector_stub_table
.type vgk_vector_stub_table, @object
vgk_vector_stub_table:
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
    .quad vgk_interrupt_stub_32
    .quad vgk_interrupt_stub_33
    .quad vgk_interrupt_stub_34
    .quad vgk_interrupt_stub_35
    .quad vgk_interrupt_stub_36
    .quad vgk_interrupt_stub_37
    .quad vgk_interrupt_stub_38
    .quad vgk_interrupt_stub_39
    .quad vgk_interrupt_stub_40
    .quad vgk_interrupt_stub_41
    .quad vgk_interrupt_stub_42
    .quad vgk_interrupt_stub_43
    .quad vgk_interrupt_stub_44
    .quad vgk_interrupt_stub_45
    .quad vgk_interrupt_stub_46
    .quad vgk_interrupt_stub_47
    .quad vgk_interrupt_stub_48
    .quad vgk_interrupt_stub_49
    .quad vgk_interrupt_stub_50
    .quad vgk_interrupt_stub_51
    .quad vgk_interrupt_stub_52
    .quad vgk_interrupt_stub_53
    .quad vgk_interrupt_stub_54
    .quad vgk_interrupt_stub_55
    .quad vgk_interrupt_stub_56
    .quad vgk_interrupt_stub_57
    .quad vgk_interrupt_stub_58
    .quad vgk_interrupt_stub_59
    .quad vgk_interrupt_stub_60
    .quad vgk_interrupt_stub_61
    .quad vgk_interrupt_stub_62
    .quad vgk_interrupt_stub_63
    .quad vgk_interrupt_stub_64
    .quad vgk_interrupt_stub_65
    .quad vgk_interrupt_stub_66
    .quad vgk_interrupt_stub_67
    .quad vgk_interrupt_stub_68
    .quad vgk_interrupt_stub_69
    .quad vgk_interrupt_stub_70
    .quad vgk_interrupt_stub_71
    .quad vgk_interrupt_stub_72
    .quad vgk_interrupt_stub_73
    .quad vgk_interrupt_stub_74
    .quad vgk_interrupt_stub_75
    .quad vgk_interrupt_stub_76
    .quad vgk_interrupt_stub_77
    .quad vgk_interrupt_stub_78
    .quad vgk_interrupt_stub_79
    .quad vgk_interrupt_stub_80
    .quad vgk_interrupt_stub_81
    .quad vgk_interrupt_stub_82
    .quad vgk_interrupt_stub_83
    .quad vgk_interrupt_stub_84
    .quad vgk_interrupt_stub_85
    .quad vgk_interrupt_stub_86
    .quad vgk_interrupt_stub_87
    .quad vgk_interrupt_stub_88
    .quad vgk_interrupt_stub_89
    .quad vgk_interrupt_stub_90
    .quad vgk_interrupt_stub_91
    .quad vgk_interrupt_stub_92
    .quad vgk_interrupt_stub_93
    .quad vgk_interrupt_stub_94
    .quad vgk_interrupt_stub_95
    .quad vgk_interrupt_stub_96
    .quad vgk_interrupt_stub_97
    .quad vgk_interrupt_stub_98
    .quad vgk_interrupt_stub_99
    .quad vgk_interrupt_stub_100
    .quad vgk_interrupt_stub_101
    .quad vgk_interrupt_stub_102
    .quad vgk_interrupt_stub_103
    .quad vgk_interrupt_stub_104
    .quad vgk_interrupt_stub_105
    .quad vgk_interrupt_stub_106
    .quad vgk_interrupt_stub_107
    .quad vgk_interrupt_stub_108
    .quad vgk_interrupt_stub_109
    .quad vgk_interrupt_stub_110
    .quad vgk_interrupt_stub_111
    .quad vgk_interrupt_stub_112
    .quad vgk_interrupt_stub_113
    .quad vgk_interrupt_stub_114
    .quad vgk_interrupt_stub_115
    .quad vgk_interrupt_stub_116
    .quad vgk_interrupt_stub_117
    .quad vgk_interrupt_stub_118
    .quad vgk_interrupt_stub_119
    .quad vgk_interrupt_stub_120
    .quad vgk_interrupt_stub_121
    .quad vgk_interrupt_stub_122
    .quad vgk_interrupt_stub_123
    .quad vgk_interrupt_stub_124
    .quad vgk_interrupt_stub_125
    .quad vgk_interrupt_stub_126
    .quad vgk_interrupt_stub_127
    .quad vgk_interrupt_stub_128
    .quad vgk_interrupt_stub_129
    .quad vgk_interrupt_stub_130
    .quad vgk_interrupt_stub_131
    .quad vgk_interrupt_stub_132
    .quad vgk_interrupt_stub_133
    .quad vgk_interrupt_stub_134
    .quad vgk_interrupt_stub_135
    .quad vgk_interrupt_stub_136
    .quad vgk_interrupt_stub_137
    .quad vgk_interrupt_stub_138
    .quad vgk_interrupt_stub_139
    .quad vgk_interrupt_stub_140
    .quad vgk_interrupt_stub_141
    .quad vgk_interrupt_stub_142
    .quad vgk_interrupt_stub_143
    .quad vgk_interrupt_stub_144
    .quad vgk_interrupt_stub_145
    .quad vgk_interrupt_stub_146
    .quad vgk_interrupt_stub_147
    .quad vgk_interrupt_stub_148
    .quad vgk_interrupt_stub_149
    .quad vgk_interrupt_stub_150
    .quad vgk_interrupt_stub_151
    .quad vgk_interrupt_stub_152
    .quad vgk_interrupt_stub_153
    .quad vgk_interrupt_stub_154
    .quad vgk_interrupt_stub_155
    .quad vgk_interrupt_stub_156
    .quad vgk_interrupt_stub_157
    .quad vgk_interrupt_stub_158
    .quad vgk_interrupt_stub_159
    .quad vgk_interrupt_stub_160
    .quad vgk_interrupt_stub_161
    .quad vgk_interrupt_stub_162
    .quad vgk_interrupt_stub_163
    .quad vgk_interrupt_stub_164
    .quad vgk_interrupt_stub_165
    .quad vgk_interrupt_stub_166
    .quad vgk_interrupt_stub_167
    .quad vgk_interrupt_stub_168
    .quad vgk_interrupt_stub_169
    .quad vgk_interrupt_stub_170
    .quad vgk_interrupt_stub_171
    .quad vgk_interrupt_stub_172
    .quad vgk_interrupt_stub_173
    .quad vgk_interrupt_stub_174
    .quad vgk_interrupt_stub_175
    .quad vgk_interrupt_stub_176
    .quad vgk_interrupt_stub_177
    .quad vgk_interrupt_stub_178
    .quad vgk_interrupt_stub_179
    .quad vgk_interrupt_stub_180
    .quad vgk_interrupt_stub_181
    .quad vgk_interrupt_stub_182
    .quad vgk_interrupt_stub_183
    .quad vgk_interrupt_stub_184
    .quad vgk_interrupt_stub_185
    .quad vgk_interrupt_stub_186
    .quad vgk_interrupt_stub_187
    .quad vgk_interrupt_stub_188
    .quad vgk_interrupt_stub_189
    .quad vgk_interrupt_stub_190
    .quad vgk_interrupt_stub_191
    .quad vgk_interrupt_stub_192
    .quad vgk_interrupt_stub_193
    .quad vgk_interrupt_stub_194
    .quad vgk_interrupt_stub_195
    .quad vgk_interrupt_stub_196
    .quad vgk_interrupt_stub_197
    .quad vgk_interrupt_stub_198
    .quad vgk_interrupt_stub_199
    .quad vgk_interrupt_stub_200
    .quad vgk_interrupt_stub_201
    .quad vgk_interrupt_stub_202
    .quad vgk_interrupt_stub_203
    .quad vgk_interrupt_stub_204
    .quad vgk_interrupt_stub_205
    .quad vgk_interrupt_stub_206
    .quad vgk_interrupt_stub_207
    .quad vgk_interrupt_stub_208
    .quad vgk_interrupt_stub_209
    .quad vgk_interrupt_stub_210
    .quad vgk_interrupt_stub_211
    .quad vgk_interrupt_stub_212
    .quad vgk_interrupt_stub_213
    .quad vgk_interrupt_stub_214
    .quad vgk_interrupt_stub_215
    .quad vgk_interrupt_stub_216
    .quad vgk_interrupt_stub_217
    .quad vgk_interrupt_stub_218
    .quad vgk_interrupt_stub_219
    .quad vgk_interrupt_stub_220
    .quad vgk_interrupt_stub_221
    .quad vgk_interrupt_stub_222
    .quad vgk_interrupt_stub_223
    .quad vgk_interrupt_stub_224
    .quad vgk_interrupt_stub_225
    .quad vgk_interrupt_stub_226
    .quad vgk_interrupt_stub_227
    .quad vgk_interrupt_stub_228
    .quad vgk_interrupt_stub_229
    .quad vgk_interrupt_stub_230
    .quad vgk_interrupt_stub_231
    .quad vgk_interrupt_stub_232
    .quad vgk_interrupt_stub_233
    .quad vgk_interrupt_stub_234
    .quad vgk_interrupt_stub_235
    .quad vgk_interrupt_stub_236
    .quad vgk_interrupt_stub_237
    .quad vgk_interrupt_stub_238
    .quad vgk_interrupt_stub_239
    .quad vgk_interrupt_stub_240
    .quad vgk_interrupt_stub_241
    .quad vgk_interrupt_stub_242
    .quad vgk_interrupt_stub_243
    .quad vgk_interrupt_stub_244
    .quad vgk_interrupt_stub_245
    .quad vgk_interrupt_stub_246
    .quad vgk_interrupt_stub_247
    .quad vgk_interrupt_stub_248
    .quad vgk_interrupt_stub_249
    .quad vgk_interrupt_stub_250
    .quad vgk_interrupt_stub_251
    .quad vgk_interrupt_stub_252
    .quad vgk_interrupt_stub_253
    .quad vgk_interrupt_stub_254
    .quad vgk_interrupt_stub_255
.size vgk_vector_stub_table, .-vgk_vector_stub_table

.section .note.GNU-stack,"",@progbits
