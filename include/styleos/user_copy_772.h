#ifndef STYLEOS_USER_COPY_772_H
#define STYLEOS_USER_COPY_772_H
#include <stddef.h>
#include <stdint.h>
/* 772: dedicated single-CPU CPL3 trap experiment. These helpers do not
 * authenticate a syscall caller or walk page tables; the privileged x86
 * adapter must do that, using the actual CR3 and immutable image-owned PTEs.
 * Never pass a raw CPL3 pointer as the backing page. */
enum { UC772_PAGE = 4096, UC772_MAX_COPY = 32 };
enum uc772_status { UC772_OK = 0, UC772_BAD, UC772_ROOT, UC772_RANGE };
/* The output offset is unchanged on any error. All arithmetic is checked. */
enum uc772_status uc772_plan(uint64_t observed_cr3, uint64_t verified_cr3,
                             uint64_t known_user_page, uint64_t user_pointer,
                             size_t count, size_t *offset);
/* The adapter supplies only a kernel-image-backed page after verifying the
 * actual user leaf's U/S, R/W, NX and physical page identity. */
enum uc772_status uc772_copy_page(void *kernel_out, size_t capacity,
                                  const void *verified_kernel_page,
                                  size_t offset, size_t count);
#endif
