#include <styleos/cache_extent_804.h>
#include <stddef.h>
#include <stdint.h>
/* Reject overflow, unaligned MMIO bases, empty/implausible extents and partial reads. */
bool cache804_plan(uint64_t start,uint64_t bytes,uint64_t max_bytes,struct cache804_span *out){
 if(!out || !bytes || bytes>max_bytes || max_bytes>UINT64_C(0x40000000) ||
    (start&4095u) || start>UINT64_MAX-bytes || bytes>UINT64_MAX-4095u)return false;
 uint64_t pages=(bytes+4095u)/4096u;
 if(!pages || pages>max_bytes/4096u || pages>UINT64_C(65536))return false;
 out->start=start;out->end=start+bytes;out->pages=pages;return true;
}
bool cache804_leaf_end(uint64_t cursor,uint64_t end,uint64_t leaf_bytes,uint64_t *stop){
 if(!stop || cursor>=end || (cursor&4095u) ||
    (leaf_bytes!=4096u && leaf_bytes!=UINT64_C(0x200000) && leaf_bytes!=UINT64_C(0x40000000)))return false;
 uint64_t offset=cursor&(leaf_bytes-1u), remaining=leaf_bytes-offset;
 if(remaining>end-cursor)remaining=end-cursor;
 if(!remaining || remaining>UINT64_MAX-cursor)return false;
 *stop=cursor+remaining;return true;
}
