#include <styleos/stylefs_paged_volume64.h>
#ifdef STYLEOS_EXPLORER_115_TEST
#include <styleos/explorer_volume_115.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {D=2+16*8,I=2+16*8,F=300,N=2+D+I+F};
static uint8_t disk[N][512],durable[N][512];
static struct bs_service b;static struct pp64_layout l;
static struct pp64_stream_workspace w;
static struct dc64_node nodes[128];
static struct sfs64_run current[128],previous[128];
static struct pp64_pair pair;
static unsigned writes,fail;
static bool rd(void *c,uint64_t n,uint8_t *p){(void)c;if(n>=N)return false;memcpy(p,disk[n],512);return true;}
static bool wr(void *c,uint64_t n,const uint8_t *p){(void)c;assert(n<N);if(++writes==fail)return false;memcpy(disk[n],p,512);return true;}
static bool fl(void *c){(void)c;memcpy(durable,disk,sizeof disk);return true;}
static void attach(void){
 memset(&b,0,sizeof b);uint64_t g;
 struct bs_backend be={0,1,N,true,false,rd,wr,fl};assert(bs_attach(&b,&be,&g)==BS_OK);
 l=(struct pp64_layout){.owner=7,.dir_sectors=D,.inode_sectors=I,.data_sectors=F,.data_start=1};
 assert(bs_grant_range(&b,g,7,0,2,3,&l.receipt)==BS_OK);
 assert(bs_grant_range(&b,g,7,2,D,3,&l.dir)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D,I,3,&l.inode)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D+I,F,3,&l.data)==BS_OK);
}
static void reopen(void){
 memcpy(disk,durable,sizeof disk);writes=fail=0;attach();
 assert(pp64_mount(&b,&l,&w.pair,nodes,128,current,128,&pair)==PP64_OK);
}
static struct sd64_key key(char name){struct sd64_key k={.parent=1,.length=1};k.name[0]=(uint8_t)name;return k;}
struct pattern{uint8_t seed;uint64_t cancel,seen;};
static uint8_t byte(uint8_t seed,uint64_t i){return (uint8_t)(seed+i*17u);}
static bool source(void *ctx,uint64_t at,uint8_t *p,size_t n){
 struct pattern *s=ctx;if(at>=s->cancel)return false;
 for(size_t i=0;i<n;i++)p[i]=byte(s->seed,at+i);
 return true;
}
static bool sink(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 struct pattern *s=ctx;assert(at==s->seen);
 for(size_t i=0;i<n;i++)if(p[i]!=byte(s->seed,at+i))return false;
 s->seen+=n;return true;
}
static enum pp64_status put(char name,uint64_t id,uint64_t rev,uint64_t bytes,struct pattern *s){
 struct sd64_key k=key(name);
 return pp64_put(&b,&l,&w,nodes,128,current,previous,128,pair.sequence,&k,id,rev,bytes,source,s,&pair);
}
static void check(char name,uint64_t id,uint64_t rev,uint64_t bytes,uint8_t seed){
 struct sd64_key k=key(name);struct pattern s={seed,UINT64_MAX,0};
 assert(pp64_get(&b,&l,&w,nodes,128,current,128,pair.sequence,&k,id,rev,sink,&s)==PP64_OK&&s.seen==bytes);
}
static struct pv64_volume v;
static unsigned grants(void){
 unsigned n=0;for(unsigned i=0;i<BS_GRANTS;i++)if(b.grants[i].token)n++;return n;
}
static uint64_t generation(void){
 for(unsigned i=0;i<BS_DEVICES;i++)if(b.devices[i].generation)return b.devices[i].generation;
 return 0;
}
static bool reentry(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 assert(pv64_close(&v)==PO64_BUSY);return sink(ctx,at,p,n);
}
int main(void){
 attach();assert(dc64_provision(&b,7,l.dir,D)==DC64_OK);
 assert(ir64_provision(&b,7,l.inode,I,F,1)==IP64_OK);
 assert(pp64_provision(&b,&l,&w.pair,nodes,128,current,128,&pair)==PP64_OK);reopen();
 struct pattern p={31,UINT64_MAX,0};
 assert(put('a',2,0,513,&p)==PP64_OK);reopen();check('a',2,1,513,31);
 uint64_t g=generation(),t=0,other=0;assert(g);
 struct pv64_geometry geom={0,N,D,I,1},bad=geom;
 assert(pv64_mount(&v,&b,g,8,&geom,true,&w,nodes,128,current,previous,128)==PO64_DENIED&&grants()==4&&!v.attached);
 assert(bs_revoke_owner(&b,7)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D,I,BS_READ,&other)==BS_OK);
 assert(pv64_mount(&v,&b,g,8,&geom,true,&w,nodes,128,current,previous,128)==PO64_DENIED&&grants()==1&&!v.attached);
 assert(bs_revoke(&b,7,other)==BS_OK);
 bad.start=UINT64_MAX;
 assert(pv64_mount(&v,&b,g,8,&bad,true,&w,nodes,128,current,previous,128)==PO64_INVALID&&grants()==0);
 unsigned before=writes;
 assert(pv64_mount(&v,&b,g,8,&geom,true,&w,nodes,128,current,previous,128)==PO64_OK&&grants()==4);
 struct sd64_key k=key('a');
