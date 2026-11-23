#ifndef STYLEOS_ACPI_NAMESPACE_816_H
#define STYLEOS_ACPI_NAMESPACE_816_H
#include <stddef.h>
#include <stdint.h>
#define ACPI816_MAX_SSDT 8u
#define ACPI816_MAX_TABLE 24576u
#define ACPI816_MAX_OBJECTS 128u
#define ACPI816_MAX_DEPTH 8u
#define ACPI816_PATH_SIZE 64u
#define ACPI816_MAX_RESOURCE 256u
/* A literal-only AML test evaluator. Unsupported bytecode is NEVER run. */
struct acpi816_table { const uint8_t *bytes; size_t length; };
struct acpi816_object {
 char path[ACPI816_PATH_SIZE];
 uint64_t number;
 uint16_t resource_len;
 uint8_t kind;  /* 1 number, 2 byte buffer */
 uint8_t method;/* Return(literal), zero arguments */
 uint8_t resource[ACPI816_MAX_RESOURCE];
};
struct acpi816_result {
 uint16_t tables,ssdts,objects,scopes,devices,unsupported,regions_denied,duplicates;
 uint8_t complete,hostbridge_literal_ready,seg_known,bbn_known,crs_known;
 uint16_t hostbridge_segment,hostbridge_bus,crs_length;
 /* Resource bytes are retained only as an inert model output; not a DMA map. */
 struct acpi816_object object[ACPI816_MAX_OBJECTS];
};
/* Returns 1 for valid ACPI table containers; incomplete AML reports complete=0.
 * Failure leaves output byte-identical. No callbacks, hardware access or heap. */
int acpi816_inspect(const struct acpi816_table *tables,size_t count,
                    struct acpi816_result *out);
int acpi816_allow_dma(const struct acpi816_result *out,uint16_t requester);
#endif
