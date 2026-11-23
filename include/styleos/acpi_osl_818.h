#ifndef STYLEOS_ACPI_OSL_818_H
#define STYLEOS_ACPI_OSL_818_H
#include <stddef.h>
#include <stdint.h>
/* Early freestanding policy substrate for a future ACPICA OS Services Layer.
 * NOT an ACPICA linkage, OS interpreter, physical address mapper or grant. */
#define ACPI818_MAX_TABLE (24576u)
#define ACPI818_MAX_QUERIES (256u)
enum acpi818_space { ACPI818_SYSTEM_MEMORY=0, ACPI818_SYSTEM_IO=1,
                    ACPI818_PCI_CONFIG=2, ACPI818_EMBEDDED_CONTROLLER=3 };
enum acpi818_status { ACPI818_OK=0, ACPI818_DENIED=1,
                      ACPI818_INVALID=2, ACPI818_EXHAUSTED=3 };
struct acpi818_gate {
 const uint8_t *table; uint32_t length, generation;
 uint16_t queries, denied, copied_reads, invalid;
 uint8_t valid;
};
/* Input MUST be a private, durable, checksummed copy of DSDT/SSDT. */
int acpi818_open(struct acpi818_gate *, const uint8_t *, size_t, uint32_t);
/* Only copies from the already validated image-owned ACPI table, never MMIO. */
enum acpi818_status acpi818_table_read(struct acpi818_gate *, uint32_t,
  size_t, unsigned, uint64_t *);
/* Refuses all untrusted AML OperationRegion requests: never dereferences PA,
 * never writes I/O port or PCI registers, never modifies the output word. */
enum acpi818_status acpi818_region(struct acpi818_gate *, unsigned space,
  uint64_t address, unsigned bits, int write, uint64_t *value);
int acpi818_dma_grant(const struct acpi818_gate *, uint16_t requester);
#endif
