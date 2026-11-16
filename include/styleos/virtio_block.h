#ifndef STYLEOS_VIRTIO_BLOCK_H
#define STYLEOS_VIRTIO_BLOCK_H
#include "styleos/boot_memory.h"

#define VB_PAGE 4096u
#define VB_SECTOR_BYTES 512u
#define VB_QUEUE_SIZE 8u
#define VB_MAX_REQUESTS 8192u
#define VB_POLL_LIMIT 1000000u
#define VB_RESET_LIMIT 10000u
#define VB_DESC_OFFSET 0u
#define VB_AVAIL_OFFSET 256u
#define VB_USED_OFFSET 512u
#define VB_HEADER_OFFSET 0u
#define VB_STATUS_OFFSET 64u
#define VB_DATA_OFFSET 512u
#define VB_REQUIRED_FEATURES ((UINT64_C(1)<<32)|(UINT64_C(1)<<9))

struct vb_mmio { uint64_t base, bytes; };
enum vb_error {
    VB_OK, VB_ARGUMENT, VB_PLATFORM, VB_MEMORY, VB_DEVICE, VB_VERSION,
    VB_FEATURES, VB_READ_ONLY, VB_QUEUE, VB_RESET, VB_CAPACITY, VB_RANGE,
    VB_LIMIT, VB_STATE, VB_TIMEOUT, VB_USED, VB_STATUS, VB_CONFIG
};
union vb_control_storage { uint64_t align; unsigned char bytes[VB_PAGE]; };
extern _Alignas(4096) unsigned char styleos_vblk_queue[VB_PAGE];
extern _Alignas(4096) unsigned char styleos_vblk_request[VB_PAGE];
extern _Alignas(4096) union vb_control_storage styleos_vblk_control;

/* One device, one outstanding sector operation. All DMA addresses refer only
 * to the admitted queue/request pages; callers are copied via the bounce area.
 * Device ranges are the validated FDT virtio,mmio nodes of fixed QEMU virt. */
enum vb_error vb_open(const struct boot_memory_evidence *,
                      const struct vb_mmio *,size_t count);
enum vb_error vb_read(uint64_t sector,void *one_sector);
enum vb_error vb_write(uint64_t sector,const void *one_sector);
enum vb_error vb_flush(void);
uint64_t vb_capacity(void);
uint64_t vb_requests(void);
uint64_t vb_reads(void);
uint64_t vb_writes(void);
uint64_t vb_flushes(void);
uint64_t vb_features(void);
uint64_t vb_offered_features(void);
uint64_t vb_mmio_base(void);
unsigned vb_queue_size(void);
const char *vb_error_name(enum vb_error);

#ifdef STYLEOS_VBLK_HOST_TEST
/* Test adapter only: same feature/queue/request engine, model register I/O and
 * simulated DMA. This bypasses the ARM physical-placement proof, never claims
 * host pointers are physical addresses, and is absent from the guest build. */
struct vb_model_bus {
    void *context;
    uint32_t (*read)(void *,uint32_t);
    void (*write)(void *,uint32_t,uint32_t);
    void (*poll)(void *);
};
enum vb_error vb_model_open(const struct vb_model_bus *);
void vb_model_request_count(uint64_t); /* bounded-counter refusal test */
#endif
#endif
