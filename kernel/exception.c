#include "styleos/arch.h"
#include "styleos/exception.h"

static volatile unsigned int exception_entered;

static void text(void (*putc)(char), const char *value)
{
    while (*value)
        putc(*value++);
}

static void hexadecimal(void (*putc)(char), uint64_t value, unsigned int digits)
{
    text(putc, "0x");
    for (unsigned int n = digits; n != 0; --n)
        putc("0123456789abcdef"[(value >> ((n - 1u) * 4u)) & 15u]);
}

static void decimal(void (*putc)(char), uint64_t value)
{
    uint64_t divisor = 1;
    while (value / divisor >= 10 && divisor <= UINT64_MAX / 10)
        divisor *= 10;
    do {
        putc((char)('0' + value / divisor));
        value %= divisor;
        divisor /= 10;
    } while (divisor);
}

/* FAR is deliberately conservative. For known instruction/data aborts we
 * accept address-size, translation, access-flag and permission faults (0..15),
 * synchronous external abort without table walk (16), and data alignment (33).
 * FnV=1 always suppresses the address. Other FSCs (including implementation-
 * specific, tag-check or external table-walk cases) stay absent, even if an
 * implementation might provide a useful FAR. Non-abort ECs never inherit FAR.
 * This classifies a captured synchronous syndrome; asynchronous entries do not
 * call this decoder with a potentially stale ESR. */
struct arm_exception_class exception_decode_arm(uint64_t esr)
{
    struct arm_exception_class result = {
        .ec = (uint32_t)((esr >> 26) & 63u),
        .reason = "unknown-synchronous", .far_valid = 0
    };
    const uint32_t fsc = (uint32_t)(esr & 63u);
    const int instruction = result.ec == 0x20 || result.ec == 0x21;
    const int data = result.ec == 0x24 || result.ec == 0x25;
    if (instruction || data) {
        if (data && fsc == 0x21)
            result.reason = "alignment-fault";
        else if (fsc <= 3)
            result.reason = "address-size-fault";
        else if (fsc <= 7)
            result.reason = "translation-fault";
        else if (fsc <= 11)
            result.reason = "access-flag-fault";
        else if (fsc <= 15)
            result.reason = "permission-fault";
        else if (fsc == 16)
            result.reason = "external-abort";
        else
            result.reason = instruction ? "instruction-abort" : "data-abort";
        result.far_valid = !(esr & (UINT64_C(1) << 10)) &&
            (fsc <= 16 || (data && fsc == 0x21));
    } else {
        switch (result.ec) {
        case 0x00: result.reason = "unknown-reason"; break;
        case 0x01: result.reason = "trapped-wfi-wfe"; break;
        case 0x07: result.reason = "fp-simd-access"; break;
        case 0x0e: result.reason = "illegal-execution-state"; break;
        case 0x15: result.reason = "svc64"; break;
        case 0x18: result.reason = "system-register-trap"; break;
        case 0x22: result.reason = "pc-alignment"; break;
        case 0x26: result.reason = "sp-alignment"; break;
        case 0x2c: result.reason = "floating-point"; break;
        case 0x30: case 0x31: result.reason = "hardware-breakpoint"; break;
        case 0x32: case 0x33: result.reason = "software-step"; break;
        case 0x34: case 0x35: result.reason = "watchpoint"; break;
        case 0x3c: result.reason = "brk"; break;
        default: break;
        }
    }
    return result;
}

int exception_x86_has_error(uint64_t vector)
{
    switch (vector) {
    case 8: case 10: case 11: case 12: case 13: case 14: case 17:
    case 21: case 29: case 30:
        return 1;
    default:
        return 0;
    }
}

