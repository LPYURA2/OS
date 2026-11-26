#include <styleos/stylefs_inodetable64.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { FILES=100000, PER=48, PAGES=(FILES+PER-1)/PER };
static uint8_t *disk;
static uint64_t reads,fail=UINT64_MAX;
static bool rd(void *ctx,uint64_t lba,uint8_t *p){
 (void)ctx;++reads;if(lba>=PAGES*8u||lba==fail)return false;
 memcpy(p,disk+lba*512,512);return true;
}
static struct it64_geometry g={9,0,PAGES,UINT64_C(8589934592),32};
static uint64_t id(unsigned i){return UINT64_C(0x100000000)+i*2u;}
static uint64_t sector(unsigned i){return UINT64_C(0x100010000)+i;}
static void make_page(uint8_t *p,unsigned ordinal,unsigned first,unsigned count){
 uint8_t maps[PER][80];struct it64_blob blobs[PER];
 for(unsigned i=0;i<count;i++){
  unsigned index=first+i;struct sfs64_file file={id(index),3,511,index+1};
  struct sfs64_run run={sector(index),1};
  assert(sfs64_map_encode(maps[i],80,file,g.capacity,g.data_start,&run,1));
  blobs[i]=(struct it64_blob){maps[i],80};
 }
 assert(it64_encode(p,&g,ordinal,blobs,count));
}
int main(void){
 disk=calloc(PAGES,IT64_PAGE);assert(disk);
 for(unsigned i=0;i<PAGES;i++){
  unsigned n=FILES-i*PER<PER?FILES-i*PER:PER;
  make_page(disk+(size_t)i*IT64_PAGE,i,i*PER,n);
 }
 struct bs_service b={0};struct bs_backend be={0,1,PAGES*8u,false,false,rd,0,0};
 uint64_t gen,token;
 assert(bs_attach(&b,&be,&gen)==BS_OK&&bs_grant_range(&b,gen,7,0,PAGES*8u,BS_READ,&token)==BS_OK);
 struct it64_state s;uint8_t scratch[IT64_PAGE];struct sfs64_map m={0};
 assert(it64_mount(&s,&b,7,token,&g,scratch)==IT64_OK&&s.count==FILES&&reads==PAGES*8u);
 for(unsigned i=0;i<FILES;i+=127){
  uint64_t before=reads;
  assert(it64_lookup(&s,9,id(i),scratch,&m)==IT64_OK&&m.file.id==id(i)&&m.file.bytes==511&&m.file.revision==3);
  assert(reads-before<=12*8u);
  struct sfs_extent64 e;uint64_t local;size_t n;
  assert(sfs64_map_chunk(&m,0,511,&e,&local,&n)&&e.first==sector(i)&&local==0&&n==511);
 }
 assert(it64_lookup(&s,9,id(FILES-1),scratch,&m)==IT64_OK);
 uint64_t before=reads;assert(it64_lookup(&s,8,id(0),scratch,&m)==IT64_STALE&&reads==before);
 assert(it64_lookup(&s,9,id(1)+1,scratch,&m)==IT64_MISSING&&m.file.id==id(FILES-1));
 /* Cross-page order corruption with individually valid pages. */
 uint8_t saved[IT64_PAGE];memcpy(saved,disk+IT64_PAGE,IT64_PAGE);
 make_page(disk+IT64_PAGE,1,0,PER);
 assert(it64_mount(&s,&b,7,token,&g,scratch)==IT64_CORRUPT&&!s.valid);
 memcpy(disk+IT64_PAGE,saved,IT64_PAGE);
 assert(it64_mount(&s,&b,7,token,&g,scratch)==IT64_OK);
 unsigned middle=PAGES/2;disk[(size_t)middle*IT64_PAGE+100]^=1;
 assert(it64_lookup(&s,9,id(middle*PER),scratch,&m)==IT64_CORRUPT&&!s.valid);
 disk[(size_t)middle*IT64_PAGE+100]^=1;
 assert(it64_mount(&s,&b,7,token,&g,scratch)==IT64_OK);
 fail=(uint64_t)middle*8+3;
 assert(it64_lookup(&s,9,id(middle*PER),scratch,&m)==IT64_IO&&!s.valid);
 assert(b.devices[0].recovery);
 free(disk);
 printf("STYLEFS 30 INODE TABLE PASS: %u manifests, %u pages, <=12 page reads per lookup, high LBA, cross-page order and I/O refusal\n",FILES,PAGES);
 return 0;
}
