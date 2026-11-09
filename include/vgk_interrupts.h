#ifndef VGK_INTERRUPTS_H
#define VGK_INTERRUPTS_H

#include <stdint.h>

#define VGK_IDT_EXCEPTION_COUNT 32U

/* 64-bitowy wpis tablicy przerwan x86-64. */
typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} VGK_IDT_GATE;

typedef enum {
    VGK_INTERRUPT_OK = 0,
    VGK_INTERRUPT_INVALID_ARGUMENT = 1
} VGK_INTERRUPT_STATUS;

#define VGK_IDT_GATE_INTERRUPT 0x0EU
#define VGK_IDT_GATE_TRAP      0x0FU

/*
 * Buduje pojedynczy wpis IDT bez wykonywania instrukcji uprzywilejowanych.
 * handler: adres procedury obslugi; selector: selektor segmentu kodu;
 * ist: numer stosu awaryjnego 0..7; dpl: poziom uprawnien 0..3.
 * present=0 tworzy nieobecny wpis, przydatny do bezpiecznej inicjalizacji.
 */
VGK_INTERRUPT_STATUS vgk_idt_gate_build(
    VGK_IDT_GATE *gate,
    uint64_t handler,
    uint16_t selector,
    uint8_t ist,
    uint8_t dpl,
    uint8_t gate_type,
    uint8_t present);

/*
 * Przygotowuje 32 wpisy dla wektorow wyjatkow procesora (0..31).
 * Kazdy adres musi byc niezerowy; wywolanie nie zmienia tablicy,
 * jesli argumenty sa niepoprawne. Nie instaluje tablicy instrukcja LIDT.
 */
VGK_INTERRUPT_STATUS vgk_idt_build_exception_table(
    VGK_IDT_GATE table[VGK_IDT_EXCEPTION_COUNT],
    const uint64_t handlers[VGK_IDT_EXCEPTION_COUNT],
    uint16_t code_selector);

#endif
