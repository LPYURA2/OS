#include <styleos/stylefs_ownership64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { N=2+16*32,FILES=1000,SLOTS=1100 };
#define CAP UINT64_C(8589934592)
#define HIGH UINT64_C(4294967296)
static uint8_t disk[N][512];
static struct bs_service b;
static struct ic64_state s;
static struct ic64_workspace w;
static struct ow64_extent entries[2][SLOTS];
static struct sfs64_run runs[2][SLOTS];
static struct ow64_inventory inv[2]={{entries[0],runs[0],SLOTS,0},{entries[1],runs[1],SLOTS,0}};
static struct ow64_result result;
static uint64_t gen,token,writes,reads,fail=UINT64_MAX;
static bool rd(void *ctx,uint64_t at,uint8_t *p){(void)ctx;++reads;if(at>=N||at==fail)return false;memcpy(p,disk[at],512);return true;}
static bool wr(void *ctx,uint64_t at,const uint8_t *p){(void)ctx;++writes;if(at>=N)return false;memcpy(disk[at],p,512);return true;}
static bool fl(void *ctx){(void)ctx;return true;}
static bool next(void *ctx,uint64_t i,uint8_t *out,size_t limit,size_t *n){
 unsigned mode=*(unsigned *)ctx;
 struct sfs64_file file={i+2,1,512,7};
 uint64_t at=HIGH+2*i;
 if(mode==1&&i==0){file.revision=2;at=HIGH+2*FILES+10;}
 if(mode==2&&i==1)at=HIGH;
 if(mode==3&&i==0){file.revision=3;at=HIGH+2*FILES+10;}
 struct sfs64_run run={at,1};
 assert(limit>=80);*n=80;return sfs64_map_encode(out,*n,file,CAP,32,&run,1);
}
static void publish(unsigned mode){assert(ic64_publish(&s,&w,FILES,next,&mode)==IC64_OK);}
int main(void){
 struct bs_backend be={0,1,N,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK&&bs_grant_range(&b,gen,7,0,N,BS_READ|BS_WRITE,&token)==BS_OK);
 assert(ic64_provision(&b,7,token,N,CAP,32)==IC64_OK);
 assert(ic64_mount(&s,&b,7,token,N,CAP,32,&w)==IC64_OK);
 publish(0);struct ic64_state stale=s;publish(1);
 uint64_t before=writes;
 assert(ow64_collect(&s,&w,inv,&result)==OW64_OK&&result.valid&&writes==before);
 assert(inv[0].count==FILES&&inv[1].count==FILES&&result.space.free_sectors==CAP-32-FILES-1);
 struct sfs64_inventory a={inv[0].runs,inv[0].count},c={inv[1].runs,inv[1].count};
 struct sfs64_run plan[8];size_t count=0;
 assert(sfs64_allocate(CAP,32,a,c,UINT64_C(100)*1024*1024*1024,plan,8,&count)==SFS64_ALLOC_OK&&count==1&&plan[0].first==32&&plan[0].sectors==209715200);
 assert(ow64_collect(&stale,&w,inv,&result)==OW64_STALE&&!result.valid);
 inv[0].slots=10;assert(ow64_collect(&s,&w,inv,&result)==OW64_OUTPUT&&!result.valid);inv[0].slots=SLOTS;
 /* Mode 3 must actually overlap mode 1's retained revised extent.
  * The earlier test incorrectly used the old, now unretained sector. */
 publish(3);assert(ow64_collect(&s,&w,inv,&result)==OW64_CORRUPT&&!result.valid);
 publish(2);assert(ow64_collect(&s,&w,inv,&result)==OW64_CORRUPT&&!result.valid);
 publish(0);publish(1);assert(ow64_collect(&s,&w,inv,&result)==OW64_OK);
 unsigned old=s.active^1u;disk[old][0]^=1;
 before=writes;assert(ow64_collect(&s,&w,inv,&result)==OW64_CORRUPT&&!result.valid&&writes==before);
 disk[old][0]^=1;fail=2;
 assert(ow64_collect(&s,&w,inv,&result)==OW64_IO&&!result.valid);
 assert(b.devices[0].recovery);
 puts("STYLEFS 32 OWNERSHIP PASS: 2000 retained extents, 100GiB allocation plan, high LBA, alias and stale/corrupt refusal");
 return 0;
}
