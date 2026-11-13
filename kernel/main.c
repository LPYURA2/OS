#include "styleos/arch.h"
#include "styleos/boot_info.h"
#include "styleos/boot_memory.h"
#include "styleos/boot_pool.h"
#include "styleos/exception.h"

#ifndef STYLEOS_EXPECT_ARCH
#error STYLEOS_EXPECT_ARCH must identify the compiled target
#endif

_Noreturn void kernel_main(const struct boot_info *info)
{
    arch_puts("STYLEOS kernel_main entered\n");
    enum boot_info_error error = boot_info_validate(info, STYLEOS_EXPECT_ARCH);
    if (error != BOOT_INFO_OK) {
        arch_puts("STYLEOS BOOT_INFO ERROR: ");
        arch_puts(boot_info_error_name(error));
        arch_puts("\n");
        arch_halt();
    }
    arch_puts("STYLEOS boot_info validated v1\n");
#if STYLEOS_EXPECT_ARCH == STYLEOS_ARCH_X86_64
    arch_puts("STYLEOS HANDOFF PASS x86_64\n");
#elif STYLEOS_EXPECT_ARCH == STYLEOS_ARCH_AARCH64
    arch_puts("STYLEOS HANDOFF PASS aarch64\n");
#else
#error Unsupported StyleOS target
#endif
    const struct boot_memory_evidence *evidence = boot_memory_diagnose(info);
#ifdef STYLEOS_TEST_BOOTPOOL
    boot_pool_selftest(evidence);
#else
    (void)evidence;
#endif
#ifdef STYLEOS_TEST_EXCEPTION
    /* Dedicated test image only, after valid handoff and diagnostic map. */
    arch_test_exception();
    arch_puts("STYLEOS BOOT_ERROR exception trigger returned\n");
    arch_halt();
#endif
    /* Candidate RAM remains diagnostic only, including in the pool test. */
    arch_puts("STYLEOS KERNEL HALT\n");
    arch_halt();
}
