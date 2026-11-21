#ifndef STYLEOS_NATIVE_TEA_STAGING_H
#define STYLEOS_NATIVE_TEA_STAGING_H

#include <styleos/native_tea_package.h>
#include <styleos/native_tea_elf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Stage 776: bounded, read-only disk -> canonical .tea -> one RX code page.
 * Shared by the dedicated CPL3 QEMU fixture and dormant Desktop code.
 * THIS IS NOT a production loader, signature trust store or memory allocator.
 * All buffers and their physical ownership are provided by a trusted caller.
 * No PID, session, root, IPC port or window is created here. */
enum nts_status {
    NTS_OK=0, NTS_ARGUMENT, NTS_LAYOUT, NTS_IO, NTS_PACKAGE, NTS_ELF
};
struct nts_device {
    bool (*read)(void *context,uint32_t lba,uint8_t out[NTP_SECTOR_BYTES]);
    void *context;
    uint32_t sectors;
};
struct nts_pin {
    const uint8_t *package_sha;
    const uint8_t *elf_sha;
    const uint8_t *manifest;
    size_t manifest_bytes;
    uint32_t package_bytes;
};
struct nts_buffers {
    uint8_t *sector;  /* 512 bytes; must not overlap package/code */
    uint8_t *package; /* NTP_MAX_PACKAGE bytes; must not overlap code */
    uint8_t *code;    /* aligned NTE_PAGE; must be unused until success */
};
struct nts_result {
    struct nte_loaded loaded;
    uint32_t package_bytes, disk_sectors;
};

/* Only a trusted, single-CPU kernel caller may invoke this before publishing
 * a user page. On every refusal, code and result are unchanged; scratch may
 * contain untrusted disk bytes and must never be treated as an executable.
 * Source disks are not written. Integrity pinning is not author authentication.
 * The read callback is trusted and must not be reentrant. */
enum nts_status nts_prepare(const struct nts_device *,const struct nts_pin *,
                            const struct nts_buffers *,struct nts_result *);

#endif
