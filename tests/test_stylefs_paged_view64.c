#include <styleos/stylefs_paged_volume64.h>
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
static void edit_dir(uint64_t id,uint64_t parent,char name){
 struct sd64_key k=key(name);k.parent=parent;
 struct dc64_edit e={.op=DC64_CREATE,.entry={k,id,SD64_DIRECTORY}};
 assert(pp64_edit(&b,&l,&w,nodes,128,current,128,pair.sequence,&e,0,&pair)==PP64_OK);
 reopen();
}
int main(void){
 attach();assert(dc64_provision(&b,7,l.dir,D)==DC64_OK);
 assert(ir64_provision(&b,7,l.inode,I,F,1)==IP64_OK);
 assert(pp64_provision(&b,&l,&w.pair,nodes,128,current,128,&pair)==PP64_OK);reopen();
 struct pattern p={31,UINT64_MAX,0};
 uint64_t folder=UINT64_C(4294967300),file=folder+1;
 edit_dir(folder,1,'d');edit_dir(folder+2,1,'e');
 assert(put('a',file,0,513,&p)==PP64_OK);reopen();check('a',file,1,513,31);
 struct sd64_key k=key('z');k.parent=folder;
 assert(pp64_put(&b,&l,&w,nodes,128,current,previous,128,pair.sequence,&k,folder+3,0,0,source,&p,&pair)==PP64_OK);
 reopen();uint64_t g=0;for(unsigned i=0;i<BS_DEVICES;i++)if(b.devices[i].generation)g=b.devices[i].generation;
 assert(g&&bs_revoke_owner(&b,7)==BS_OK);
 struct pv64_geometry geom={0,N,D,I,1};
 assert(pv64_mount(&v,&b,g,8,&geom,true,&w,nodes,128,current,previous,128)==PO64_OK);
 struct po64_snapshot snap,old;struct po64_row row;
 unsigned before=writes;
 assert(po64_snapshot(&v.objects,&snap)==PO64_OK);old=snap;
 assert(po64_child_at(&v.objects,snap,1,0,&row)==PO64_OK&&row.entry.target==file&&row.bytes==513&&row.revision==1);
 assert(po64_child_at(&v.objects,snap,1,1,&row)==PO64_OK&&row.entry.target==folder&&row.entry.kind==SD64_DIRECTORY&&!row.bytes);
 assert(po64_child_at(&v.objects,snap,1,2,&row)==PO64_OK&&row.entry.target==folder+2);
 assert(po64_child_at(&v.objects,snap,1,3,&row)==PO64_END&&row.entry.target==folder+2);
 assert(po64_child_at(&v.objects,snap,folder,0,&row)==PO64_OK&&row.entry.target==folder+3&&!row.bytes&&row.revision==1);
 assert(po64_child_at(&v.objects,snap,folder,UINT64_MAX,&row)==PO64_END);
 assert(po64_child_at(&v.objects,snap,folder+2,0,&row)==PO64_END);
 assert(po64_child_at(&v.objects,snap,file,0,&row)==PO64_DENIED);
 assert(po64_child_at(&v.objects,snap,123,0,&row)==PO64_DENIED);
 assert(writes==before);
 struct sd64_key a=key('a');uint64_t token;
 assert(po64_grant_access(&v.objects,10,&a,file,1,PO64_READ|PO64_WRITE,&token)==PO64_OK);
 p.seed=55;assert(po64_write(&v.objects,10,token,1025,source,&p)==PO64_OK);
 assert(po64_child_at(&v.objects,old,1,0,&row)==PO64_STALE);
 assert(po64_snapshot(&v.objects,&snap)==PO64_OK);
 assert(po64_child_at(&v.objects,snap,1,0,&row)==PO64_OK&&row.bytes==1025&&row.revision==2);
 p.seen=0;assert(po64_grant(&v.objects,10,&a,file,2,&token)==PO64_OK);
 assert(po64_read(&v.objects,10,token,sink,&p)==PO64_OK&&p.seen==1025);
 old=snap;assert(pv64_close(&v)==PO64_OK);
 assert(pv64_mount(&v,&b,g,8,&geom,false,&w,nodes,128,current,0,128)==PO64_OK);
 assert(po64_child_at(&v.objects,old,1,0,&row)==PO64_STALE);
 assert(po64_snapshot(&v.objects,&snap)==PO64_OK&&snap.mount!=old.mount);
 assert(bs_detach(&b,g)==BS_OK);
 assert(po64_child_at(&v.objects,snap,1,0,&row)==PO64_STALE);
 assert(pv64_close(&v)==PO64_OK);
 puts("STYLEFS 40 DIRECTORY VIEW PASS: 64-bit IDs, child order, metadata, empty/end, stale mutation/remount, detach");
 return 0;
}
