#include <stdint.h>
#include "vgk_boot_info.h"
#include "vgk_acpi.h"
#include "vgk_memory.h"
#include "vgk_cpu_features.h"
#include "vgk_paging.h"
#include "vgk_paging_requirements.h"

#define COM1_PORT 0x3F8U

extern char vgk_boot_stack[];
extern char vgk_boot_stack_top[];

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

static void serial_write_hex(uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char buffer[16];
    size_t count = 0;

    do {
        buffer[count++] = digits[value & 0xFU];
        value >>= 4;
    } while (value != 0U && count < sizeof(buffer));

    serial_write("0x");
    while (count != 0U)
        serial_write_char(buffer[--count]);
}

static int page_table_pool_is_zeroed(const VGK_BOOT_INFO *boot_info)
{
    const volatile uint64_t *words;
    uint64_t word_count;

    if (boot_info == 0 ||
        boot_info->page_table_pool_base == 0 ||
        boot_info->page_table_pool_pages != VGK_BOOT_PAGE_TABLE_POOL_PAGES)
        return 0;

    words = (const volatile uint64_t *)(uintptr_t)
        boot_info->page_table_pool_base;
    word_count = boot_info->page_table_pool_pages *
                 (VGK_BOOT_PAGE_SIZE / sizeof(uint64_t));
    for (uint64_t i = 0; i < word_count; ++i) {
        if (words[i] != 0)
            return 0;
    }
    return 1;
}

static VGK_PAGING_RESULT map_identity_object(
    VGK_PAGE_TABLES *tables,
    uint64_t address,
    uint64_t length,
    uint32_t permissions)
{
    const uint64_t page_mask = VGK_PAGING_PAGE_SIZE - 1U;
    uint64_t page_base;
    uint64_t offset;

    if (address == 0U || length == 0U)
        return VGK_PAGING_INVALID_ARGUMENT;

    page_base = address & ~page_mask;
    offset = address - page_base;
    if (length > UINT64_MAX - offset)
        return VGK_PAGING_BAD_ADDRESS;

    return vgk_paging_map_range(
        tables, page_base, page_base, length + offset, permissions);
}

/*
 * PCD+PWT selects PAT entry 3 on PAT-capable CPUs. Verify that this
 * entry is UC before preparing a framebuffer mapping with those bits.
 * On CPUs without PAT, the legacy PCD=1/PWT=1 encoding is UC.
 */
static int framebuffer_pcd_pwt_is_uncacheable(void)
{
    uint32_t max_basic;
    uint32_t feature_edx;
    uint32_t pat_low;
    uint32_t pat_high;
    uint64_t pat_value;

    __asm__ volatile ("cpuid"
        : "=a"(max_basic)
        : "a"(0U), "c"(0U)
        : "rbx", "rdx");
    if (max_basic < 1U)
        return 1;

    __asm__ volatile ("cpuid"
        : "=a"(max_basic), "=d"(feature_edx)
        : "a"(1U), "c"(0U)
        : "rbx");
    if ((feature_edx & (UINT32_C(1) << 16)) == 0U)
        return 1;

    __asm__ volatile ("rdmsr"
        : "=a"(pat_low), "=d"(pat_high)
        : "c"(UINT32_C(0x277)));
    pat_value = ((uint64_t)pat_high << 32) | pat_low;

    /* PAT entry 3 occupies bits 24..31; memory type 0 is UC. */
    return ((pat_value >> 24) & UINT64_C(0xFF)) == 0U;
}


/* Add an object using the same page-flooring rule as map_identity_object(). */
static int add_paging_requirement_range(
    VGK_PAGING_VIRTUAL_RANGE *ranges,
    size_t *range_count,
    uint64_t address,
    uint64_t length)
{
    uint64_t page_base;
    uint64_t offset;

    if (ranges == 0 || range_count == 0 || address == 0U || length == 0U ||
        *range_count >= VGK_PAGING_REQUIREMENTS_MAX_RANGES)
        return 0;

    page_base = address & ~(VGK_PAGING_PAGE_SIZE - 1U);
    offset = address - page_base;
    if (length > UINT64_MAX - offset)
        return 0;

    ranges[*range_count].virtual_address = page_base;
    ranges[*range_count].length = length + offset;
    ++(*range_count);
    return 1;
}

