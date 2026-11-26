#include <styleos/stylefs_inodestream64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { META=258,DATA=1024,TOTAL=META+DATA };
static uint8_t disk[TOTAL][512];
static struct bs_service b;
static struct ic64_state s;
static struct is64_workspace w;
static struct ow64_extent entries[2][100];
static struct sfs64_run runs[2][100];
static struct ow64_inventory inv[2]={{entries[0],runs[0],100,0},{entries[1],runs[1],100,0}};
static uint64_t gen,mt,dt,writes,fail=UINT64_MAX;
static bool rd(void *ctx,uint64_t at,uint8_t *p){(void)ctx;if(at>=TOTAL)return false;memcpy(p,disk[at],512);return true;}
static bool wr(void *ctx,uint64_t at,const uint8_t *p){(void)ctx;if(++writes==fail||at>=TOTAL)return false;memcpy(disk[at],p,512);return true;}
static bool fl(void *ctx){(void)ctx;return true;}
static void attach(void){
 struct bs_backend be={0,1,TOTAL,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK);
 assert(bs_grant_range(&b,gen,7,0,META,BS_READ|BS_WRITE,&mt)==BS_OK);
 assert(bs_grant_range(&b,gen,7,META,DATA,BS_READ|BS_WRITE,&dt)==BS_OK);
}
static void mount(void){assert(ic64_mount(&s,&b,7,mt,META,DATA,32,&w.commit)==IC64_OK);}
static bool old(void *ctx,uint64_t i,uint8_t *out,size_t cap,size_t *n){
 (void)ctx;assert(cap>=64);*n=64;
 struct sfs64_file f={i+2,1,0,0};
 return sfs64_map_encode(out,*n,f,DATA,32,0,0);
}
static bool source(void *ctx,uint64_t off,uint8_t *out,size_t n){
 if(ctx)return false;
 for(size_t i=0;i<n;i++)out[i]=(uint8_t)((off+i)^0x5a);
 return true;
}
int main(void){
 attach();assert(ic64_provision(&b,7,mt,META,DATA,32)==IC64_OK);mount();
 assert(ic64_publish(&s,&w.commit,100,old,0)==IC64_OK);
 uint64_t before=writes;
 assert(is64_create(&s,mt,&w,inv,102,1041,source,0)==IS64_INVALID&&writes==before);
 assert(is64_create(&s,dt,&w,inv,101,1041,source,0)==IS64_INVALID&&writes==before);
 assert(is64_create(&s,dt,&w,inv,102,1041,source,(void *)1)==IS64_CANCELLED&&writes==before);
 assert(is64_create(&s,dt,&w,inv,102,1041,source,0)==IS64_OK&&s.table.count==101);
 assert(bs_detach(&b,gen)==BS_OK);attach();mount();
 struct sfs64_map m;
 assert(it64_lookup(&s.table,s.table.geometry.epoch,102,w.old_page,&m)==IT64_OK&&m.file.bytes==1041&&m.file.revision==1);
 struct sfs_extent64 e;uint64_t local;size_t n,done;
 assert(sfs64_map_chunk(&m,0,1041,&e,&local,&n)&&n==1041&&e.first==32);
 uint8_t out[1041];assert(sfs64_read_window(&b,7,dt,e,local,out,n,&done)==BS_OK&&done==n);
 for(size_t i=0;i<n;i++)assert(out[i]==(uint8_t)(i^0x5a));
 struct ow64_result proof;assert(ow64_collect(&s,&w.commit,inv,&proof)==OW64_OK&&proof.valid);
 fail=writes+1;assert(is64_create(&s,dt,&w,inv,103,512,source,0)==IS64_UNCERTAIN&&!s.table.valid);
 assert(bs_detach(&b,gen)==BS_OK);fail=UINT64_MAX;attach();mount();assert(s.table.count==101);
 assert(it64_lookup(&s.table,s.table.geometry.epoch,103,w.old_page,&m)==IT64_MISSING);
 puts("STYLEFS 33 INODE STREAM PASS: 100 existing objects, durable 1041-byte creation, allocation and I/O reuse, cancel and failed write");
 return 0;
}
