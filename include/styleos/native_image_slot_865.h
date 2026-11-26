#ifndef STYLEOS_NATIVE_IMAGE_SLOT_865_H
#define STYLEOS_NATIVE_IMAGE_SLOT_865_H
#include <styleos/native_tea_elf.h>
#include <styleos/physical_arena_844.h>
/* Bounded native launch source. The kernel's trusted caller supplies the
 * admitted read capability, immutable expected digest, extent, and live
 * physical lease. No compiled-in app identity or workload policy. Caller
 * must serialize and audit the detached root, and must never stage into
 * an executing/published process. On error the lease bytes and output
 * are unchanged; scratch may change. No signature trust is implied. */
struct s865_image_source {
 nte_sector_read read;
 void *opaque;
 uint32_t lba,sectors,bytes;
 uint8_t sha256[32];
};
enum nte_status s865_load_owned(const struct s844_arena *,struct s844_lease,
 const struct s865_image_source *,uint8_t *,size_t,struct nte_loaded *);
#endif