/*
 * Count every identity-mapped object before allocating a single page-table
 * page. The stack is already contained in a writable, NX ELF segment.
 */
static int page_table_pool_has_capacity(
    const VGK_BOOT_INFO *boot_info,
    const VGK_CPU_FEATURES *cpu_features,
    int framebuffer_is_uncacheable)
{
    VGK_PAGING_VIRTUAL_RANGE ranges[VGK_PAGING_REQUIREMENTS_MAX_RANGES];
    VGK_PAGING_REQUIREMENTS requirements;
    const VGK_MEMORY_MAP_ENTRY *memory_map;
    size_t range_count = 0U;
    uint64_t rsdp_length = 0U;
    VGK_PAGING_REQUIREMENTS_RESULT calc_result;

    if (boot_info == 0 || cpu_features == 0)
        return 0;

    for (uint32_t i = 0U; i < boot_info->kernel_segment_count; ++i) {
        const VGK_KERNEL_SEGMENT *segment = &boot_info->kernel_segments[i];
        if (!add_paging_requirement_range(
                ranges, &range_count,
                segment->physical_base, segment->memory_size))
            return 0;
    }

    if (!add_paging_requirement_range(
            ranges, &range_count,
            (uint64_t)(uintptr_t)boot_info, sizeof(*boot_info)) ||
        !add_paging_requirement_range(
            ranges, &range_count,
            boot_info->memory_map_address, boot_info->memory_map_size))
        return 0;

    if (boot_info->page_table_pool_pages >
        UINT64_MAX / VGK_BOOT_PAGE_SIZE)
        return 0;
    if (!add_paging_requirement_range(
            ranges, &range_count,
            boot_info->page_table_pool_base,
            boot_info->page_table_pool_pages * VGK_BOOT_PAGE_SIZE))
        return 0;

    if ((boot_info->flags & VGK_BOOT_FLAG_ACPI) != 0U) {
        size_t memory_map_count;
        const volatile uint8_t *rsdp;

        if (boot_info->memory_map_size % sizeof(VGK_MEMORY_MAP_ENTRY) != 0U)
            return 0;
        memory_map = (const VGK_MEMORY_MAP_ENTRY *)(uintptr_t)
            boot_info->memory_map_address;
        memory_map_count = (size_t)(
            boot_info->memory_map_size / sizeof(VGK_MEMORY_MAP_ENTRY));
        rsdp = (const volatile uint8_t *)(uintptr_t)
            boot_info->acpi_rsdp_address;
        if (!vgk_acpi_rsdp_validate(
                memory_map, memory_map_count,
                boot_info->acpi_rsdp_address,
                cpu_features->physical_address_bits,
                rsdp, &rsdp_length) ||
            !add_paging_requirement_range(
                ranges, &range_count,
                boot_info->acpi_rsdp_address, rsdp_length))
            return 0;
    }

    if ((boot_info->flags & VGK_BOOT_FLAG_FRAMEBUFFER) != 0U &&
        framebuffer_is_uncacheable != 0 &&
        !add_paging_requirement_range(
            ranges, &range_count,
            boot_info->framebuffer_base, boot_info->framebuffer_size))
        return 0;

    calc_result = vgk_paging_requirements_calculate(
        ranges, range_count, &requirements);
    if (calc_result != VGK_PAGING_REQUIREMENTS_OK) {
        serial_write("VGK: kalkulator zapotrzebowania odrzucil zakresy, kod = ");
        serial_write_hex((uint64_t)calc_result);
        serial_write("\r\n");
        return 0;
    }

    serial_write("VGK: wymagane strony tablic = ");
    serial_write_hex(requirements.total_pages);
    serial_write("; dostepne = ");
    serial_write_hex(boot_info->page_table_pool_pages);
    serial_write("\r\n");

    return requirements.total_pages <= boot_info->page_table_pool_pages;
}

