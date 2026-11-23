/* Compile and call S819 OSL exports against REAL ACPICA 20260930 types. */
#include "acpi.h"
#include <stdint.h>
#include <stddef.h>
_Static_assert(sizeof(ACPI_STATUS)==4,"ACPICA status ABI");
_Static_assert(sizeof(ACPI_PCI_ID)==8,"ACPICA PCI ID");
_Static_assert(sizeof(ACPI_PHYSICAL_ADDRESS)==8,"physical width");
_Static_assert(sizeof(ACPI_SIZE)==sizeof(size_t),"size width");
_Static_assert(AE_ACCESS==0x001d,"AE_ACCESS mismatch");
_Static_assert(AE_LIMIT==0x0010,"AE_LIMIT mismatch");
_Static_assert(AE_BAD_PARAMETER==0x1001,"AE_BAD_PARAMETER mismatch");
int main(void){
 ACPI_PCI_ID pci={0};UINT64 value=UINT64_MAX;UINT32 port=UINT32_MAX;
 if(AcpiOsMapMemory(0xe0000000,4096)!=NULL)return 1;
 if(AcpiOsReadMemory(0xe0000000,&value,32)!=AE_ACCESS)return 2;
 if(value!=UINT64_MAX)return 3;
 if(AcpiOsWriteMemory(0xe0000000,0,32)!=AE_ACCESS)return 4;
 if(AcpiOsReadPort(0xcf8,&port,32)!=AE_ACCESS)return 5;
 if(port!=UINT32_MAX)return 6;
 if(AcpiOsWritePort(0xcf8,0,32)!=AE_ACCESS)return 7;
 if(AcpiOsReadPciConfiguration(&pci,0,&value,32)!=AE_ACCESS)return 8;
 if(AcpiOsWritePciConfiguration(&pci,0,0,32)!=AE_ACCESS)return 9;
 return 0;
}
