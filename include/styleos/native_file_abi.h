#ifndef STYLEOS_NATIVE_FILE_ABI_H
#define STYLEOS_NATIVE_FILE_ABI_H
#include <stdint.h>
/* v1 register ABI: RAX op/status, RBX exact grant/request token,
 * RCX private DATA address, RDX size/secondary result.
 * READ submit: address zero, capacity 1..4096.
 * WRITE submit: data length 0..4096, zero address iff empty.
 * TAKE: output address zero iff capacity zero; successful collection
 * returns operation status in low 32 bits, byte count in high 32 bits.
 * All refused calls return zero secondary result. No path authority.
 * No disk I/O in the trap; grants are issued by trusted supervisor only. */
#define NF91_READ UINT64_C(0x130)
#define NF91_WRITE UINT64_C(0x131)
#define NF91_TAKE UINT64_C(0x132)
#define NF91_CANCEL UINT64_C(0x133)
/* RECEIVE has zero RBX/RCX/RDX; returns one supervisor-approved token
 * in RDX on success. END means no pending approval. */
#define NF93_RECEIVE UINT64_C(0x134)
/* Submit and park atomically; same arguments and returned request ID.
 * A successful return means the operation has finished or become stale,
 * not that the operation succeeded. TAKE obtains the actual result. */
#define NF95_READ_WAIT UINT64_C(0x135)
#define NF95_WRITE_WAIT UINT64_C(0x136)
#define NF91_MAX 4096u
#define NF91_UNAVAILABLE 11u
#endif