#ifdef STYLEOS_EXPLORER_115_TEST
 struct ex115_volume_info info={0},saved={0};
 struct pv64_volume *list[]={&v};
 assert(ex115_volume_at(&b,list,1,0,&info)==PO64_OK);
 assert(info.device_generation==g&&info.mount==v.objects.mount_id&&info.sequence==v.objects.pair.sequence&&
        info.start_sector==0&&info.volume_bytes==(uint64_t)N*512&&info.device_bytes==(uint64_t)N*512&&info.writable);
 memcpy(&saved,&info,sizeof info);
 assert(ex115_volume_at(&b,list,1,1,&info)==PO64_END&&!memcmp(&info,&saved,sizeof info));
 assert(ex115_volume_at(&b,0,0,0,&info)==PO64_END&&!memcmp(&info,&saved,sizeof info));
 struct pv64_volume *dup[]={&v,&v};
 assert(ex115_volume_at(&b,dup,2,0,&info)==PO64_INVALID&&!memcmp(&info,&saved,sizeof info));
 assert(ex115_volume_info(&v,(struct ex115_volume_info *)&v)==PO64_INVALID);
 assert(writes==before);
#endif
 assert(po64_grant_access(&v.objects,10,&k,2,1,PO64_READ|PO64_WRITE,&t)==PO64_OK);
 p.seen=0;assert(po64_read(&v.objects,10,t,reentry,&p)==PO64_OK&&p.seen==513);
 assert(writes==before);
 p.seed=55;assert(po64_write(&v.objects,10,t,513,source,&p)==PO64_OK);
 assert(pv64_close(&v)==PO64_OK&&grants()==0);
 assert(pv64_close(&v)==PO64_OK);
 assert(pv64_mount(&v,&b,g,8,&geom,false,&w,nodes,128,current,0,128)==PO64_OK);
 assert(po64_grant(&v.objects,10,&k,2,2,&other)==PO64_OK&&other>t);
 p.seen=0;assert(po64_read(&v.objects,10,other,sink,&p)==PO64_OK&&p.seen==513);
 assert(po64_write(&v.objects,10,other,0,0,0)==PO64_DENIED);
#ifdef STYLEOS_EXPLORER_115_TEST
 assert(ex115_volume_at(&b,list,1,0,&info)==PO64_OK&&!info.writable&&info.mount!=saved.mount);
 assert(info.sequence!=saved.sequence);
 memcpy(&saved,&info,sizeof info);
#endif
 assert(bs_detach(&b,g)==BS_OK);
#ifdef STYLEOS_EXPLORER_115_TEST
 assert(ex115_volume_at(&b,list,1,0,&info)==PO64_STALE&&!memcmp(&info,&saved,sizeof info));
 puts("EXPLORER115 actual volume geometry, empty list, remount and detach PASS");
#endif
 assert(po64_read(&v.objects,10,other,sink,&p)==PO64_STALE);
 assert(pv64_close(&v)==PO64_OK&&grants()==0);
 puts("STYLEFS 38 MOUNTED VOLUME PASS: owned grants, partial rollback, persistent handles, readonly mount, detach");
 return 0;
}
