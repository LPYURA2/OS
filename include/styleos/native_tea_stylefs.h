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
/* Stage 106: durable copy of an externally approved package into the
 * trusted supervisor's provisioned Apps directory on an ID-managed volume.
 * Filename is the full approved package SHA256 + .tea. Presence/name
 * alone NEVER grants approval. Resolve rechecks file CRC and immutable pins.
 * No arbitrary disk trust, no automatic approval, no ambient grants.
 * Trusted serialized caller owns the scratch, registry and output spans.
 * Destination must be a separately authorized directory ID; supervisor
 * supplies explicit approval of this install action. No overwrite.
 * Install success means durable copy, not running process or UI shortcut.
 * Uncertain media outcome requires recovery, not blind retry. */
enum po64_status nts106_install(struct po64_service *,const struct ntr104_registry *,
 uint64_t principal,const struct po64_row *,uint64_t apps_parent,
 uint8_t *,size_t,unsigned *slot);
enum po64_status nts106_resolve(struct po64_service *,const struct ntr104_registry *,
 uint64_t principal,unsigned slot,uint64_t apps_parent,uint8_t *,size_t,struct po64_row *);
#endif
