#define _POSIX_C_SOURCE 200809L
#include <styleos/stylefs.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static struct sfs_volume volume;
static struct sfs_volume old_volume;
static struct sfs_workspace work;
static uint8_t bytes[SFS_FILE_MAX];

static bool rd(void *p, uint32_t lba, uint8_t sector[512])
{ return pread(*(int *)p, sector, 512, (off_t)lba * 512) == 512; }
static bool wr(void *p, uint32_t lba, const uint8_t sector[512])
{ return pwrite(*(int *)p, sector, 512, (off_t)lba * 512) == 512; }
static bool sync_image(void *p) { return fsync(*(int *)p) == 0; }

static bool read_chunk(void *p, uint32_t offset, uint8_t *out, size_t size)
{ return pread(*(int *)p, out, size, offset) == (ssize_t)size; }
static bool write_chunk(void *p, uint32_t offset, const uint8_t *out, size_t size)
{ return pwrite(*(int *)p, out, size, offset) == (ssize_t)size; }

static bool regular(int fd, off_t *size)
{
    struct stat s;
    if (fd < 0 || fstat(fd, &s) < 0 || !S_ISREG(s.st_mode)) return false;
    *size = s.st_size;
    return true;
}

static uint32_t image_capacity(off_t size)
{
    if (size <= 0 || size % SFS_SECTOR_BYTES || size / SFS_SECTOR_BYTES > SFS_V2_MAX_SECTORS) return 0;
    uint32_t sectors = (uint32_t)(size / SFS_SECTOR_BYTES);
    return sfs_capacity_valid(sectors) ? sectors : 0;
}

static uint32_t parse_mib(const char *s)
{
    unsigned n = 0;
    for (; *s; ++s) {
        if (*s < '0' || *s > '9' || n > 1024) return 0;
        n = n * 10 + (unsigned)(*s - '0');
    }
    return n <= 1024 && sfs_capacity_valid(n * 2048) ? n * 2048 : 0;
}

/* Publish only when verified. link fails instead of replacing a path of the
 * same name. A temporary sibling keeps publishing on the same filesystem. */
static int pending_output(const char *path, char **temp)
{
    const char suffix[] = ".stylefs-XXXXXX";
    size_t n = strlen(path);
    if (n > SIZE_MAX - sizeof(suffix)) return -1;
    *temp = malloc(n + sizeof(suffix));
    if (!*temp) return -1;
    memcpy(*temp, path, n);
    memcpy(*temp + n, suffix, sizeof(suffix));
    int fd = mkstemp(*temp);
    if (fd < 0) { free(*temp); *temp = NULL; }
    return fd;
}

static enum sfs_result publish(int fd, char *temp, const char *path, enum sfs_result result)
{
    if (result == SFS_OK && fsync(fd) < 0) result = SFS_IO;
    if (close(fd) < 0 && result == SFS_OK) result = SFS_IO;
    if (result == SFS_OK && link(temp, path) < 0) result = SFS_IO;
    unlink(temp);
    free(temp);
    return result;
}

static int lookup(const char *path)
{
    if (*path++ != '/') return -1;
    unsigned here = SFS_ROOT;
    while (*path) {
        const char *slash = strchr(path, '/');
        size_t n = slash ? (size_t)(slash - path) : strlen(path);
        char name[SFS_NAME_MAX + 1] = {0};
        if (!n || n > SFS_NAME_MAX || (slash && !slash[1])) return -1;
        memcpy(name, path, n);
        int id = sfs_find(&volume, here, name);
        if (id < 0) return -1;
        here = (unsigned)id;
        if (!slash) break;
        path = slash + 1;
    }
    return (int)here;
}

static int target(const char *path, const char **name)
{
    const char *slash = strrchr(path, '/');
    if (path[0] != '/' || !slash || !sfs_name_valid(slash + 1)) return -1;
    *name = slash + 1;
    if (slash == path) return SFS_ROOT;
    char parent[SFS_ENTRIES * (SFS_NAME_MAX + 1) + 1];
    size_t n = (size_t)(slash - path);
    if (n >= sizeof(parent)) return -1;
    memcpy(parent, path, n);
    parent[n] = 0;
    return lookup(parent);
}

