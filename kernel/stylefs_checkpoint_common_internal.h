#ifndef STYLEOS_CHECKPOINT_COMMON_INTERNAL_H
#define STYLEOS_CHECKPOINT_COMMON_INTERNAL_H
#include <styleos/block_service.h>
static bool equal(const uint8_t *a,const uint8_t *b,size_t n){for(size_t i=0;i<n;i++)if(a[i]!=b[i])return false;return true;}
static bool zero(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return false;return true;}
static uint64_t get(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t crc(const uint8_t *p,size_t n){uint32_t c=UINT32_MAX;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned k=0;k<8;k++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}return ~c;}
static bool sep(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static const struct bs_grant *grant(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t len,bool write){
 if(!b||b->busy||!owner||!token||!len)return 0;
 for(unsigned i=0;i<BS_GRANTS;i++){
  const struct bs_grant *g=&b->grants[i];
  if(g->token!=token)continue;
  if(g->owner!=owner||g->length!=len||g->device>=BS_DEVICES||
     g->rights!=(write?(BS_READ|BS_WRITE):BS_READ))return 0;
  const struct bs_device *d=&b->devices[g->device];
  if(!g->generation||d->generation!=g->generation||d->recovery||g->start>=d->backend.sectors||
     len>d->backend.sectors-g->start||(write&&(!d->backend.writable||d->backend.system_media)))return 0;
  return g;
 }return 0;
}
static bool overlap(const struct bs_grant *a,const struct bs_grant *b){
 return a->start<b->start+b->length&&b->start<a->start+a->length;
}
#endif
