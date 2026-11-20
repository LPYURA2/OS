#ifndef STYLEOS_STYLEFS_H
#define STYLEOS_STYLEFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* StyleFS v1, experimental. Disk bytes are explicitly encoded by the driver. */
enum {
    SFS_SECTOR_BYTES = 512, SFS_DISK_SECTORS = 16384, SFS_ENTRIES = 64,
    SFS_NAME_MAX = 47, SFS_FILE_MAX = 65536, SFS_ROOT = 0
};
enum sfs_type { SFS_FREE, SFS_DIRECTORY, SFS_FILE };
enum sfs_result {
    SFS_OK, SFS_IO, SFS_SIGNATURE, SFS_CORRUPT, SFS_STATE, SFS_NAME,
    SFS_NOT_FOUND, SFS_EXISTS, SFS_TYPE, SFS_FULL, SFS_RANGE, SFS_NOT_EMPTY
};

struct sfs_io {
    void *context;
    uint32_t sectors;
    bool (*read)(void *, uint32_t, uint8_t[SFS_SECTOR_BYTES]);
    bool (*write)(void *, uint32_t, const uint8_t[SFS_SECTOR_BYTES]);
    bool (*flush)(void *);
};

struct sfs_entry {
    uint32_t size, checksum;
    uint16_t parent;
    uint8_t type, bank;
    char name[SFS_NAME_MAX + 1];
};

struct sfs_volume {
    struct sfs_io io;
    struct sfs_entry entries[SFS_ENTRIES];
    uint32_t generation;
    unsigned active;
    bool mounted;
};

/* Caller supplies storage for a single synchronous transaction, not a stack
 * allocation. No shared global state in the filesystem. */
struct sfs_workspace {
    struct sfs_entry pending[SFS_ENTRIES];
    uint8_t table[SFS_ENTRIES * 64];
    uint8_t sector[SFS_SECTOR_BYTES];
};

/* Mount itself never writes; there is no initialize-on-error path. Omit write
 * and flush for a read-only view. An uncertain I/O failure requires a remount. */
enum sfs_result sfs_mount(struct sfs_volume *, struct sfs_workspace *, const struct sfs_io *);
#ifdef STYLEFS_HOST_FORMAT
/* Explicit formatter, compiled into host utilities only. Refuses nonzero media. */
enum sfs_result sfs_format(struct sfs_volume *, struct sfs_workspace *, const struct sfs_io *);
#endif
const struct sfs_entry *sfs_entry_at(const struct sfs_volume *, unsigned id);
int sfs_find(const struct sfs_volume *, unsigned parent, const char *name);
bool sfs_name_valid(const char *name);
enum sfs_result sfs_mkdir(struct sfs_volume *, struct sfs_workspace *, unsigned parent,
                         const char *name, unsigned *id);
enum sfs_result sfs_create(struct sfs_volume *, struct sfs_workspace *, unsigned parent,
                          const char *name, const uint8_t *data, size_t size, unsigned *id);
enum sfs_result sfs_read(const struct sfs_volume *, unsigned id, uint8_t *data, size_t capacity);
enum sfs_result sfs_write(struct sfs_volume *, struct sfs_workspace *, unsigned id,
                         const uint8_t *data, size_t size);
enum sfs_result sfs_move(struct sfs_volume *, struct sfs_workspace *, unsigned id,
                        unsigned parent, const char *name);
enum sfs_result sfs_copy(struct sfs_volume *, struct sfs_workspace *, unsigned id,
                        unsigned parent, const char *name, unsigned *new_id);
enum sfs_result sfs_remove(struct sfs_volume *, struct sfs_workspace *, unsigned id);
const char *sfs_result_name(enum sfs_result);

#endif
