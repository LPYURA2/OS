#ifndef STYLEOS_USER_IPC_773_H
#define STYLEOS_USER_IPC_773_H
#include <stddef.h>
#include <stdint.h>
#include <styleos/native_session_hw_770.h>
#include <styleos/native_window_channel.h>
/* Dedicated 773 x86-64 guest experiment. Fixed 16-byte, versioned wire ABI;
 * no raw kernel pointers, general syscall dispatch or production authorization. */
#define IPC773_VERSION 1u
#define IPC773_GET_KEY 1u
#define IPC773_KEY_CODE 0x773u
#define IPC773_USER_DENIED 2u
struct ipc773_request { uint32_t version, operation, window_id, reply_size; };
struct ipc773_reply { uint32_t status, sequence, keycode, kind; };
_Static_assert(sizeof(struct ipc773_request)==16,"IPC request wire size");
_Static_assert(sizeof(struct ipc773_reply)==16,"IPC reply wire size");
struct ipc773_broker { struct nwc_service channel; struct nsh_stage *stage; unsigned returns, denied; };
/* Start before PID admission; attach+arm after registration and real roots. */
int ipc773_start(struct ipc773_broker *,struct nsh_stage *);
int ipc773_arm(struct ipc773_broker *);
/* The x86 trap alone authenticates hardware CS/SS/TSS/CR3 and proves both
 * PTEs map the admitted, image-owned private stack. Caller supplies only
 * kernel-copied request bytes, never an untrusted userspace pointer. */
int ipc773_fetch(struct ipc773_broker *,uint64_t observed_root,
                 const struct ipc773_request *,struct ipc773_reply *);
int ipc773_validate(const struct ipc773_request *,uint32_t expected_window);
#endif
