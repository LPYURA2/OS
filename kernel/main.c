#include "styleos/arch.h"
#include "styleos/boot_info.h"
#include "styleos/boot_memory.h"
#include "styleos/boot_pool.h"
#include "styleos/exception.h"
#if defined(STYLEOS_TEST_DESKTOP_DISPLAY) || defined(STYLEOS_DESKTOP_V0)
#include "styleos/desktop_display.h"
#endif
#ifdef STYLEOS_DESKTOP_V0
#include "styleos/desktop_v0.h"
#endif
#ifdef STYLEOS_TEST_DISK_APP
#include "styleos/disk_app_demo.h"
#endif
#ifdef STYLEOS_TEST_STORAGE
#include "styleos/storage_demo.h"
#endif
#ifdef STYLEOS_TEST_USER_PROCESSES
#include "styleos/user_processes.h"
#endif
#ifdef STYLEOS_TEST_TASKS
#include "styleos/tasks.h"
#endif
#ifdef STYLEOS_TEST_OWNERSHIP
#include "styleos/ownership.h"
#endif
#ifdef STYLEOS_TEST_PAGING
#include "styleos/paging.h"
#endif

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
#ifdef STYLEOS_DESKTOP_V0
    const struct desktop_surface *desktop = desktop_platform_surface();
    if (!desktop_surface_excludes_boot_memory(desktop, evidence)) {
        arch_puts("STYLEOS DESKTOP ERROR framebuffer or map\n");
        arch_halt();
    }
    desktop_v0_run(desktop);
#endif
#ifdef STYLEOS_TEST_DESKTOP_DISPLAY
    const struct desktop_surface *surface = desktop_platform_surface();
    if (desktop_surface_excludes_boot_memory(surface, evidence) &&
        desktop_draw_check(surface))
        arch_puts("STYLEOS DISPLAY PASS\n");
    else
        arch_puts("STYLEOS DISPLAY ERROR: unsupported framebuffer or memory map\n");
#endif
#ifdef STYLEOS_TEST_DISK_APP
    disk_app_demo_run(evidence);
#endif
#ifdef STYLEOS_TEST_STORAGE
    storage_demo_run(evidence);
#endif
#ifdef STYLEOS_TEST_USER_PROCESSES
    up_demo_run(evidence);
#endif
#ifdef STYLEOS_TEST_TASKS
    tasks_demo_run(evidence);
#endif
#ifdef STYLEOS_TEST_OWNERSHIP
    ownership_plan_run(info,evidence);
#endif
#ifdef STYLEOS_TEST_PAGING
    paging_experiment(evidence);
#endif
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
