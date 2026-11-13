#include <styleos/arch.h>
#include <stdint.h>

#define COM1 0x3f8u

static inline void out8(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint8_t in8(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void arch_console_init(void)
{
    out8(COM1 + 1u, 0);
    out8(COM1 + 3u, 0x80u);
    out8(COM1 + 0u, 1u);
    out8(COM1 + 1u, 0);
    out8(COM1 + 3u, 3u);
    out8(COM1 + 2u, 0xc7u);
    out8(COM1 + 4u, 3u);
}

void arch_putc(char ch)
{
    if (ch == '\n')
        arch_putc('\r');
    /* Missing physical UARTs must not make the early boot code spin forever. */
    for (unsigned int poll = 0; poll < 100000u; ++poll) {
        if ((in8(COM1 + 5u) & 0x20u) != 0) {
            out8(COM1, (uint8_t)ch);
            return;
        }
        __asm__ volatile ("pause");
    }
}

void arch_puts(const char *text)
{
    while (*text != '\0')
        arch_putc(*text++);
}

_Noreturn void arch_halt(void)
{
    for (;;)
        __asm__ volatile ("cli; hlt" ::: "memory");
}
