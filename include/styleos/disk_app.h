#ifndef STYLEOS_DISK_APP_H
#define STYLEOS_DISK_APP_H
#include <stddef.h>
#include <stdint.h>

/* T-P1.8B fixture only, not a production executable format or ABI.
 * Explicit little endian: magic[8], version u32@8, ISA u16@12, header u16@14,
 * code length u32@16, entry offset u32@20, reserved u32@24, CRC32 u32@28.
 * CRC covers exactly the file, with bytes [28,32) replaced by zero.
 * The header never supplies a VA/PA. The architecture owns all mappings. */
#define DA_HEADER_BYTES 32u
#define DA_FILE_MAX 1024u
#define DA_CODE_MAX (DA_FILE_MAX-DA_HEADER_BYTES)
#define DA_CODE_BYTES 28u
#define DA_ISA_AARCH64 183u
#define DA_FAULT_VA UINT64_C(0x12010000)

struct da_info {
    uint32_t code_length, entry_offset, fault_offset, exit_offset;
    uint32_t marker, exit_code, checksum;
};
enum da_result {
    DA_OK, DA_ARGUMENT, DA_SIZE, DA_MAGIC, DA_VERSION, DA_ISA, DA_HEADER,
    DA_ENTRY, DA_CHECKSUM, DA_OPCODE, DA_CAPACITY, DA_OVERLAP
};
/* Complete validation precedes any output write. The closed seven-instruction
 * grammar permits marker/exit immediates only; all code bytes are decoded.
 * X0=0 faults at the unique LDR; X0=1 sets X19 and exits through SVC #2.
 * No FP/SIMD, indirect branch, privileged instruction or executable padding.
 * Source must remain stable during the call; no concurrent DMA or writers. */
enum da_result da_validate(const void *, size_t, struct da_info *);
/* Copy only validated code, with no partial copy or info publication on error.
 * Destination is a caller-admitted static buffer, 4-byte aligned. This helper
 * does not grant execute rights or perform the architecture's I-cache barrier. */
enum da_result da_copy(const void *, size_t, void *, size_t, struct da_info *);
const char *da_result_name(enum da_result);
#endif
