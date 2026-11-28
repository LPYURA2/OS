#ifndef STYLEOS_NATIVE_TEA_REGISTRY_104_H
#define STYLEOS_NATIVE_TEA_REGISTRY_104_H
#include <styleos/native_tea_staging.h>
/* Boot-lifetime, trusted supervisor registry. An approval is supplied from
 * outside the package (e.g. a reviewed build catalog). Package hashes are
 * integrity pins, NOT signatures. No user syscall registers an approval.
 * Exact manifest/ELF/package pins are copied, no borrowed caller pointers.
 * No automatic update or ambient permissions; installation persistence and
 * publisher verification are separate policies. Zero-initialize once.
 * Current live package bound is 16 KiB; existing loader applies ELF limits. */
enum { NTR104_SLOTS=4,NTR104_NAME=64,NTR104_MANIFEST=4096 };
struct ntr104_record {
 bool used,wait_input;
 uint32_t bytes;
 size_t manifest_bytes;
 uint8_t package_sha[32],elf_sha[32],manifest[NTR104_MANIFEST];
 char title[NTR104_NAME];
};
struct ntr104_registry { struct ntr104_record entries[NTR104_SLOTS]; };
enum ntr104_status { NTR104_OK,NTR104_INVALID,NTR104_DUPLICATE,NTR104_FULL,NTR104_UNAPPROVED,NTR104_PACKAGE };
enum ntr104_status ntr104_register(struct ntr104_registry *,const struct nts_pin *,
 const char *title,bool wait_input,unsigned *slot);
enum ntr104_status ntr104_match(const struct ntr104_registry *,const uint8_t *,size_t,unsigned *);
bool ntr104_pin(const struct ntr104_registry *,unsigned,struct nts_pin *);
#endif
