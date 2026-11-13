#ifndef STYLEOS_EXCEPTION_H
#define STYLEOS_EXCEPTION_H
#include <stdint.h>

struct arm_exception_class {
    uint32_t ec;
    const char *reason;
    int far_valid;
};

/* Pure classification/formatting is host-testable. No saved PC/address is read. */
struct arm_exception_class exception_decode_arm(uint64_t esr);
const char *exception_x86_reason(uint64_t vector);
int exception_x86_has_error(uint64_t vector);
void exception_format_arm(uint64_t esr, uint64_t pc, uint64_t spsr, uint64_t far,
                          void (*putc)(char));
void exception_format_x86(uint64_t vector, uint64_t pc, int has_error,
                          uint64_t error, uint64_t cr2, void (*putc)(char));

/* Terminal entry points, first record only, then separate emergency halt. */
_Noreturn void early_exception_arm_sync(uint64_t esr, uint64_t pc,
                                       uint64_t spsr, uint64_t far);
_Noreturn void early_exception_arm_unsupported(uint64_t slot);
_Noreturn void early_exception_x86(uint64_t vector, uint64_t pc, int has_error,
                                 uint64_t error, uint64_t cr2);

/* Implemented by each architecture, present ONLY in --test-exception images. */
#ifdef STYLEOS_TEST_EXCEPTION
void arch_test_exception(void);
#endif
#endif
