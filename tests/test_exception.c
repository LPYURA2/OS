#include "styleos/exception.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

static char output[2048];
static size_t used;
static jmp_buf halted;

static void capture(char ch)
{
    assert(used + 1 < sizeof(output));
    output[used++] = ch;
    output[used] = '\0';
}

static void clear_output(void)
{
    used = 0;
    output[0] = '\0';
}

void arch_putc(char ch) { capture(ch); }
void arch_puts(const char *value) { while (*value) capture(*value++); }
void arch_console_init(void) { }
_Noreturn void arch_halt(void) { longjmp(halted, 1); }

static void arm_decode_tests(void)
{
    struct arm_exception_class c = exception_decode_arm(UINT64_C(0x96000061));
    assert(c.ec == 0x25 && c.far_valid);
    assert(strcmp(c.reason, "alignment-fault") == 0);
    c = exception_decode_arm(UINT64_C(0x96000461));
    assert(c.ec == 0x25 && !c.far_valid);
    c = exception_decode_arm(UINT64_C(0xf2000120));
    assert(c.ec == 0x3c && !c.far_valid && strcmp(c.reason, "brk") == 0);
    c = exception_decode_arm(UINT64_C(0x96000004));
    assert(c.far_valid && strcmp(c.reason, "translation-fault") == 0);
    c = exception_decode_arm(UINT64_C(0x8600000d));
    assert(c.far_valid && strcmp(c.reason, "permission-fault") == 0);
    c = exception_decode_arm(UINT64_C(0x96000010));
    assert(c.far_valid && strcmp(c.reason, "external-abort") == 0);
    c = exception_decode_arm(UINT64_C(0x96000410));
    assert(!c.far_valid);
    c = exception_decode_arm(UINT64_C(0x96000011));
    assert(!c.far_valid); /* Tag-check case deliberately not promised. */
    c = exception_decode_arm(UINT64_C(0x96000014));
    assert(!c.far_valid); /* External translation-table-walk syndrome. */
    c = exception_decode_arm(UINT64_C(0x9a000000));
    assert(c.ec == 0x26 && !c.far_valid && strcmp(c.reason, "sp-alignment") == 0);
    c = exception_decode_arm(UINT64_C(0xfe000000));
    assert(c.ec == 0x3f && !c.far_valid);
    c = exception_decode_arm(0);
    assert(!c.far_valid && strcmp(c.reason, "unknown-reason") == 0);
}

static void arm_format_tests(void)
{
    clear_output();
    exception_format_arm(UINT64_C(0xf2000120), UINT64_C(0x40080080),
                         0x3c5, UINT64_MAX, capture);
    assert(strcmp(output,
        "STYLEOS EXCEPTION arch=aarch64 kind=sync class=0x3c reason=brk"
        " pc=0x0000000040080080 esr=0x00000000f2000120"
        " elr=0x0000000040080080 spsr=0x00000000000003c5 far=absent\n") == 0);
    clear_output();
    exception_format_arm(UINT64_C(0x96000061), UINT64_MAX, 0x3c5,
                         UINT64_C(0x40090001), capture);
    assert(strstr(output, "reason=alignment-fault") != NULL);
    assert(strstr(output, "pc=0xffffffffffffffff") != NULL);
    assert(strstr(output, "far=0x0000000040090001\n") != NULL);
    clear_output();
    exception_format_arm(UINT64_C(0x96000461), 1, 0, 123, capture);
    assert(strstr(output, "far=absent\n") != NULL);
    assert(strstr(output, "000000000000007b") == NULL);
}

static void x86_tests(void)
{
    const unsigned int hardware[] = {8, 10, 11, 12, 13, 14, 17, 21, 29, 30};
    for (unsigned int vector = 0; vector < 256; ++vector) {
        int expected = 0;
        for (size_t i = 0; i < sizeof(hardware) / sizeof(hardware[0]); ++i)
            if (vector == hardware[i]) expected = 1;
        assert(exception_x86_has_error(vector) == expected);
    }
    clear_output();
    exception_format_x86(6, UINT64_C(0x1234), 0, 0, UINT64_MAX, capture);
    assert(strcmp(output,
        "STYLEOS EXCEPTION arch=x86_64 kind=sync vector=6 reason=invalid-opcode"
        " pc=0x0000000000001234 error=absent cr2=absent\n") == 0);
    clear_output();
    exception_format_x86(6, 1, 1, 0, 0, capture);
    assert(strstr(output, "error=absent") != NULL); /* Synthetic zero stays absent. */
    clear_output();
    exception_format_x86(13, 2, 1, 0, 0, capture);
    assert(strstr(output, "reason=general-protection") != NULL);
    assert(strstr(output, "error=0x0000000000000000 cr2=absent") != NULL);
    clear_output();
    exception_format_x86(14, UINT64_MAX, 1, 7, UINT64_C(0xdeadbeef), capture);
    assert(strstr(output, "reason=page-fault") != NULL);
    assert(strstr(output, "error=0x0000000000000007 cr2=0x00000000deadbeef") != NULL);
    clear_output();
    exception_format_x86(2, 123, 0, 0, UINT64_MAX, capture);
    assert(strstr(output, "kind=unsupported vector=2 reason=nmi") != NULL);
    assert(strstr(output, "error=absent cr2=absent") != NULL);
    clear_output();
    exception_format_x86(255, 123, 0, 0, 0, capture);
    assert(strstr(output, "kind=unsupported vector=255 reason=external-interrupt") != NULL);
}

static void terminal_guard_tests(void)
{
    clear_output();
    if (setjmp(halted) == 0)
        early_exception_arm_sync(UINT64_C(0xf2000120), UINT64_C(0x40080080),
                                 0x3c5, UINT64_MAX);
    assert(strstr(output, "STYLEOS EXCEPTION arch=aarch64 kind=sync") == output);
    assert(strstr(output, "\nSTYLEOS EMERGENCY HALT\n") != NULL);
    size_t first_size = used;
    if (setjmp(halted) == 0)
        early_exception_x86(6, 0x1234, 0, 0, 0);
    assert(used == first_size); /* Re-entry neither logs nor retries the UART. */
    assert(strstr(output, "STYLEOS KERNEL HALT") == NULL);
}

int main(void)
{
    arm_decode_tests();
    arm_format_tests();
    x86_tests();
    terminal_guard_tests();
    puts("early exception decoder/formatter/first-entry tests: PASS");
    return 0;
}