static int map_acpi_root_pointer(
    const VGK_BOOT_INFO *boot_info,
    const VGK_CPU_FEATURES *cpu_features,
    VGK_PAGE_TABLES *tables)
{
    const VGK_MEMORY_MAP_ENTRY *memory_map;
    const volatile uint8_t *rsdp;
    size_t memory_map_count;
    uint64_t length = 0;

    if (boot_info == 0 || cpu_features == 0 || tables == 0 ||
        boot_info->memory_map_address == 0 ||
        boot_info->memory_map_size == 0 ||
        boot_info->memory_map_size % sizeof(VGK_MEMORY_MAP_ENTRY) != 0)
        return 0;

    memory_map = (const VGK_MEMORY_MAP_ENTRY *)(uintptr_t)
        boot_info->memory_map_address;
    memory_map_count = (size_t)(
        boot_info->memory_map_size / sizeof(VGK_MEMORY_MAP_ENTRY));
    rsdp = (const volatile uint8_t *)(uintptr_t)
        boot_info->acpi_rsdp_address;

    if (!vgk_acpi_rsdp_validate(
            memory_map, memory_map_count,
            boot_info->acpi_rsdp_address,
            cpu_features->physical_address_bits, rsdp, &length))
        return 0;

    return map_identity_object(
        tables, boot_info->acpi_rsdp_address, length, 0U) ==
        VGK_PAGING_OK;
}

