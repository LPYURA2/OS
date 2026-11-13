#include <stddef.h>
#include <stdint.h>
#include <styleos/arch.h>
#include <styleos/exception.h>
#include "exceptions.h"

struct idt_gate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint64_t address;
} __attribute__((packed));

_Static_assert(sizeof(struct idt_gate) == 16, "x86-64 IDT gate");
_Static_assert(sizeof(struct idt_pointer) == 10, "x86-64 IDTR operand");
_Static_assert(offsetof(struct idt_gate, offset_high) == 8, "IDT offset high");

static struct idt_gate early_idt[256] __attribute__((aligned(16)));
extern void (*const styleos_isr_table[256])(void);

static void put_hex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    for (unsigned int index = 0; index < 16; ++index)
        arch_putc(digits[(value >> (60u - index * 4u)) & 15u]);
}

void arch_exception_install(void)
{
    uint16_t selector;
    __asm__ volatile ("mov %%cs, %0" : "=r"(selector));
    /* UEFI enters in a 64-bit ring-0 code segment. Keep that active GDT
     * selector, rather than assuming OVMF/VirtualBox use one fixed value.
     * TI=0 is required: an LDT dependency is outside this prototype. */
    if (selector == 0 || (selector & 7u) != 0) {
        arch_puts("STYLEOS BOOT_ERROR unsupported current CS for early IDT\n");
        arch_halt();
    }

    for (unsigned int vector = 0; vector < 256; ++vector) {
        uintptr_t target = (uintptr_t)styleos_isr_table[vector];
        early_idt[vector].offset_low = (uint16_t)target;
        early_idt[vector].selector = selector;
        early_idt[vector].ist = 0;
        early_idt[vector].attributes = 0x8eu; /* present, DPL0, interrupt gate */
        early_idt[vector].offset_middle = (uint16_t)(target >> 16);
        early_idt[vector].offset_high = (uint32_t)(target >> 32);
        early_idt[vector].reserved = 0;
    }
    const struct idt_pointer pointer = {
        .limit = sizeof(early_idt) - 1u,
        .address = (uint64_t)(uintptr_t)early_idt,
    };
    __asm__ volatile ("lidt %0" :: "m"(pointer) : "memory");
    arch_puts("STYLEOS x86_64 IDT ready cs=0x");
    put_hex(selector);
    arch_puts("\n");
}

#ifdef STYLEOS_TEST_EXCEPTION
/* The referenced label begins with UD2 itself, not a function prologue. Its
 * runtime address includes PE relocation and is the expected saved RIP. */
extern void styleos_test_fault(void);

void arch_test_exception(void)
{
    arch_puts("STYLEOS EXCEPTION TEST x86_64 pc=0x");
    put_hex((uint64_t)(uintptr_t)styleos_test_fault);
    arch_puts("\n");
    styleos_test_fault();
    arch_halt();
}
#endif
