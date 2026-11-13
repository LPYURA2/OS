#include "styleos/arch.h"
#include "styleos/boot_pool.h"

#ifdef STYLEOS_TEST_BOOTPOOL
/* This test only dereferences pages returned by the admitted image-owned pool.
 * Printed addresses use the kernel's existing mapping, not a translation from
 * arbitrary C pointers into physical addresses. */
static _Noreturn void fail(const char *reason)
{
    arch_puts("STYLEOS BOOTPOOL ERROR: ");
    arch_puts(reason);
    arch_puts("\n");
    arch_halt();
}

static void hex64(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    arch_puts("0x");
    for (unsigned int shift = 64; shift != 0; shift -= 4)
        arch_putc(digits[(value >> (shift - 4)) & UINT64_C(15)]);
}

static void print_range(const char *name, struct boot_range range)
{
    arch_puts("STYLEOS BOOTPOOL ");
    arch_puts(name);
    arch_puts(" ");
    hex64(range.start);
    arch_puts(" ");
    hex64(range.end);
    arch_puts("\n");
}

static int zero_page(const volatile uint8_t *page)
{
    for (size_t offset = 0; offset < BOOT_POOL_PAGE; ++offset)
        if (page[offset] != 0)
            return 0;
    return 1;
}

static uint8_t page_pattern(unsigned int index, size_t offset)
{
    /* Each page has a different pattern at every corresponding byte. Including
     * both parts of the offset also exercises each complete 4096-byte page. */
    return (uint8_t)(offset ^ (offset >> 8) ^ ((index + 1u) * 29u));
}

void boot_pool_selftest(const struct boot_memory_evidence *evidence)
{
    enum boot_pool_error error = boot_pool_admit(evidence);
    if (error != BP_OK)
        fail(boot_pool_error_name(error));

    const struct boot_range pool = boot_pool_storage_range();
    const struct boot_range metadata = boot_pool_metadata_range();
    if (pool.end <= pool.start || pool.end - pool.start != BOOT_POOL_BYTES ||
        pool.start % BOOT_POOL_PAGE != 0 || pool.end % BOOT_POOL_PAGE != 0)
        fail("selftest pool boundaries");

    /* Admission has succeeded; no pages have been issued or touched yet. */
    print_range("RANGE", pool);
    print_range("META", metadata);
    print_range("WORKSPACE", evidence->workspace);
    arch_puts("STYLEOS BOOTPOOL ADMIT PASS\n");

    void *pages[BOOT_POOL_PAGES];
    for (unsigned int index = 0; index < BOOT_POOL_PAGES; ++index) {
        struct boot_pool_result result = boot_pool_alloc();
        if (result.error != BP_OK)
            fail(boot_pool_error_name(result.error));
        if (result.page == 0)
            fail("selftest null page");
        const uint64_t address = (uint64_t)(uintptr_t)result.page;
        /* Check containment before the first read. Subtraction avoids an
         * overflowing address + page-size check at the end of address space. */
        if (address % BOOT_POOL_PAGE != 0 || address < pool.start ||
            address > pool.end - BOOT_POOL_PAGE)
            fail("selftest page outside aligned pool");
        for (unsigned int previous = 0; previous < index; ++previous)
            if (pages[previous] == result.page)
                fail("selftest duplicate page");
        if (!zero_page(result.page))
            fail("selftest initial page not zero");
        pages[index] = result.page;
        arch_puts("STYLEOS BOOTPOOL ALLOC ");
        arch_putc((char)('0' + index));
        arch_puts(" ");
        hex64(address);
        arch_puts(" ZERO\n");
    }

    for (unsigned int index = 0; index < BOOT_POOL_PAGES; ++index) {
        volatile uint8_t *page = pages[index];
        for (size_t offset = 0; offset < BOOT_POOL_PAGE; ++offset)
            page[offset] = page_pattern(index, offset);
    }
    for (unsigned int index = 0; index < BOOT_POOL_PAGES; ++index) {
        const volatile uint8_t *page = pages[index];
        for (size_t offset = 0; offset < BOOT_POOL_PAGE; ++offset)
            if (page[offset] != page_pattern(index, offset))
                fail("selftest readback mismatch");
    }
    arch_puts("STYLEOS BOOTPOOL RW PASS pages=8 bytes=4096\n");

    struct boot_pool_result result = boot_pool_alloc();
    if (result.error != BP_EXHAUSTED || result.page != 0)
        fail("selftest exhaustion result");
    arch_puts("STYLEOS BOOTPOOL EXHAUSTED PASS\n");

    error = boot_pool_free(pages[3]);
    if (error != BP_OK)
        fail(boot_pool_error_name(error));
    result = boot_pool_alloc();
    if (result.error != BP_OK)
        fail(boot_pool_error_name(result.error));
    if (result.page != pages[3])
        fail("selftest reuse address");
    if (!zero_page(result.page))
        fail("selftest reused page not zero");

    /* Reissuing one page must not clear or change another outstanding page. */
    for (unsigned int index = 0; index < BOOT_POOL_PAGES; ++index) {
        if (index == 3)
            continue;
        const volatile uint8_t *page = pages[index];
        for (size_t offset = 0; offset < BOOT_POOL_PAGE; ++offset)
            if (page[offset] != page_pattern(index, offset))
                fail("selftest reuse changed another page");
    }
    arch_puts("STYLEOS BOOTPOOL REUSE ");
    hex64((uint64_t)(uintptr_t)result.page);
    arch_puts(" ZERO\n");
    arch_puts("STYLEOS BOOTPOOL PASS\n");
}
#endif