static int build_inactive_page_tables(
    const VGK_BOOT_INFO *boot_info,
    const VGK_CPU_FEATURES *cpu_features,
    VGK_PAGE_TABLES *tables)
{
    VGK_PAGING_RESULT result;
    uint64_t stack_start = (uint64_t)(uintptr_t)vgk_boot_stack;
    uint64_t stack_end = (uint64_t)(uintptr_t)vgk_boot_stack_top;
    int stack_is_writable = 0;
    int framebuffer_is_uncacheable = 0;

    if (boot_info == 0 || cpu_features == 0 || tables == 0 ||
        cpu_features->nx_supported == 0U ||
        cpu_features->physical_address_bits < 32U ||
        stack_start == 0U || stack_end <= stack_start)
        return 0;

    for (uint32_t i = 0; i < boot_info->kernel_segment_count; ++i) {
        const VGK_KERNEL_SEGMENT *segment = &boot_info->kernel_segments[i];
        uint64_t segment_end = segment->physical_base + segment->memory_size;
        if ((segment->flags & VGK_KERNEL_SEGMENT_WRITE) != 0U &&
            (segment->flags & VGK_KERNEL_SEGMENT_EXEC) == 0U &&
            stack_start >= segment->physical_base && stack_end <= segment_end) {
            stack_is_writable = 1;
            break;
        }
    }
    if (!stack_is_writable)
        return 0;
    serial_write("VGK: zakres stosu potwierdzony jako RW+NX.\r\n");

    if ((boot_info->flags & VGK_BOOT_FLAG_FRAMEBUFFER) != 0U)
        framebuffer_is_uncacheable = framebuffer_pcd_pwt_is_uncacheable();

    if (!page_table_pool_has_capacity(
            boot_info, cpu_features, framebuffer_is_uncacheable)) {
        serial_write("VGK: pula tablic stron za mala lub nie mozna zweryfikowac mapowan.\r\n");
        return 0;
    }
    serial_write("VGK: wstepna kontrola pojemnosci tablic stron zakonczona.\r\n");

    result = vgk_paging_init(
        tables,
        (void *)(uintptr_t)boot_info->page_table_pool_base,
        (size_t)boot_info->page_table_pool_pages,
        cpu_features->physical_address_bits,
        cpu_features->nx_supported);
    if (result != VGK_PAGING_OK)
        return 0;
    serial_write("VGK: utworzono korzen nieaktywnych tablic stron.\r\n");

    for (uint32_t i = 0; i < boot_info->kernel_segment_count; ++i) {
        const VGK_KERNEL_SEGMENT *segment = &boot_info->kernel_segments[i];
        uint32_t permissions = 0;

        if ((segment->flags & VGK_KERNEL_SEGMENT_WRITE) != 0U)
            permissions |= VGK_PAGE_WRITE;
        if ((segment->flags & VGK_KERNEL_SEGMENT_EXEC) != 0U)
            permissions |= VGK_PAGE_EXEC;

        result = vgk_paging_map_range(
            tables, segment->physical_base, segment->physical_base,
            segment->memory_size, permissions);
        if (result != VGK_PAGING_OK)
            return 0;
    }
    serial_write("VGK: segmenty ELF zmapowane.\r\n");

    /* Boot protocol and normalized map are read-only and non-executable. */
    serial_write("VGK: adres VGK_BOOT_INFO = ");
    serial_write_hex((uint64_t)(uintptr_t)boot_info);
    serial_write("\r\n");
    result = map_identity_object(
        tables, (uint64_t)(uintptr_t)boot_info,
        sizeof(*boot_info), 0U);
    if (result != VGK_PAGING_OK) {
        serial_write("VGK: blad mapowania VGK_BOOT_INFO, kod = ");
        serial_write_hex((uint64_t)result);
        serial_write("\r\n");
        return 0;
    }
    serial_write("VGK: VGK_BOOT_INFO zmapowany.\r\n");

    serial_write("VGK: adres mapy pamieci = ");
    serial_write_hex(boot_info->memory_map_address);
    serial_write("; rozmiar = ");
    serial_write_hex(boot_info->memory_map_size);
    serial_write("\r\n");
    result = map_identity_object(
        tables, boot_info->memory_map_address,
        boot_info->memory_map_size, 0U);
    if (result != VGK_PAGING_OK) {
        serial_write("VGK: blad mapowania mapy pamieci, kod = ");
        serial_write_hex((uint64_t)result);
        serial_write("\r\n");
        return 0;
    }
    serial_write("VGK: mapa pamieci zmapowana.\r\n");

    if ((boot_info->flags & VGK_BOOT_FLAG_ACPI) != 0U) {
        if (!map_acpi_root_pointer(boot_info, cpu_features, tables)) {
            serial_write("VGK: blad mapowania wskaznika ACPI RSDP.\r\n");
            return 0;
        }
        serial_write("VGK: wskaznik ACPI RSDP zmapowany tylko do odczytu i NX.\r\n");
    }


    /* The builder must remain able to access its own reserved pool. */
    result = map_identity_object(
        tables, boot_info->page_table_pool_base,
        boot_info->page_table_pool_pages * VGK_BOOT_PAGE_SIZE,
        VGK_PAGE_WRITE);
    if (result != VGK_PAGING_OK) {
        serial_write("VGK: blad mapowania puli tablic, kod = ");
        serial_write_hex((uint64_t)result);
        serial_write("\r\n");
        return 0;
    }
    serial_write("VGK: pula tablic zmapowana RW+NX.\r\n");

    /*
     * GOP framebuffer is device memory, not ordinary cacheable RAM.
     * PCD+PWT is used only after confirming that its effective type is UC.
     */
    if ((boot_info->flags & VGK_BOOT_FLAG_FRAMEBUFFER) != 0U) {
        if (!framebuffer_is_uncacheable) {
            serial_write("VGK: pomijam mapowanie framebufferu; PAT entry 3 nie jest UC.\r\n");
        } else {
            result = map_identity_object(
                tables, boot_info->framebuffer_base,
                boot_info->framebuffer_size,
                VGK_PAGE_WRITE | VGK_PAGE_CACHE_DISABLE |
                    VGK_PAGE_WRITE_THROUGH);
            if (result != VGK_PAGING_OK) {
                serial_write("VGK: blad mapowania framebufferu GOP, kod = ");
                serial_write_hex((uint64_t)result);
                serial_write("\r\n");
                return 0;
            }
            serial_write("VGK: framebuffer GOP zmapowany RW+NX; PCD+PWT zweryfikowane jako UC.\r\n");
        }
    }

    return vgk_paging_root_physical(tables) ==
           boot_info->page_table_pool_base;
}