const char *exception_x86_reason(uint64_t vector)
{
    switch (vector) {
    case 0: return "divide-error";
    case 1: return "debug";
    case 2: return "nmi";
    case 3: return "breakpoint";
    case 4: return "overflow";
    case 5: return "bound-range";
    case 6: return "invalid-opcode";
    case 7: return "device-not-available";
    case 8: return "double-fault";
    case 10: return "invalid-tss";
    case 11: return "segment-not-present";
    case 12: return "stack-segment";
    case 13: return "general-protection";
    case 14: return "page-fault";
    case 16: return "x87-floating-point";
    case 17: return "alignment-check";
    case 18: return "machine-check";
    case 19: return "simd-floating-point";
    case 20: return "virtualization";
    case 21: return "control-protection";
    case 29: return "vmm-communication";
    case 30: return "security";
    default: return vector >= 32 ? "external-interrupt" : "unsupported-vector";
    }
}

static int supported_x86(uint64_t vector)
{
    return (vector <= 21 && vector != 2 && vector != 9 && vector != 15 &&
            vector != 18) || vector == 29 || vector == 30;
}

void exception_format_arm(uint64_t esr, uint64_t pc, uint64_t spsr, uint64_t far,
                          void (*putc)(char))
{
    const struct arm_exception_class decoded = exception_decode_arm(esr);
    text(putc, "STYLEOS EXCEPTION arch=aarch64 kind=sync class=");
    hexadecimal(putc, decoded.ec, 2);
    text(putc, " reason="); text(putc, decoded.reason);
    text(putc, " pc="); hexadecimal(putc, pc, 16);
    text(putc, " esr="); hexadecimal(putc, esr, 16);
    text(putc, " elr="); hexadecimal(putc, pc, 16);
    text(putc, " spsr="); hexadecimal(putc, spsr, 16);
    text(putc, " far=");
    if (decoded.far_valid) hexadecimal(putc, far, 16);
    else text(putc, "absent");
    putc('\n');
}

void exception_format_x86(uint64_t vector, uint64_t pc, int has_error,
                          uint64_t error, uint64_t cr2, void (*putc)(char))
{
    text(putc, "STYLEOS EXCEPTION arch=x86_64 kind=");
    text(putc, supported_x86(vector) ? "sync" : "unsupported");
    text(putc, " vector="); decimal(putc, vector);
    text(putc, " reason="); text(putc, exception_x86_reason(vector));
    text(putc, " pc="); hexadecimal(putc, pc, 16);
    text(putc, " error=");
    /* Frame-normalization padding is not an architectural error code. Even a
     * mistaken caller cannot turn the synthetic #UD zero into hardware data. */
    if (has_error && exception_x86_has_error(vector)) hexadecimal(putc, error, 16);
    else text(putc, "absent");
    text(putc, " cr2=");
    if (vector == 14) hexadecimal(putc, cr2, 16);
    else text(putc, "absent");
    putc('\n');
}

static void claim_first_exception(void)
{
    /* One boot CPU, IRQs masked. A synchronous repeat during UART formatting
     * reaches halt without another UART attempt or another full record. This
     * is not recovery from a broken stack, memory or asynchronous NMI/SError. */
    if (exception_entered)
        arch_halt();
    exception_entered = 1;
}

static _Noreturn void emergency_stop(void)
{
    arch_puts("STYLEOS EMERGENCY HALT\n");
    arch_halt();
}

_Noreturn void early_exception_arm_sync(uint64_t esr, uint64_t pc,
                                       uint64_t spsr, uint64_t far)
{
    claim_first_exception();
    exception_format_arm(esr, pc, spsr, far, arch_putc);
    emergency_stop();
}

_Noreturn void early_exception_arm_unsupported(uint64_t slot)
{
    claim_first_exception();
    text(arch_putc, "STYLEOS EXCEPTION arch=aarch64 kind=unsupported slot=");
    decimal(arch_putc, slot);
    text(arch_putc, " reason=unsupported-vector pc=absent esr=absent elr=absent"
                    " spsr=absent far=absent\n");
    emergency_stop();
}

_Noreturn void early_exception_x86(uint64_t vector, uint64_t pc, int has_error,
                                 uint64_t error, uint64_t cr2)
{
    claim_first_exception();
    exception_format_x86(vector, pc, has_error, error, cr2, arch_putc);
    emergency_stop();
}
