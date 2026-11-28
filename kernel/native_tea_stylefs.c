#include <styleos/native_tea_stylefs.h>
struct collect {uint8_t *bytes;size_t expected,seen;};
static bool collect_chunk(void *opaque,uint64_t at,const uint8_t *bytes,size_t n){
 struct collect *c=opaque;
 if(!bytes||at!=c->seen||c->seen>c->expected||n>c->expected-c->seen)return false;
 for(size_t i=0;i<n;++i)c->bytes[c->seen+i]=bytes[i];
 c->seen+=n;return true;
}
static void wipe_n105(uint8_t *scratch,size_t bytes){
 volatile uint8_t *p=scratch;
 for(size_t i=0;i<bytes;++i)p[i]=0;
}
enum ntsfs_status ntsfs_use(struct po64_service *s,uint64_t principal,uint64_t token,
 const struct nts_pin *pin,uint8_t scratch[NTP_MAX_PACKAGE],ntsfs_consumer use,void *ctx){
 if(!s||!principal||!token||!pin||!scratch||!use||!pin->package_sha||
    !pin->elf_sha||!pin->manifest||!pin->manifest_bytes||
    pin->package_bytes<22||pin->package_bytes>NTP_MAX_PACKAGE)return NTSFS_ARGUMENT;
 wipe_n105(scratch,NTP_MAX_PACKAGE);
 struct collect c={scratch,pin->package_bytes,0};
 enum ntsfs_status status=NTSFS_READ;
 if(po64_read(s,principal,token,collect_chunk,&c)==PO64_OK&&c.seen==c.expected){
  struct ntp_payload payload={0};
  status=NTSFS_PACKAGE;
  if(ntp_open(scratch,c.seen,pin->package_sha,pin->elf_sha,pin->manifest,
       pin->manifest_bytes,&payload)==NTP_OK)
   status=use(ctx,scratch,c.seen)?NTSFS_OK:NTSFS_CONSUMER;
 }
 wipe_n105(scratch,NTP_MAX_PACKAGE);return status;
}

enum ntsfs_status nts104_use(struct po64_service *s,uint64_t principal,uint64_t token,size_t bytes,
 const struct ntr104_registry *registry,uint8_t *scratch,size_t capacity,nts104_consumer use,void *ctx){
 if(!s||!principal||!token||!registry||!scratch||!use||bytes<22||
    bytes>NTP105_MAX_PACKAGE||bytes>capacity||bytes>UINTPTR_MAX-(uintptr_t)scratch)
  return NTSFS_ARGUMENT;
 wipe_n105(scratch,bytes);
 struct collect c={scratch,bytes,0};enum ntsfs_status status=NTSFS_READ;
 if(po64_read(s,principal,token,collect_chunk,&c)==PO64_OK&&c.seen==c.expected){
  unsigned slot=0;status=NTSFS_PACKAGE;
  if(ntr104_match(registry,scratch,c.seen,&slot)==NTR104_OK)
   status=use(ctx,scratch,c.seen,slot)?NTSFS_OK:NTSFS_CONSUMER;
 }
 wipe_n105(scratch,bytes);return status;
}