__attribute__((noreturn))
void vgk_kernel_main(const VGK_BOOT_INFO *boot_info)
{
    VGK_CPU_FEATURES cpu_features;
    int cpu_features_valid;

    serial_init();
    serial_write("\r\nVGK: uruchomiono wlasne jadro.\r\n");

    if (!vgk_boot_info_is_valid(boot_info)) {
        serial_write("VGK: BLAD protokolu startowego. Zatrzymanie.\r\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    serial_write("VGK: protokol startowy poprawny.\r\n");

    if ((boot_info->flags & VGK_BOOT_FLAG_PCI) != 0) {
        serial_write("VGK: otrzymano inwentarz urzadzen PCI z UEFI.\r\n");
        if ((boot_info->flags & VGK_BOOT_FLAG_PCI_TRUNCATED) != 0)
            serial_write("VGK: lista PCI zostala ograniczona do maksymalnej liczby wpisow.\r\n");
    } else {
        serial_write("VGK: firmware nie udostepnilo inwentarza PCI.\r\n");
    }

    if ((boot_info->flags & VGK_BOOT_FLAG_FRAMEBUFFER) != 0) {
        serial_write("VGK: przekazano zweryfikowany framebuffer GOP.\r\n");
    } else {
        serial_write("VGK: brak obslugiwanej ramki GOP; pozostaje tryb tekstowo-diagnostyczny.\r\n");
    }

    cpu_features_valid =
        vgk_cpu_detect_features(&cpu_features) == VGK_CPU_OK;
    {
        if (cpu_features_valid) {
            serial_write("VGK: procesor obsluguje NX; wykryto szerokosc adresu fizycznego.\r\n");
            if (cpu_features.sse2_supported != 0U)
                serial_write("VGK: wykryto SSE2.\r\n");
            if (cpu_features.avx_hardware_supported != 0U &&
                cpu_features.avx_state_supported == 0U)
                serial_write("VGK: AVX jest w procesorze, ale stan systemowy go nie udostepnia.\r\n");
            if (cpu_features.avx2_usable != 0U)
                serial_write("VGK: AVX2 jest dostepne; jadro nadal uzywa bezpiecznego zestawu podstawowego.\r\n");
        } else {
            serial_write("VGK: brak potwierdzonej obslugi NX lub szerokosci adresu; ochrona stron nie zostanie wlaczona.\r\n");
        }
    }

    {
        VGK_MEMORY_STATS memory_stats;
        uint64_t test_page = 0;

        if (vgk_memory_init(boot_info) != VGK_MEMORY_OK ||
            vgk_memory_get_stats(&memory_stats) != VGK_MEMORY_OK ||
            memory_stats.total_pages == 0 ||
            vgk_memory_alloc_pages(1U, 1U, &test_page) != VGK_MEMORY_OK ||
            (test_page >= boot_info->page_table_pool_base &&
             test_page < boot_info->page_table_pool_base +
                         boot_info->page_table_pool_pages *
                             VGK_BOOT_PAGE_SIZE) ||
            vgk_memory_free_pages(test_page, 1U) != VGK_MEMORY_OK) {
            serial_write("VGK: BLAD inicjalizacji menedzera pamieci.\r\n");
            for (;;) {
                __asm__ volatile ("cli; hlt");
            }
        }
    }

    if (!page_table_pool_is_zeroed(boot_info)) {
        serial_write("VGK: BLAD dostepu do puli tablic stron lub pula nie jest wyzerowana.\r\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
    serial_write("VGK: pula tablic stron jest dostepna, wyzerowana i zarezerwowana.\r\n");
    if (cpu_features_valid) {
        VGK_PAGE_TABLES inactive_tables;
        if (!build_inactive_page_tables(
                boot_info, &cpu_features, &inactive_tables)) {
            serial_write("VGK: BLAD budowania nieaktywnych mapowan stron.\r\n");
            for (;;) {
                __asm__ volatile ("cli; hlt");
            }
        }
        serial_write("VGK: nieaktywne mapowania jadra, stosu, danych startowych i puli sa gotowe.\r\n");
        /* Deliberately do not load CR3 or change CPU protection registers. */
    } else {
        serial_write("VGK: pomijam budowanie mapowan ochronnych; brak potwierdzonego NX.\r\n");
    }
    serial_write("VGK: menedzer pamieci uruchomiony.\r\n");
    serial_write("VGK: etap diagnostyczny; sterowniki i uslugi nie sa jeszcze gotowe.\r\n");

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
