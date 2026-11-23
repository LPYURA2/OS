#ifndef STYLEOS_ACPICA_EFI_TYPES_820_H
#define STYLEOS_ACPICA_EFI_TYPES_820_H
/* ACPICA EFI acefi.h expects UINT64 from the boot environment BEFORE acpi.h.
   This is a compile-only x86-64 COFF ABI prelude; it is not an OSL runtime. */
typedef unsigned long long UINT64;
#endif