static enum sfs_result list(const char *path)
{
    int id = lookup(path);
    const struct sfs_entry *e = id < 0 ? 0 : sfs_entry_at(&volume, (unsigned)id);
    if (!e) return SFS_NOT_FOUND;
    if (e->type != SFS_DIRECTORY) return SFS_TYPE;
    for (unsigned i = 1; i < SFS_ENTRIES; ++i) {
        e = sfs_entry_at(&volume, i);
        if (e && e->parent == (unsigned)id)
            printf("%c\t%u\t%s\n", e->type == SFS_DIRECTORY ? 'd' : 'f', (unsigned)e->size, e->name);
    }
    return ferror(stdout) ? SFS_IO : SFS_OK;
}

static enum sfs_result import_file(const char *file, unsigned parent, const char *name)
{
    int fd = open(file, O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    off_t size;
    enum sfs_result r = SFS_IO;
    if (regular(fd, &size)) {
        if (size < 0 || size > sfs_file_limit(&volume)) r = SFS_RANGE;
        else r = sfs_create_from(&volume, &work, parent, name, (uint32_t)size, read_chunk, &fd, 0);
    }
    if (fd >= 0) close(fd);
    return r;
}

static enum sfs_result export_file(unsigned id, const char *file)
{
    const struct sfs_entry *e = sfs_entry_at(&volume, id);
    if (!e) return SFS_NOT_FOUND;
    if (e->type != SFS_FILE) return SFS_TYPE;
    char *temp = NULL;
    int fd = pending_output(file, &temp);
    if (fd < 0) return SFS_IO;
    enum sfs_result r = sfs_read_to(&volume, id, write_chunk, &fd);
    return publish(fd, temp, file, r);
}

static enum sfs_result make_volume(int *fd, uint32_t sectors)
{
    if (!sectors || ftruncate(*fd, (off_t)sectors * SFS_SECTOR_BYTES) < 0) return SFS_IO;
    const struct sfs_io io = {fd, sectors, rd, wr, sync_image};
    return sfs_format(&volume, &work, &io);
}

static enum sfs_result create_image(const char *to, uint32_t sectors)
{
    static const char *const dirs[] = {"Документы", "Проекты", "Загрузки", "Музыка", "Изображения", "Видео"};
    if (!sectors) return SFS_RANGE;
    char *temp = NULL;
    int fd = pending_output(to, &temp);
    if (fd < 0) return SFS_IO;
    enum sfs_result r = make_volume(&fd, sectors);
    for (unsigned i = 0; i < sizeof(dirs)/sizeof(*dirs) && r == SFS_OK; ++i)
        r = sfs_mkdir(&volume, &work, SFS_ROOT, dirs[i], 0);
    return publish(fd, temp, to, r);
}

static enum sfs_result migrate(const char *from, const char *to, uint32_t sectors)
{
    if (sectors < SFS_V2_MIN_SECTORS) return SFS_RANGE;
    int source = open(from, O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    off_t size;
    enum sfs_result r = SFS_IO;
    if (!regular(source, &size)) goto finish;
    if (image_capacity(size) != SFS_DISK_SECTORS) { r = SFS_SIGNATURE; goto finish; }
    const struct sfs_io old_io = {&source, SFS_DISK_SECTORS, rd, 0, 0};
    r = sfs_mount(&old_volume, &work, &old_io);
    if (r != SFS_OK) goto finish;
    char *temp = NULL;
    int dest = pending_output(to, &temp);
    if (dest < 0) { r = SFS_IO; goto finish; }
    r = make_volume(&dest, sectors);
    unsigned mapped[SFS_V1_ENTRIES] = {0};
    unsigned left = 0;
    for (unsigned i = 1; i < SFS_V1_ENTRIES; ++i) left += !!sfs_entry_at(&old_volume, i);
    while (left && r == SFS_OK) {
        unsigned before = left;
        for (unsigned i = 1; i < SFS_V1_ENTRIES && r == SFS_OK; ++i) {
            const struct sfs_entry *entry = sfs_entry_at(&old_volume, i);
            if (!entry || mapped[i] || (entry->parent && !mapped[entry->parent])) continue;
            unsigned parent = mapped[entry->parent];
            if (entry->type == SFS_DIRECTORY) r = sfs_mkdir(&volume, &work, parent, entry->name, &mapped[i]);
            else {
                r = sfs_read(&old_volume, i, bytes, sizeof(bytes));
                if (r == SFS_OK) r = sfs_create(&volume, &work, parent, entry->name, bytes, entry->size, &mapped[i]);
                if (r == SFS_OK) r = sfs_read_to(&volume, mapped[i], NULL, NULL);
            }
            if (r == SFS_OK) --left;
        }
        if (left == before && r == SFS_OK) r = SFS_CORRUPT;
    }
    r = publish(dest, temp, to, r);
finish:
    if (source >= 0) close(source);
    return r;
}

int main(int argc, char **argv)
{
    if (argc < 3) goto usage;
    const char *cmd = argv[1];
    enum sfs_result r = SFS_IO;
    if (!strcmp(cmd, "new")) {
        if (argc != 3 && argc != 4) goto usage;
        r = create_image(argv[2], argc == 4 ? parse_mib(argv[3]) : SFS_DISK_SECTORS);
        goto report;
    }
    if (!strcmp(cmd, "migrate")) {
        if (argc != 4 && argc != 5) goto usage;
        r = migrate(argv[2], argv[3], argc == 5 ? parse_mib(argv[4]) : 256 * 2048);
        goto report;
    }
    bool info = !strcmp(cmd, "info"), readonly = info || !strcmp(cmd, "list") || !strcmp(cmd, "get");
    bool mkdir = !strcmp(cmd, "mkdir"), put = !strcmp(cmd, "put"), get = !strcmp(cmd, "get");
    bool mv = !strcmp(cmd, "move"), cp = !strcmp(cmd, "copy"), rm = !strcmp(cmd, "remove");
    int expected = info ? 3 : mkdir || rm || !strcmp(cmd, "list") ? 4 : put || get || mv || cp ? 5 : 0;
    if (argc != expected) goto usage;
    int fd = open(argv[2], (readonly ? O_RDONLY : O_RDWR) | O_NOFOLLOW | O_NONBLOCK);
    off_t size;
    if (!regular(fd, &size)) goto done;
    uint32_t sectors = image_capacity(size);
    if (!sectors) { r = SFS_SIGNATURE; goto done; }
    const struct sfs_io io = {&fd, sectors, rd, readonly ? 0 : wr, readonly ? 0 : sync_image};
    r = sfs_mount(&volume, &work, &io);
    if (r != SFS_OK) goto done;
    if (info) {
        printf("StyleFS v%u\ncapacity: %u MiB\nmax file: %u bytes\nmax name: %u bytes\n", volume.format,
               volume.io.sectors / 2048, (unsigned)sfs_file_limit(&volume), sfs_name_limit(&volume));
    } else if (!strcmp(cmd, "list")) r = list(argv[3]);
    else if (mkdir || put) {
        const char *name;
        int parent = target(argv[3], &name);
        r = parent < 0 ? SFS_NOT_FOUND : mkdir ? sfs_mkdir(&volume, &work, (unsigned)parent, name, 0) :
            import_file(argv[4], (unsigned)parent, name);
    } else {
        int id = lookup(argv[3]);
        if (id < 0) r = SFS_NOT_FOUND;
        else if (get) r = export_file((unsigned)id, argv[4]);
        else if (rm) r = sfs_remove(&volume, &work, (unsigned)id);
        else {
            const char *name;
            int parent = target(argv[4], &name);
            r = parent < 0 ? SFS_NOT_FOUND : cp ? sfs_copy(&volume, &work, (unsigned)id, (unsigned)parent, name, 0) :
                sfs_move(&volume, &work, (unsigned)id, (unsigned)parent, name);
        }
    }
done:
    if (fd >= 0 && close(fd) < 0 && r == SFS_OK) r = SFS_IO;
report:
    if (r != SFS_OK) fprintf(stderr, "StyleFS: %s\n", sfs_result_name(r));
    return r == SFS_OK ? 0 : 1;
usage:
    fprintf(stderr, "usage: %s new IMAGE [MiB] | migrate OLD_IMAGE NEW_IMAGE [MiB] | info IMAGE | list/mkdir/remove IMAGE /path | get/put IMAGE /path FILE | move/copy IMAGE /source /target\n", argv[0]);
    return 2;
}
