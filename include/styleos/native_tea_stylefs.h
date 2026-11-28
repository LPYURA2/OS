#ifndef STYLEOS_NATIVE_TEA_STYLEFS_H
#define STYLEOS_NATIVE_TEA_STYLEFS_H
#include <styleos/native_tea_staging.h>
#include <styleos/native_tea_registry_104.h>
#include <styleos/stylefs_paged_object64.h>
/* Supervisor-only synchronous adapter. Caller authorizes the exact file
 * revision with a read token, and separately approves immutable pins.
 * Scratch is exclusive NTP_MAX_PACKAGE bytes and disjoint from service,
 * pins, and consumer context. No user pointers or reentry. Consumer may
 * use the verified bytes only during its call, must not retain them.
 * The caller owns and must revoke the token. No storage writes. */
typedef bool (*ntsfs_consumer)(void *,const uint8_t *,size_t);
enum ntsfs_status { NTSFS_OK,NTSFS_ARGUMENT,NTSFS_READ,NTSFS_PACKAGE,NTSFS_CONSUMER };
enum ntsfs_status ntsfs_use(struct po64_service *,uint64_t principal,uint64_t token,
 const struct nts_pin *,uint8_t scratch[NTP_MAX_PACKAGE],ntsfs_consumer,void *);
/* Exact file CRC, then external approval registry, then canonical ZIP,
 * manifest and ELF digest checks. Registry is immutable during callback.
 * No filename-based authority and no pointers survive this call.
 * Scratch capacity must cover bytes; used prefix is scrubbed on exit. */
typedef bool (*nts104_consumer)(void *,const uint8_t *,size_t,unsigned);
enum ntsfs_status nts104_use(struct po64_service *,uint64_t,uint64_t,size_t,
 const struct ntr104_registry *,uint8_t *scratch,size_t scratch_capacity,nts104_consumer,void *);
#endif
