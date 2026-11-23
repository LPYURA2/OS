#ifndef STYLEOS_ACPICA_BRIDGE_819_H
#define STYLEOS_ACPICA_BRIDGE_819_H
#include <stddef.h>
#include <stdint.h>
/* Early x86-64 ACPICA OSL transport contract, NOT a linked ACPICA engine.
 * The only mapped addresses are exact subranges of previously checksummed,
 * image-owned ACPI table copies captured BEFORE ExitBootServices.
 * Deliberately no dynamic physical map or PCI register access. */
#define STYLEOS819_MAX_TABLES 16u
#define STYLEOS819_MAX_LENGTH 24576u
#define STYLEOS819_MAX_MAPPINGS 24u
#define STYLEOS819_MAX_REQUESTS 256u
#define STYLEOS819_AE_OK UINT32_C(0x0000)
#define STYLEOS819_AE_ACCESS UINT32_C(0x001d)
#define STYLEOS819_AE_BAD_PARAMETER UINT32_C(0x1001)
#define STYLEOS819_AE_LIMIT UINT32_C(0x0010)
struct styleos819_table {
    uint64_t firmware_pa;
    const uint8_t *copy;
    uint32_t length;
};
struct styleos819_map {
    const void *ptr;
    uint64_t address;
    size_t length;
    uint8_t live;
};
struct styleos819_gate {
    struct styleos819_table tables[STYLEOS819_MAX_TABLES];
    struct styleos819_map maps[STYLEOS819_MAX_MAPPINGS];
    uint32_t generation, mapped, denied_maps, denied_io, invalid, queries;
    uint16_t count;
    uint8_t sealed;
};
/* Whole-list validation is transactional: a failed registration changes no gate. */
int styleos819_prepare(struct styleos819_gate *gate,
                      const struct styleos819_table *tables, size_t count,
                      uint32_t generation);
/* One diagnostic CPU, interrupts off. State remains image owned and immutable. */
int styleos819_activate(struct styleos819_gate *gate, uint32_t generation);
void styleos819_deactivate(struct styleos819_gate *gate);
/* ACPICA-style ABI subset for trial linkage only; compile against upstream
 * acpiosxf.h and a full OSL before any ACPICA runtime can be accepted. */
typedef uint32_t STYLEOS819_ACPI_STATUS;
typedef struct {
    uint16_t Segment, Bus, Device, Function;
} STYLEOS819_PCI_ID;
void *AcpiOsMapMemory(uint64_t address, size_t length);
void AcpiOsUnmapMemory(void *logical, size_t length);
STYLEOS819_ACPI_STATUS AcpiOsReadMemory(uint64_t addr,uint64_t *value,uint32_t width);
STYLEOS819_ACPI_STATUS AcpiOsWriteMemory(uint64_t addr,uint64_t value,uint32_t width);
STYLEOS819_ACPI_STATUS AcpiOsReadPort(uint64_t addr,uint32_t *value,uint32_t width);
STYLEOS819_ACPI_STATUS AcpiOsWritePort(uint64_t addr,uint32_t value,uint32_t width);
STYLEOS819_ACPI_STATUS AcpiOsReadPciConfiguration(STYLEOS819_PCI_ID *id,
    uint32_t reg,uint64_t *value,uint32_t width);
STYLEOS819_ACPI_STATUS AcpiOsWritePciConfiguration(STYLEOS819_PCI_ID *id,
    uint32_t reg,uint64_t value,uint32_t width);
int styleos819_dma_grant(uint16_t requester);
#endif
