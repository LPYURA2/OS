#ifndef STYLEOS_SYSTEM_UPDATE_H
#define STYLEOS_SYSTEM_UPDATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { UP_SECTOR = 512, UP_DISK_SECTORS = 139264, UP_REGION = 133120,
       UP_JOURNAL_A = UP_REGION + 1, UP_JOURNAL_B = UP_REGION + 2,
       UP_SLOT_A = UP_REGION + 8, UP_SLOT_SECTORS = 2048,
       UP_SLOT_B = UP_SLOT_A + UP_SLOT_SECTORS, UP_BLOB_MAX = UP_SECTOR * UP_SLOT_SECTORS };

struct upd_io {
    void *context;
    uint32_t sectors;
    bool (*read)(void *, uint32_t, uint8_t[UP_SECTOR]);
    bool (*write)(void *, uint32_t, const uint8_t[UP_SECTOR]);
    bool (*flush)(void *);
};
struct upd_state {
    uint32_t generation, active_slot, active_build, pending_slot, pending_build, tried;
};
struct upd_disk {
    struct upd_io io;
    struct upd_state state;
    unsigned journal;
    bool valid;
};
struct upd_package { uint32_t build, bytes; };
enum upd_result { UP_OK, UP_DISK, UP_IO, UP_FORMAT, UP_AUTH, UP_OLD, UP_BUSY };
bool upd_open(struct upd_disk *, const struct upd_io *);
enum upd_result upd_check(const uint8_t *, size_t, const uint8_t[32], struct upd_package *);
enum upd_result upd_load(struct upd_disk *, unsigned slot, uint8_t[UP_BLOB_MAX],
                         const uint8_t[32], struct upd_package *);
enum upd_result upd_stage(struct upd_disk *, uint8_t[UP_BLOB_MAX], size_t, uint32_t running,
                          const uint8_t[32], struct upd_package *);
bool upd_trial(struct upd_disk *);
bool upd_rollback(struct upd_disk *);
bool upd_accept(struct upd_disk *, unsigned slot, uint32_t build);

/* The only writable device adapter for this guest profile is primary IDE master. */
bool desk_update_io(struct upd_io *);
extern const uint8_t styleos_update_key[32];
uint8_t *desk_update_buffer(void);
bool desk_update_probe(void);
bool desk_update_pending(void);
const char *desk_update_status(void);
void desk_update_mark_ready(void);
enum upd_result desk_update_stage(size_t bytes);

#endif
