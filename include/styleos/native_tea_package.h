#ifndef STYLEOS_NATIVE_TEA_PACKAGE_H
#define STYLEOS_NATIVE_TEA_PACKAGE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Strict, fixed-schema, read-only canonical stored ZIP parser for a pinned
 * x86_64 .tea test fixture. NOT a general package manager or trust store. */
#define NTP_MAX_PACKAGE 16384u
#define NTP_SECTOR_BYTES 512u
#define NTP_DISK_MAGIC "STEA769\0"

enum ntp_status { NTP_OK=0, NTP_ARGUMENT, NTP_FORMAT, NTP_BOUNDS,
                  NTP_HASH, NTP_IO };
struct ntp_payload { const uint8_t *elf; size_t elf_bytes; };
/* Both expected fingerprints come from the separately approved test package.
 * On any refusal neither payload nor its destination is modified. */
enum ntp_status ntp_open(const uint8_t *package, size_t length,
                         const uint8_t expected_package_sha[32],
                         const uint8_t expected_elf_sha[32],
                         const uint8_t *expected_manifest, size_t manifest_length,
                         struct ntp_payload *result);
/* Sector zero: marker[8], version LE32, byte length LE32, sha256[32], zero[464]. */
enum ntp_status ntp_disk_header(const uint8_t header[NTP_SECTOR_BYTES],
                                const uint8_t expected_sha[32],
                                uint32_t *package_bytes);
/* Synchronous bounded reader of an already validated immutable ELF view.
 * Context is the caller-owned ntp_payload, valid only for the launch call.
 * Never exposes ZIP bytes outside the ELF extent; final sector zero padded. */
bool ntp_elf_sector(void *context,uint32_t lba,uint8_t out[NTP_SECTOR_BYTES]);
#endif
