#ifndef STYLEOS_NATIVE_TEA_ELF_H
#define STYLEOS_NATIVE_TEA_ELF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Deliberately narrow ELF64 loader for the one-page, trusted x86-64 CPL3
 * diagnostic. A .tea is checked/extracted by build-time tools. This module
 * consumes the validated ELF bytes, not arbitrary paths or user pointers.
 * SHA-256 is an integrity check, NOT proof of a signed package. */
#define NTE_PAGE 4096u
#define NTE_USER_CODE_VA UINT64_C(0x10000000)

enum nte_status { NTE_OK=0, NTE_ARGUMENT, NTE_FORMAT, NTE_BOUNDS,
                  NTE_RIGHTS, NTE_HASH };
struct nte_loaded { uint64_t entry; uint32_t code_bytes, memory_bytes; };

/* No destination or result write on error. Caller must have separately proven
 * that destination is a writable, reserved, image-owned physical page.
 * Single CPU, privileged kernel call only, no general ELF/ASLR support. */
void nte_sha256(const uint8_t *input, size_t bytes, uint8_t out[32]);

enum nte_status nte_load_x64_page(const uint8_t *elf, size_t elf_bytes,
                                  const uint8_t expected_sha256[32],
                                  uint8_t *page, size_t page_bytes,
                                  struct nte_loaded *loaded);

/* Bounded native image loader for a trusted, immutable ELF buffer. Same
 * single RX PT_LOAD ABI; permit a nonzero entry inside actual file bytes.
 * Destination page and result must be distinct from all input spans.
 * Caller establishes package trust, owned lease, safe root and serialization.
 * This is not a general multi-segment ELF loader. */
enum nte_status nte_load_x64_image(const uint8_t *,size_t,const uint8_t[32],
 uint8_t *,size_t,struct nte_loaded *);

/* Read a bounded ELF through an admitted block reader. Scratch is private
 * kernel memory, may change on failure; destination and metadata may not.
 * Digest and extent must come from trusted policy, NOT from the read medium.
 * No filesystem, signature verification or device admission is implied. */
typedef bool (*nte_sector_read)(void *,uint32_t,uint8_t[512]);
enum nte_status nte_load_x64_reader(nte_sector_read,void *,uint32_t,uint32_t,
 size_t,const uint8_t[32],uint8_t *,size_t,uint8_t *,size_t,struct nte_loaded *);

#endif
