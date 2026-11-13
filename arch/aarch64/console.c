#include <stdint.h>
#include "styleos/arch.h"

#define PL011_BASE UINT64_C(0x09000000)
#define UART_DR 0x00u
#define UART_FR 0x18u
#define UART_IBRD 0x24u
#define UART_FBRD 0x28u
#define UART_LCR_H 0x2cu
#define UART_CR 0x30u
#define UART_IMSC 0x38u
#define UART_ICR 0x44u

static volatile uint32_t *uart_register(uint32_t offset)
{
    return (volatile uint32_t *)(uintptr_t)(PL011_BASE + offset);
}

void arch_console_init(void)
{
    *uart_register(UART_CR) = 0;
    *uart_register(UART_IMSC) = 0;
    *uart_register(UART_ICR) = 0x7ff;
    /* QEMU virt PL011 uses a 24 MHz clock: 115200 baud, 8 data bits, FIFO. */
    *uart_register(UART_IBRD) = 13;
    *uart_register(UART_FBRD) = 1;
    *uart_register(UART_LCR_H) = (3u << 5) | (1u << 4);
    *uart_register(UART_CR) = 1u | (1u << 8) | (1u << 9);
    __asm__ volatile("dsb sy" ::: "memory");
}

static void uart_write(char ch)
{
    /* Keep an absent/stuck UART from trapping the boot path in an endless poll.
     * Without a working output device there is no alternate diagnostic sink. */
    for (uint32_t attempt = 0; attempt < 1000000; ++attempt) {
        if ((*uart_register(UART_FR) & (1u << 5)) == 0) {
            *uart_register(UART_DR) = (uint8_t)ch;
            return;
        }
    }
}

void arch_putc(char ch)
{
    if (ch == '\n')
        uart_write('\r');
    uart_write(ch);
}

void arch_puts(const char *text)
{
    while (*text)
        arch_putc(*text++);
}

_Noreturn void arch_halt(void)
{
    __asm__ volatile("msr daifset, #0xf" ::: "memory");
    for (;;)
        __asm__ volatile("wfe");
}
