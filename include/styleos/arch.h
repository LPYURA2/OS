#ifndef STYLEOS_ARCH_H
#define STYLEOS_ARCH_H
void arch_console_init(void);
void arch_putc(char ch);
void arch_puts(const char *text);
_Noreturn void arch_halt(void);
#endif
