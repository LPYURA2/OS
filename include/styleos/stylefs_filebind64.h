#ifndef STYLEOS_FILEBIND64_H
#define STYLEOS_FILEBIND64_H
#include <styleos/stylefs_namespace64.h>
#include <styleos/stylefs_stream64.h>
/* Experimental binding of two disjoint grants on the SAME device.
 * Trusted serialized service only. No direct raw mutation while bound.
 * Creates append-only file IDs: durable data first, directory name last.
 * Unreferenced durable data are retained, never automatically reclaimed.
 * No production volume format, ACL, replacement or deletion in this API. */
enum fb64_status { FB64_OK, FB64_INVALID, FB64_STALE, FB64_CORRUPT,
 FB64_CONFLICT, FB64_FULL, FB64_IO, FB64_CANCELLED, FB64_ORPHAN, FB64_UNCERTAIN };
struct fb64_view {
 struct dc64_state *directory;struct ns64_proof *proof;struct sfs64_journal *data;
 uint64_t epoch,generation,dir_token,data_token,device_generation;
 bool valid;
};
struct fb64_workspace {
 struct dc64_workspace writer,reader;
 struct sfs64_stream stream;
 uint8_t buffer[SFS64_WINDOW_BYTES];
};
typedef bool (*fb64_source)(void *,uint64_t,uint8_t *,size_t);
/* All named file IDs must have valid manifests, directory IDs must not.
 * Unreferenced file manifests may remain after an interrupted creation. */
enum fb64_status fb64_bind(struct fb64_view *,struct dc64_state *,struct ns64_proof *,struct sfs64_journal *);
enum fb64_status fb64_stat(struct fb64_view *,const uint8_t *path,size_t length,
 uint8_t scratch[SD64_PAGE],struct sfs64_file *);
enum fb64_status fb64_create(struct fb64_view *,struct fb64_workspace *,
 const struct sd64_record *,uint64_t bytes,fb64_source,void *);
#endif
