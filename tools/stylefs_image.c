#define _POSIX_C_SOURCE 200809L
#include <styleos/stylefs.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static struct sfs_volume volume;
static struct sfs_workspace work;
static uint8_t bytes[SFS_FILE_MAX];

static bool rd(void *p, uint32_t lba, uint8_t sector[512])
{ return pread(*(int *)p, sector, 512, (off_t)lba * 512) == 512; }
static bool wr(void *p, uint32_t lba, const uint8_t sector[512])
{ return pwrite(*(int *)p, sector, 512, (off_t)lba * 512) == 512; }
static bool sync_image(void *p) { return fsync(*(int *)p) == 0; }

static bool regular(int fd, off_t *size)
{
    struct stat s;
    if (fd < 0 || fstat(fd, &s) < 0 || !S_ISREG(s.st_mode)) return false;
    *size = s.st_size;
    return true;
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
        if (size < 0 || size > SFS_FILE_MAX) r = SFS_RANGE;
        else if (pread(fd, bytes, (size_t)size, 0) == size)
            r = sfs_create(&volume, &work, parent, name, bytes, (size_t)size, 0);
    }
    if (fd >= 0) close(fd);
    return r;
}

static enum sfs_result export_file(unsigned id, const char *file)
{
    const struct sfs_entry *e = sfs_entry_at(&volume, id);
    if (!e) return SFS_NOT_FOUND;
    enum sfs_result r = sfs_read(&volume, id, bytes, sizeof(bytes));
    if (r != SFS_OK) return r;
    int fd = open(file, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0666);
    if (fd < 0) return SFS_IO;
    bool ok = write(fd, bytes, e->size) == (ssize_t)e->size && fsync(fd) == 0;
    if (close(fd) < 0 || !ok) return SFS_IO;
    return SFS_OK;
}

int main(int argc, char **argv)
{
    if (argc < 3) goto usage;
    const char *cmd = argv[1];
    bool create = !strcmp(cmd, "new"), readonly = !strcmp(cmd, "list") || !strcmp(cmd, "get");
    bool mkdir = !strcmp(cmd, "mkdir"), put = !strcmp(cmd, "put"), get = !strcmp(cmd, "get");
    bool mv = !strcmp(cmd, "move"), cp = !strcmp(cmd, "copy"), rm = !strcmp(cmd, "remove");
    int expected = create ? 3 : mkdir || rm || !strcmp(cmd, "list") ? 4 : put || get || mv || cp ? 5 : 0;
    if (argc != expected) goto usage;
    int fd = open(argv[2], (readonly ? O_RDONLY : O_RDWR) | O_NOFOLLOW | O_NONBLOCK |
                  (create ? O_CREAT | O_EXCL : 0), 0666);
    off_t size, capacity = (off_t)SFS_DISK_SECTORS * SFS_SECTOR_BYTES;
    enum sfs_result r = SFS_IO;
    if (!regular(fd, &size)) goto done;
    if (create) { if (ftruncate(fd, capacity) < 0) goto done; }
    else if (size != capacity) { r = SFS_SIGNATURE; goto done; }
    const struct sfs_io io = {&fd, SFS_DISK_SECTORS, rd, readonly ? 0 : wr, readonly ? 0 : sync_image};
    r = create ? sfs_format(&volume, &work, &io) : sfs_mount(&volume, &work, &io);
    if (r != SFS_OK) goto done;
    if (create) {
        static const char *const dirs[] = {"Документы", "Проекты", "Загрузки", "Музыка", "Изображения", "Видео"};
        for (unsigned i = 0; i < sizeof(dirs)/sizeof(*dirs) && r == SFS_OK; ++i)
            r = sfs_mkdir(&volume, &work, SFS_ROOT, dirs[i], 0);
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
    if (r != SFS_OK) fprintf(stderr, "StyleFS: %s\n", sfs_result_name(r));
    return r == SFS_OK ? 0 : 1;
usage:
    fprintf(stderr, "usage: %s new IMAGE | list/mkdir/remove IMAGE /path | get/put IMAGE /path FILE | move/copy IMAGE /source /target\n", argv[0]);
    return 2;
}
