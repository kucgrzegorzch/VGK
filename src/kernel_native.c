#include <stdint.h>
#include "vgk_boot_info.h"
#include "vgk_memory.h"

#define COM1_PORT 0x3F8U

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_init(void)
{
    outb(COM1_PORT + 1U, 0x00U);
    outb(COM1_PORT + 3U, 0x80U);
    outb(COM1_PORT + 0U, 0x01U);
    outb(COM1_PORT + 1U, 0x00U);
    outb(COM1_PORT + 3U, 0x03U);
    outb(COM1_PORT + 2U, 0xC7U);
    outb(COM1_PORT + 4U, 0x0BU);
}

static void serial_write_char(char value)
{
    unsigned int timeout = 100000U;
    while ((inb(COM1_PORT + 5U) & 0x20U) == 0U && timeout != 0U)
        --timeout;
    if (timeout != 0U)
        outb(COM1_PORT, (uint8_t)value);
}

static void serial_write(const char *message)
{
    while (*message != '\0')
        serial_write_char(*message++);
}

__attribute__((noreturn))
void vgk_kernel_main(const VGK_BOOT_INFO *boot_info)
{
    serial_init();
    serial_write("\r\nVGK: uruchomiono wlasne jadro.\r\n");

    if (!vgk_boot_info_is_valid(boot_info)) {
        serial_write("VGK: BLAD protokolu startowego. Zatrzymanie.\r\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    serial_write("VGK: protokol startowy poprawny.\r\n");

    {
        VGK_MEMORY_STATS memory_stats;
        uint64_t test_page = 0;

        if (vgk_memory_init(boot_info) != VGK_MEMORY_OK ||
            vgk_memory_get_stats(&memory_stats) != VGK_MEMORY_OK ||
            memory_stats.total_pages == 0 ||
            vgk_memory_alloc_pages(1U, 1U, &test_page) != VGK_MEMORY_OK ||
            vgk_memory_free_pages(test_page, 1U) != VGK_MEMORY_OK) {
            serial_write("VGK: BLAD inicjalizacji menedzera pamieci.\r\n");
            for (;;) {
                __asm__ volatile ("cli; hlt");
            }
        }
    }

    serial_write("VGK: menedzer pamieci uruchomiony.\r\n");
    serial_write("VGK: etap diagnostyczny; sterowniki i uslugi nie sa jeszcze gotowe.\r\n");

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
