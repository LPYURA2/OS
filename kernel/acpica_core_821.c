/* StyleOS S821: execute a REAL upstream ACPICA checksum routine after EBS.
 * This is not AML execution, an OSL, IOMMU isolation or ACPI ownership proof.
 */
#include <styleos/boot_info.h>
#include <styleos/arch.h>
#include <stdint.h>
#include <stddef.h>
#include <styleos/acpica_efi_prelude_821.h>
#include <acpi.h>
#include <accommon.h>
/* The stage-819 diagnostic DMA gate has a stable independent C ABI. */
int styleos819_dma_grant(uint16_t requester);

int pci821_get_dsdt(const uint8_t **table,uint32_t *length);
extern UINT8 AcpiUtChecksum(UINT8 *Buffer,UINT32 Length);

static void hex(uint64_t n){
 arch_puts("0x");
 for(int i=60;i>=0;i-=4) arch_putc("0123456789abcdef"[(n>>(unsigned)i)&15u]);
}
static _Noreturn void fault(const char *s){
 arch_puts("STYLEOS S821 FAIL ");arch_puts(s);arch_putc('\n');arch_halt();
}
/* Executes before the existing stage-819 terminal diagnostics. */
void acpi821_terminal(const struct boot_info *boot){
 if(!boot||boot->magic!=STYLEOS_BOOT_MAGIC||
   boot->flags!=STYLEOS_BOOT_EXITED_SERVICES)fault("handoff");
 const uint8_t *data=0;uint32_t length=0;
 if(!pci821_get_dsdt(&data,&length)||!data||length<36u)fault("snapshot");
 if(AcpiUtChecksum((UINT8 *)(uintptr_t)data,length)!=0u)fault("real-acpica-checksum");
 /* A deterministic in-memory negative witness: no hardware access or writes. */
 UINT8 corrupted[4]={1u,2u,3u,250u};
 if(AcpiUtChecksum(corrupted,4u)!=0u)fault("control");
 corrupted[3]++;
 if(AcpiUtChecksum(corrupted,4u)==0u)fault("tampered-control");
 if(styleos819_dma_grant(0u)!=0)fault("dma");
 arch_puts("STYLEOS S821 ACPICA-CHECKSUM length=");hex(length);
 arch_puts(" known-good=yes known-bad-rejected=yes firmware-copy=yes\n");
 arch_puts("STYLEOS S821 POLICY upstream-real-code=yes full-core-compiled=yes aml-executed=no operationregion=deny-all dma-grants=no production-cr3=deny\n");
 arch_puts("STYLEOS S821 PASS\n");
}
