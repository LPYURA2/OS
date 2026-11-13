#ifndef STYLEOS_X86_EXCEPTIONS_H
#define STYLEOS_X86_EXCEPTIONS_H

/* Called after ExitBootServices, with IF=0, on the private StyleOS stack.
 * Reuses the current ring-0 GDT code selector; does not install a GDT or TSS. */
void arch_exception_install(void);

#endif
