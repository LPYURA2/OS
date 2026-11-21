#ifndef STYLEOS_TEA_768_DEMO_H
#define STYLEOS_TEA_768_DEMO_H
#include <stddef.h>
#include <stdint.h>
#include <styleos/native_tea_elf.h>
extern unsigned char styleos_tea_768_code_b[NTE_PAGE];
/* Only call after the x86 read-only proof of ownership and writable mapping. */
int tea_768_prepare(void);
uint64_t tea_768_yield_pc(void);
uint64_t tea_768_resume_pc(void);
uint64_t tea_768_exit_pc(void);
void tea_768_report(uint32_t pid);
#endif
