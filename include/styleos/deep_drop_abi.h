#ifndef STYLEOS_DEEP_DROP_ABI_H
#define STYLEOS_DEEP_DROP_ABI_H
/* x86-64 INT 0x80. RBX is the gesture or transfer id. RCX/RDX are
 * private data address and length for OFFER/READ, zero otherwise.
 * RAX returns dd_status. RDX returns the offered id or the byte count
 * only on success, zero on all other results. No caller-supplied identity. */
#define DD_CALL_OFFER  0x110
#define DD_CALL_ACCEPT 0x111
#define DD_CALL_READ   0x112
#define DD_CALL_FINISH 0x113
#define DD_CALL_CANCEL 0x114
/* NWC input wire kind 4. Code selects the message, bytes[0..7] are
 * the nonzero little-endian id. No text or kernel addresses in the queue. */
#define DD_EVENT_KIND 4
#define DD_REQUEST_TEXT 1
#define DD_TEXT_OFFER 2
#define DD_TEXT_LIMIT 1024
#endif