static bool distinct106(const void *a,size_t n,const void *b,size_t m){
 return a&&b&&n&&m&&n<=UINTPTR_MAX-(uintptr_t)a&&m<=UINTPTR_MAX-(uintptr_t)b&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static bool external106(const struct po64_service *s,const void *p,size_t n){
 return distinct106(s,sizeof *s,p,n)&&distinct106(s->block,sizeof *s->block,p,n)&&
 distinct106(s->work,sizeof *s->work,p,n)&&
 distinct106(s->nodes,s->node_cap*sizeof *s->nodes,p,n)&&
 distinct106(s->runs,s->run_cap*sizeof *s->runs,p,n)&&
 (!s->previous||distinct106(s->previous,s->run_cap*sizeof *s->previous,p,n));
}
static bool key106(const struct ntr104_registry *r,unsigned slot,uint64_t parent,struct sd64_key *key){
 static const char hex[]="0123456789abcdef";
 if(!r||slot>=NTR104_SLOTS||!r->entries[slot].used||parent<=1)return false;
 *key=(struct sd64_key){.parent=parent,.length=68};
 for(unsigned i=0;i<32;++i){
  unsigned v=r->entries[slot].package_sha[i];key->name[2*i]=hex[v>>4];key->name[2*i+1]=hex[v&15];
 }
 key->name[64]='.';key->name[65]='t';key->name[66]='e';key->name[67]='a';return true;
}
struct verify106 {unsigned slot;};
static bool verify106_use(void *ctx,const uint8_t *p,size_t n,unsigned slot){
 (void)p;(void)n;((struct verify106 *)ctx)->slot=slot;return true;
}
static enum po64_status verify106_row(struct po64_service *s,const struct ntr104_registry *r,
 uint64_t principal,const struct po64_row *row,uint8_t *scratch,size_t cap,unsigned *slot){
 if(row->entry.kind!=SD64_FILE||row->bytes<22||row->bytes>NTP105_MAX_PACKAGE||row->bytes>cap)
  return PO64_INVALID;
 uint64_t token=0;enum po64_status rc=po64_grant(s,principal,&row->entry.key,row->entry.target,row->revision,&token);
 if(rc!=PO64_OK)return rc;
 struct verify106 result={NTR104_SLOTS};
 enum ntsfs_status st=nts104_use(s,principal,token,(size_t)row->bytes,r,scratch,cap,verify106_use,&result);
 if(!s->active)return PO64_CORRUPT;
 rc=po64_revoke(s,principal,token);if(rc!=PO64_OK)return rc;
 if(st!=NTSFS_OK)return st==NTSFS_PACKAGE?PO64_DENIED:PO64_IO;
 *slot=result.slot;return PO64_OK;
}
static enum po64_status find106(struct po64_service *s,struct po64_snapshot snap,
 const struct sd64_key *key,struct po64_row *row){
 for(uint64_t i=0;i<s->node_cap;++i){
  enum po64_status rc=po64_child_at(s,snap,key->parent,i,row);
  if(rc!=PO64_OK)return rc;
  int cmp=sd64_compare(&row->entry.key,key);
  if(!cmp)return PO64_OK;
  if(cmp>0)return PO64_END;
 }
 return PO64_END;
}
static bool args106(struct po64_service *s,const struct ntr104_registry *r,uint64_t principal,
 uint64_t parent,uint8_t *scratch,size_t cap,const void *out,size_t out_bytes){
 return s&&r&&principal&&parent>1&&scratch&&cap&&cap<=NTP105_MAX_PACKAGE&&
 external106(s,r,sizeof *r)&&external106(s,scratch,cap)&&external106(s,out,out_bytes)&&
 distinct106(r,sizeof *r,scratch,cap)&&distinct106(r,sizeof *r,out,out_bytes)&&
 distinct106(scratch,cap,out,out_bytes);
}
enum po64_status nts106_install(struct po64_service *s,const struct ntr104_registry *r,
 uint64_t principal,const struct po64_row *source,uint64_t parent,uint8_t *scratch,size_t cap,unsigned *slot){
 if(!s||!source)return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!s->active)return PO64_STALE;
 if(!s->writable||!s->pair.id_floor)return PO64_DENIED;
 if(!args106(s,r,principal,parent,scratch,cap,slot,sizeof *slot)||
    !external106(s,source,sizeof *source)||!distinct106(source,sizeof *source,scratch,cap)||
    !distinct106(source,sizeof *source,slot,sizeof *slot)||!distinct106(source,sizeof *source,r,sizeof *r)||
    source->entry.key.parent==parent)return PO64_INVALID;
 struct po64_snapshot snap;enum po64_status rc=po64_snapshot(s,&snap);if(rc!=PO64_OK)return rc;
 unsigned matched=NTR104_SLOTS;
 rc=verify106_row(s,r,principal,source,scratch,cap,&matched);if(rc!=PO64_OK)return rc;
 struct sd64_key dest;
 if(!key106(r,matched,parent,&dest))return PO64_INVALID;
 struct po64_row exists;
 rc=find106(s,snap,&dest,&exists);
 if(rc==PO64_OK)return PO64_DENIED;
 if(rc!=PO64_END)return rc;
 /* Exact snapshot and source identity rechecked by atomic copy. */
 rc=po64_copy_file(s,snap,source,&dest,0);
 if(rc==PO64_OK)*slot=matched;
 return rc;
}
enum po64_status nts106_resolve(struct po64_service *s,const struct ntr104_registry *r,
 uint64_t principal,unsigned slot,uint64_t parent,uint8_t *scratch,size_t cap,struct po64_row *out){
 if(!s)return PO64_INVALID;
 if(s->busy)return PO64_BUSY;
 if(!s->active)return PO64_STALE;
 if(!args106(s,r,principal,parent,scratch,cap,out,sizeof *out))return PO64_INVALID;
 struct sd64_key key;
 if(!key106(r,slot,parent,&key))return PO64_DENIED;
 struct po64_snapshot snap;enum po64_status rc=po64_snapshot(s,&snap);
 if(rc!=PO64_OK)return rc;
 struct po64_row row;
 rc=find106(s,snap,&key,&row);if(rc!=PO64_OK)return rc;
 unsigned matched=NTR104_SLOTS;
 rc=verify106_row(s,r,principal,&row,scratch,cap,&matched);
 if(rc!=PO64_OK)return rc;
 if(matched!=slot)return PO64_DENIED;
 *out=row;return PO64_OK;
}
