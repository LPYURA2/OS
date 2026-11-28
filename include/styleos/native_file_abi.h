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
/* Streaming: OPEN uses grant in RBX, RCX=0, RDX=0 (read) or total
 * bytes (write); returns direction-specific stream ID in RDX.
 * TAKE/FEED use ID, private DATA packet [LE64 offset][payload], RDX
 * packet length 9..4104. TAKE writes only payload and returns count.
 * FINISH/CANCEL/RESULT use ID, zero RCX/RDX; RESULT returns actual
 * terminal status in RDX (including integrity or durable commit).
 * Read payload is provisional until successful RESULT. No disk in trap. */
#define NF102_ROPEN UINT64_C(0x137)
#define NF102_WOPEN UINT64_C(0x138)
#define NF102_RTAKE UINT64_C(0x139)
#define NF102_WFEED UINT64_C(0x13a)
#define NF102_WFINISH UINT64_C(0x13b)
#define NF102_RCANCEL UINT64_C(0x13c)
#define NF102_WCANCEL UINT64_C(0x13d)
#define NF102_RRESULT UINT64_C(0x13e)
#define NF102_WRESULT UINT64_C(0x13f)
#define NF102_HEADER 8u
#define NF91_MAX 4096u
#define NF102_PACKET_MAX (NF102_HEADER+NF91_MAX)
#define NF91_UNAVAILABLE 11u
/* Directory submit: RBX ordinal, RCX/RDX zero. Take: RBX request,
 * private DATA output of 296 bytes for row, zero span for navigation. */
#define EX113_ROW UINT64_C(0x140)
#define EX113_ENTER UINT64_C(0x141)
#define EX113_UP UINT64_C(0x142)
#define EX113_TAKE UINT64_C(0x143)
#define EX117_VOLUME UINT64_C(0x144) /* ordinal -> request; TAKE expects 64-byte record */
#endif
