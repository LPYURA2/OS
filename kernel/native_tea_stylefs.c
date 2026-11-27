#include <styleos/native_tea_stylefs.h>
struct collect {uint8_t *bytes;size_t expected,seen;};
static bool collect_chunk(void *opaque,uint64_t at,const uint8_t *bytes,size_t n){
 struct collect *c=opaque;
 if(!bytes||at!=c->seen||c->seen>c->expected||n>c->expected-c->seen)return false;
 for(size_t i=0;i<n;++i)c->bytes[c->seen+i]=bytes[i];
 c->seen+=n;return true;
}
static void wipe(uint8_t *scratch){
 volatile uint8_t *p=scratch;
 for(size_t i=0;i<NTP_MAX_PACKAGE;++i)p[i]=0;
}
enum ntsfs_status ntsfs_use(struct po64_service *s,uint64_t principal,uint64_t token,
 const struct nts_pin *pin,uint8_t scratch[NTP_MAX_PACKAGE],ntsfs_consumer use,void *ctx){
 if(!s||!principal||!token||!pin||!scratch||!use||!pin->package_sha||
    !pin->elf_sha||!pin->manifest||!pin->manifest_bytes||
    pin->package_bytes<22||pin->package_bytes>NTP_MAX_PACKAGE)return NTSFS_ARGUMENT;
 wipe(scratch);
 struct collect c={scratch,pin->package_bytes,0};
 enum ntsfs_status status=NTSFS_READ;
 if(po64_read(s,principal,token,collect_chunk,&c)==PO64_OK&&c.seen==c.expected){
  struct ntp_payload payload={0};
  status=NTSFS_PACKAGE;
  if(ntp_open(scratch,c.seen,pin->package_sha,pin->elf_sha,pin->manifest,
       pin->manifest_bytes,&payload)==NTP_OK)
   status=use(ctx,scratch,c.seen)?NTSFS_OK:NTSFS_CONSUMER;
 }
 wipe(scratch);return status;
}
