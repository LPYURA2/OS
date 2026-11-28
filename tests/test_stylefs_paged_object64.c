#include <styleos/stylefs_paged_object64.h>
#ifdef STYLEOS_NATIVE_FILE_85_TEST
#include <styleos/native_file_85.h>
#include <styleos/native_file_abi.h>
#endif
#ifdef STYLEOS_EXPLORER_111_TEST
#include <styleos/explorer_scope_111.h>
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
static unsigned writes,fail,reads;
static bool rd(void *c,uint64_t n,uint8_t *p){(void)c;if(n>=N)return false;++reads;memcpy(p,disk[n],512);return true;}
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
static bool accept(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 (void)ctx;(void)at;(void)p;(void)n;return true;
}
static struct po64_service service;
static uint64_t token;
static bool reenter(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 assert(po64_revoke(&service,10,token)==PO64_BUSY);
 assert(po64_unmount(&service)==PO64_BUSY);
 return sink(ctx,at,p,n);
}
static bool cancel(void *ctx,uint64_t at,const uint8_t *p,size_t n){(void)ctx;(void)at;(void)p;(void)n;return false;}
static void read_only(void){
 assert(bs_revoke_owner(&b,7)==BS_OK);
 uint64_t g=0;
 for(unsigned i=0;i<BS_DEVICES;i++)if(b.devices[i].generation)g=b.devices[i].generation;
 assert(g);
 assert(bs_grant_range(&b,g,7,0,2,BS_READ,&l.receipt)==BS_OK);
 assert(bs_grant_range(&b,g,7,2,D,BS_READ,&l.dir)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D,I,BS_READ,&l.inode)==BS_OK);
 assert(bs_grant_range(&b,g,7,2+D+I,F,BS_READ,&l.data)==BS_OK);
}
static bool write_source(void *ctx,uint64_t at,uint8_t *p,size_t n){
 assert(po64_write(&service,10,token,0,0,0)==PO64_BUSY);
 assert(po64_unmount(&service)==PO64_BUSY);
 return source(ctx,at,p,n);
}
#ifdef STYLEOS_NATIVE_FILE_85_TEST
static struct nf85_service nf;
static struct pr_table procs;
static struct ws_service wins;
static struct usr_runtime users;
static void native_file85(void){
 pr_init(&procs);ws_init(&wins);
 assert(usr_init(&users,&procs,&wins)==USR_OK);
 uint32_t a=0,z=0,pid=0,peer=0;
 assert(usr_create(&users,111,&a)==USR_OK&&usr_create(&users,222,&z)==USR_OK);
 assert(usr_select(&users,a)==USR_OK);
 struct pr_admission spec={PR_NATIVE_USER,0,16,65536,4096,"Editor"};
 assert(pr_admit(&procs,&spec,&pid)==PR_OK&&usr_bind(&users,a,pid,16)==USR_OK);
 spec.scheduler_handle=32;spec.page_root=131072;
 assert(pr_admit(&procs,&spec,&peer)==PR_OK);
 /* The inactive user must refuse binding; explicitly select and return. */
 assert(usr_bind(&users,z,peer,32)==USR_DENIED);
 assert(usr_select(&users,z)==USR_OK);
 assert(usr_bind(&users,z,peer,32)==USR_OK);
 assert(usr_select(&users,a)==USR_OK);
 assert(nf85_start(&nf,&service,&users)==PO64_OK);
 struct sd64_key k=key('a');uint64_t grant=0,other=0;
 unsigned before=writes;
 assert(nf85_grant_read(&nf,pid,17,65536,&k,2,1,&grant)==PO64_DENIED&&!grant);
 assert(nf85_grant_read(&nf,pid,16,65536,&k,3,1,&grant)==PO64_DENIED&&!grant);
 assert(nf85_grant_read(&nf,pid,16,65536,&k,2,1,&grant)==PO64_OK&&grant);
 uint8_t data[514];memset(data,0x5a,sizeof data);size_t n=999;
 assert(nf85_read(&nf,pid,16,65536,grant,data,512,&n)==PO64_FULL&&n==999);
 assert(nf85_read(&nf,pid,16,65536,grant,nf.scratch,512,&n)==PO64_INVALID&&n==999);
 assert(nf85_read(&nf,pid,16,65536,grant,(uint8_t *)&n,sizeof n,&n)==PO64_INVALID&&n==999);
 assert(nf85_read(&nf,pid,16,65536,grant,(uint8_t *)&service,sizeof service<=NF85_READ_MAX?sizeof service:1,&n)==PO64_INVALID&&n==999);
 for(unsigned i=0;i<sizeof data;++i)assert(data[i]==0x5a);
 assert(nf85_read(&nf,pid,16,69632,grant,data,513,&n)==PO64_DENIED&&n==999);
 assert(nf85_read(&nf,pid,16,65536,grant,data,513,&n)==PO64_OK&&n==513);
 for(unsigned i=0;i<513;++i)assert(data[i]==byte(31,i));
 assert(data[513]==0x5a);
 for(unsigned i=0;i<NF85_READ_MAX;++i)assert(nf.scratch[i]==0);
 assert(usr_lock(&users)==USR_OK);
 assert(nf85_read(&nf,pid,16,65536,grant,data,513,&n)==PO64_DENIED);
 assert(nf85_reconcile(&nf)==0);
 assert(usr_select(&users,z)==USR_OK);
 assert(nf85_read(&nf,peer,32,131072,grant,data,513,&n)==PO64_DENIED);
 k=key('b');
 assert(nf85_grant_read(&nf,peer,32,131072,&k,3,1,&other)==PO64_OK&&other!=grant);
 assert(nf85_read(&nf,peer,32,131072,other,data,513,&n)==PO64_OK&&n==513);
 for(unsigned i=0;i<513;++i)assert(data[i]==byte(44,i));
 assert(nf85_close(&nf,peer,32,131072,grant)==PO64_DENIED);
 assert(nf85_close(&nf,peer,32,131072,other)==PO64_OK);
 assert(nf85_read(&nf,peer,32,131072,other,data,513,&n)==PO64_DENIED);
 assert(usr_select(&users,a)==USR_OK);
 assert(nf85_read(&nf,pid,16,65536,grant,data,513,&n)==PO64_OK);
 /* Revoked exact object grant is stale, never silently reissued. */
 assert(nf85_grant_read(&nf,pid,16,65536,&(struct sd64_key){.parent=1,.length=1,.name={'a'}},2,1,&other)==PO64_OK);
 assert(po64_revoke(&service,UINT64_C(0x8000000000000000)|pid,other)==PO64_OK);
 n=999;
 assert(nf85_read(&nf,pid,16,65536,other,data,513,&n)==PO64_STALE&&n==999);
 assert(nf85_reconcile(&nf)==1);
 assert(pr_transition(&procs,pid,16,PR_FAULTED)==PR_OK);
 assert(nf85_reconcile(&nf)==1&&nf85_reconcile(&nf)==0);
 assert(po64_read(&service,UINT64_C(0x8000000000000000)|pid,grant,accept,0)==PO64_DENIED);
 assert(writes==before);
 assert(nf85_grant_access(&nf,peer,32,131072,&k,3,1,PO64_WRITE,&other)==PO64_DENIED);
 puts("Native file 85: real StyleFS object, owner/session, bounded read and cleanup PASS");
}
#ifdef STYLEOS_NATIVE_FILE_86_TEST
static void native_file86(void){
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 uint32_t pid=0;
 struct pr_admission spec={PR_NATIVE_USER,0,48,196608,4096,"Writer"};
 assert(pr_admit(&procs,&spec,&pid)==PR_OK);
 assert(usr_select(&users,1)==USR_OK&&usr_bind(&users,1,pid,48)==USR_OK);
 struct sd64_key a=key('a'),h=key('h');
 uint64_t ro=0,rw=0,other=0;
 assert(nf85_grant_access(&nf,pid,48,196608,&a,4,1,0,&rw)==PO64_INVALID);
 assert(nf85_grant_read(&nf,pid,48,196608,&a,4,1,&ro)==PO64_OK);
 assert(nf85_grant_access(&nf,pid,48,196608,&a,4,1,PO64_READ|PO64_WRITE,&rw)==PO64_OK);
 assert(nf85_grant_read(&nf,pid,48,196608,&h,5,1,&other)==PO64_OK);
 uint8_t data[513],out[514];
 for(unsigned i=0;i<sizeof data;++i)data[i]=byte(91,i);
 unsigned before=writes;
 assert(nf85_write(&nf,pid,48,196608,ro,data,sizeof data)==PO64_DENIED);
 assert(nf85_write(&nf,pid,48,200704,rw,data,sizeof data)==PO64_DENIED);
 assert(nf85_write(&nf,pid,48,196608,rw,data,NF85_READ_MAX+1)==PO64_INVALID);
 assert(nf85_write(&nf,pid,48,196608,rw,nf.scratch,1)==PO64_INVALID);
 assert(usr_lock(&users)==USR_OK);
 assert(nf85_write(&nf,pid,48,196608,rw,data,sizeof data)==PO64_DENIED);
 assert(writes==before);
 assert(usr_select(&users,1)==USR_OK);
 assert(nf85_write(&nf,pid,48,196608,rw,data,sizeof data)==PO64_OK);
 assert(writes>before);
 for(unsigned i=0;i<NF85_READ_MAX;++i)assert(!nf.scratch[i]);
 for(unsigned i=0;i<NF85_HANDLES;++i)assert(!nf.grants[i].token);
 size_t n=999;
 assert(nf85_read(&nf,pid,48,196608,other,out,sizeof out,&n)==PO64_DENIED&&n==999);
 assert(nf85_write(&nf,pid,48,196608,rw,data,sizeof data)==PO64_DENIED);
 assert(nf85_grant_read(&nf,pid,48,196608,&a,4,1,&ro)==PO64_STALE);
 assert(nf85_grant_access(&nf,pid,48,196608,&a,4,2,PO64_WRITE,&rw)==PO64_OK);
 assert(nf85_read(&nf,pid,48,196608,rw,out,sizeof out,&n)==PO64_DENIED&&n==999);
 assert(po64_unmount(&service)==PO64_OK);
 assert(nf85_reconcile(&nf)==1);
 reopen();check('a',4,2,513,91);check('h',5,1,0,0);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(nf85_grant_access(&nf,pid,48,196608,&a,4,2,PO64_READ|PO64_WRITE,&rw)==PO64_OK);
 memset(out,0x5a,sizeof out);
 assert(nf85_read(&nf,pid,48,196608,rw,out,sizeof out,&n)==PO64_OK&&n==513);
 assert(!memcmp(out,data,513)&&out[513]==0x5a);
 fail=writes+1;data[0]^=0xff;
 assert(nf85_write(&nf,pid,48,196608,rw,data,sizeof data)==PO64_UNCERTAIN&&!service.active);
 assert(nf85_reconcile(&nf)==0);
 for(unsigned i=0;i<NF85_READ_MAX;++i)assert(!nf.scratch[i]);
 reopen();check('a',4,2,513,91);check('h',5,1,0,0);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(nf85_grant_access(&nf,pid,48,196608,&a,4,2,PO64_WRITE,&rw)==PO64_OK);
 assert(nf85_write(&nf,pid,48,196608,rw,0,0)==PO64_OK);
 assert(po64_unmount(&service)==PO64_OK);
 reopen();check('a',4,3,0,0);check('h',5,1,0,0);
 puts("Native file 86: authorized durable save, cold reopen, write failure recovery, empty file and revocation PASS");
}
#ifdef STYLEOS_NATIVE_FILE_87_TEST
struct stream_probe {uint32_t pid;uint64_t token,seen,stop;uint8_t seed;unsigned calls;};
static bool stream_probe_sink(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 struct stream_probe *v=ctx;
 assert(p==nf.scratch&&n&&n<=NF85_READ_MAX&&at==v->seen);
 assert(nf85_close(&nf,v->pid,64,262144,v->token)==PO64_BUSY);
 if(at>=v->stop)return false;
 for(size_t i=0;i<n;++i)assert(p[i]==byte(v->seed,at+i));
 v->seen+=n;++v->calls;return true;
}
static bool stream_probe_source(void *ctx,uint64_t at,uint8_t *p,size_t n){
 struct stream_probe *v=ctx;
 assert(p==nf.scratch&&n&&n<=NF85_READ_MAX&&at==v->seen);
 assert(nf85_close(&nf,v->pid,64,262144,v->token)==PO64_BUSY);
 if(at>=v->stop)return false;
 for(size_t i=0;i<n;++i)p[i]=byte(v->seed,at+i);
 v->seen+=n;++v->calls;return true;
}
#ifdef STYLEOS_NATIVE_FILE_96_TEST
static struct pp96_reader cursor96;
static bool sink96(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 assert(po96_cancel(&service,&cursor96)==PO64_BUSY);
 assert(po64_unmount(&service)==PO64_BUSY);
 return sink(ctx,at,p,n);
}
static void clean96(void){
 const uint8_t *p=(const void *)&cursor96;
 for(size_t i=0;i<sizeof cursor96;++i)assert(!p[i]);
 assert(!service.busy&&!service.reader);
}
static void read96(void){
 const uint64_t bytes=SFS64_WINDOW_BYTES+17;
 struct sd64_key a=key('a'),h=key('h');
 uint64_t grant=0,empty=0;unsigned before=writes;
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(po64_grant(&service,888,&a,4,4,&grant)==PO64_OK);
 assert(po64_grant(&service,888,&h,5,1,&empty)==PO64_OK);
 assert(po96_begin(&service,889,grant,&cursor96)==PO64_DENIED);
 assert(po96_begin(&service,888,grant,&cursor96)==PO64_OK);
 struct pp96_reader other={0};bool done=true;
 struct pattern v={123,UINT64_MAX,0};
 assert(po96_step(&service,&other,sink96,&v,&done)==PO64_DENIED);
 assert(po64_revoke(&service,888,grant)==PO64_BUSY);
 unsigned start=reads;
 assert(po96_step(&service,&cursor96,sink96,&v,&done)==PO64_OK&&!done);
 assert(reads-start==8&&v.seen==4096);
 assert(po96_cancel(&service,&cursor96)==PO64_CANCELLED);clean96();
 assert(po96_begin(&service,888,grant,&cursor96)==PO64_OK);
 v.seen=0;unsigned steps=0;
 do{
  start=reads;
  assert(po96_step(&service,&cursor96,sink96,&v,&done)==PO64_OK);
  assert(reads-start>0&&reads-start<=PP96_STEP_SECTORS);
  ++steps;
 }while(!done);
 assert(steps==9&&v.seen==bytes&&writes==before);clean96();
 assert(po96_begin(&service,888,empty,&cursor96)==PO64_OK);
 start=reads;v.seen=0;done=false;
 assert(po96_step(&service,&cursor96,sink96,&v,&done)==PO64_OK&&done&&!v.seen&&reads==start);clean96();
 assert(po96_begin(&service,888,grant,&cursor96)==PO64_OK);
 done=true;start=reads;
 assert(po96_step(&service,&cursor96,cancel,0,&done)==PO64_CANCELLED&&done&&reads-start==1);clean96();
 assert(po96_begin(&service,888,grant,&cursor96)==PO64_OK);
 struct sfs64_map map;struct sfs_extent64 extent;uint64_t local;size_t take;
 assert(sfs64_map_decode(cursor96.manifest,cursor96.map_bytes,l.data_sectors,l.data_start,&map));
 assert(sfs64_map_chunk(&map,0,1,&extent,&local,&take));
 disk[2+D+I+extent.first][0]^=1;
 enum po64_status rc;done=false;
 do{start=reads;rc=po96_step(&service,&cursor96,accept,0,&done);assert(reads-start<=8);}while(rc==PO64_OK&&!done);
 assert(rc==PO64_CORRUPT&&!done&&!service.active&&writes==before);clean96();
 reopen();check('a',4,4,bytes,123);
 puts("Native file 96: resumable 8-sector data read, exclusive cursor, cancel and CRC rejection PASS");
}
#endif
static void native_file87(void){
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 uint32_t pid=0;
 struct pr_admission spec={PR_NATIVE_USER,0,64,262144,4096,"Streamer"};
 assert(pr_admit(&procs,&spec,&pid)==PR_OK&&usr_bind(&users,1,pid,64)==USR_OK);
 struct sd64_key a=key('a'),h=key('h');uint64_t token=0,ro=0,completed=999;
 const uint64_t bytes=SFS64_WINDOW_BYTES+17;
 assert(nf85_grant_access(&nf,pid,64,262144,&a,4,3,PO64_READ|PO64_WRITE,&token)==PO64_OK);
 assert(nf85_grant_read(&nf,pid,64,262144,&h,5,1,&ro)==PO64_OK);
 struct stream_probe v={pid,token,0,UINT64_MAX,123,0};
 unsigned before=writes;
 assert(nf85_stream_write(&nf,pid,64,262144,ro,bytes,stream_probe_source,&v)==PO64_DENIED&&!v.calls);
 assert(nf85_stream_write(&nf,pid,64,266240,token,bytes,stream_probe_source,&v)==PO64_DENIED&&!v.calls);
 assert(writes==before);
 /* Cancel after a full underlying 32 KiB window has been staged. */
 v.stop=SFS64_WINDOW_BYTES;
 assert(nf85_stream_write(&nf,pid,64,262144,token,bytes,stream_probe_source,&v)==PO64_CANCELLED);
 assert(v.seen==SFS64_WINDOW_BYTES&&service.active);
 assert(nf85_stream_read(&nf,pid,64,262144,token,stream_probe_sink,&v,&completed)==PO64_OK&&completed==0);
 v=(struct stream_probe){pid,token,0,UINT64_MAX,123,0};
 assert(nf85_stream_write(&nf,pid,64,262144,token,bytes,stream_probe_source,&v)==PO64_OK);
 assert(v.seen==bytes&&v.calls==(bytes+NF85_READ_MAX-1)/NF85_READ_MAX);
 for(unsigned i=0;i<NF85_HANDLES;++i)assert(!nf.grants[i].token);
 for(unsigned i=0;i<NF85_READ_MAX;++i)assert(!nf.scratch[i]);
 assert(po64_unmount(&service)==PO64_OK);reopen();
 check('a',4,4,bytes,123);check('h',5,1,0,0);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(nf85_grant_read(&nf,pid,64,262144,&a,4,4,&token)==PO64_OK);
 v=(struct stream_probe){pid,token,0,NF85_READ_MAX,123,0};completed=999;
 assert(nf85_stream_read(&nf,pid,64,262144,token,stream_probe_sink,&v,&completed)==PO64_CANCELLED&&completed==999);
 assert(v.seen==NF85_READ_MAX);
 v=(struct stream_probe){pid,token,0,UINT64_MAX,123,0};
 assert(nf85_stream_read(&nf,pid,64,262144,token,stream_probe_sink,&v,&completed)==PO64_OK&&completed==bytes);
 assert(v.seen==bytes&&v.calls==(bytes+NF85_READ_MAX-1)/NF85_READ_MAX);
 for(unsigned i=0;i<NF85_READ_MAX;++i)assert(!nf.scratch[i]);
 assert(nf85_close(&nf,pid,64,262144,token)==PO64_OK);
 assert(po64_unmount(&service)==PO64_OK);
 puts("Native file 87: 64-bit offsets, 4KiB chunks, multi-window streaming, cancellation and durable reopen PASS");
#ifdef STYLEOS_NATIVE_FILE_96_TEST
 read96();
#endif
}
#ifdef STYLEOS_NATIVE_FILE_89_TEST
static struct nf89_queue queue;
static void native_file89(void){
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 uint32_t pid=0,peer=0;
 struct pr_admission spec={PR_NATIVE_USER,0,80,327680,4096,"Queue"};
 assert(pr_admit(&procs,&spec,&pid)==PR_OK&&usr_bind(&users,1,pid,80)==USR_OK);
 spec.scheduler_handle=96;spec.page_root=393216;
 assert(pr_admit(&procs,&spec,&peer)==PR_OK&&usr_bind(&users,1,peer,96)==USR_OK);
 assert(nf89_start(&queue,&nf)==PO64_OK&&nf89_start(&queue,&nf)==PO64_INVALID);
 struct sd64_key a=key('a'),h=key('h');
 uint64_t token=0,other=0,request=0,second=0;
 assert(nf85_grant_access(&nf,pid,80,327680,&a,4,4,PO64_READ|PO64_WRITE,&token)==PO64_OK);
 assert(nf85_grant_read(&nf,peer,96,393216,&h,5,1,&other)==PO64_OK);
 uint8_t data[513],out[514];size_t n=999;enum po64_status result=PO64_INVALID;
 for(unsigned i=0;i<sizeof data;++i)data[i]=byte(77,i);
 memset(out,0x5a,sizeof out);unsigned before=writes;
 assert(nf89_submit(&queue,peer,96,393216,token,NF89_WRITE,data,sizeof data,&request)==PO64_DENIED&&!request);
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_WRITE,data,sizeof data,&request)==PO64_OK&&request);
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_READ,0,513,&second)==PO64_BUSY&&!second);
 assert(nf89_submit(&queue,peer,96,393216,other,NF89_READ,0,1,&second)==PO64_OK&&second>request);
 assert(writes==before);
 assert(nf89_take(&queue,pid,80,327680,request,out,sizeof out,&n,&result)==PO64_BUSY&&n==999);
 assert(nf89_cancel(&queue,peer,96,393216,request)==PO64_DENIED);
 assert(nf89_cancel(&queue,pid,80,327680,request)==PO64_OK);
 assert(nf89_take(&queue,pid,80,327680,request,0,0,&n,&result)==PO64_OK&&n==0&&result==PO64_CANCELLED);
 assert(usr_lock(&users)==USR_OK&&!nf89_pump(&queue)&&writes==before);
 assert(usr_select(&users,1)==USR_OK&&nf89_pump(&queue));
 assert(nf89_take(&queue,peer,96,393216,second,out,sizeof out,&n,&result)==PO64_OK&&n==0&&result==PO64_OK);
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_WRITE,data,sizeof data,&request)==PO64_OK);
 memset(data,0,sizeof data);
 assert(nf89_pump(&queue)&&writes>before);
 assert(nf89_cancel(&queue,pid,80,327680,request)==PO64_BUSY);
 assert(nf89_take(&queue,pid,80,327680,request,0,0,&n,&result)==PO64_OK&&n==0&&result==PO64_OK);
 assert(nf89_take(&queue,pid,80,327680,request,0,0,&n,&result)==PO64_DENIED);
 assert(nf85_grant_read(&nf,pid,80,327680,&a,4,5,&token)==PO64_OK);
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_READ,0,513,&request)==PO64_OK);
 assert(nf89_pump(&queue));
 assert(nf89_take(&queue,pid,80,327680,request,out,512,&n,&result)==PO64_FULL);
 assert(nf89_take(&queue,peer,96,393216,request,out,sizeof out,&n,&result)==PO64_DENIED);
 assert(nf89_take(&queue,pid,80,327680,request,out,sizeof out,&n,&result)==PO64_OK&&n==513&&result==PO64_OK);
 for(unsigned i=0;i<513;++i)assert(out[i]==byte(77,i));
 assert(out[513]==0x5a);
#ifdef STYLEOS_NATIVE_FILE_90_TEST
 /* Revoke after successful I/O, before collection; no cached disclosure. */
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_READ,0,513,&request)==PO64_OK);
 assert(nf89_pump(&queue));
 assert(nf85_close(&nf,pid,80,327680,token)==PO64_OK);
 memset(out,0x5a,sizeof out);n=999;
 assert(nf89_take(&queue,pid,80,327680,request,out,sizeof out,&n,&result)==PO64_OK&&n==0&&result==PO64_STALE);
 for(unsigned i=0;i<sizeof out;++i)assert(out[i]==0x5a);
 assert(nf85_grant_read(&nf,pid,80,327680,&a,4,5,&token)==PO64_OK);
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_READ,0,513,&request)==PO64_OK);
 assert(nf85_close(&nf,pid,80,327680,token)==PO64_OK);
 assert(!nf89_pump(&queue));
 assert(nf89_take(&queue,pid,80,327680,request,out,sizeof out,&n,&result)==PO64_OK&&n==0&&result==PO64_STALE);
 assert(nf85_grant_read(&nf,pid,80,327680,&a,4,5,&token)==PO64_OK);
 puts("Native file 90: revoked pending and completed reads disclose no data PASS");
#endif
#ifdef STYLEOS_NATIVE_FILE_93_TEST
 /* No access until the exact selected process receives its approval. */
 assert(nf85_close(&nf,pid,80,327680,token)==PO64_OK);
 token=999;before=writes;
 assert(nf93_receive(&nf,pid,80,327680,&token)==PO64_END&&token==999);
 assert(nf93_offer(&nf,pid,80,327680,&a,4,4,PO64_READ)==PO64_STALE);
 assert(nf93_offer(&nf,pid,80,327680,&a,4,5,PO64_READ)==PO64_OK);
 assert(nf93_offer(&nf,pid,80,327680,&h,5,1,PO64_READ)==PO64_BUSY);
 uint64_t pending=0;
 for(unsigned i=0;i<NF85_HANDLES;++i)if(nf.grants[i].pending_delivery)pending=nf.grants[i].token;
 assert(pending);
 assert(nf89_submit(&queue,pid,80,327680,pending,NF89_READ,0,513,&request)==PO64_DENIED);
 assert(nf93_receive(&nf,peer,96,393216,&token)==PO64_END&&token==999);
 assert(nf93_receive(&nf,pid,80,331776,&token)==PO64_DENIED&&token==999);
 assert(usr_lock(&users)==USR_OK);
 assert(nf93_receive(&nf,pid,80,327680,&token)==PO64_DENIED&&token==999);
 assert(usr_select(&users,1)==USR_OK);
 assert(nf93_receive(&nf,pid,80,327680,&token)==PO64_OK&&token==pending);
 assert(nf93_receive(&nf,pid,80,327680,&other)==PO64_END);
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_WRITE,data,sizeof data,&request)==PO64_DENIED);
 assert(writes==before);
 /* Pending approval for a different owner vanishes on process fault. */
 assert(nf93_offer(&nf,peer,96,393216,&h,5,1,PO64_READ)==PO64_OK);
 assert(pr_transition(&procs,peer,96,PR_FAULTED)==PR_OK);
 assert(nf85_reconcile(&nf)==1);
 assert(nf93_receive(&nf,peer,96,393216,&other)==PO64_DENIED);
 puts("Native file 93: explicit one-shot grant receipt, pending denial, session and fault cleanup PASS");
#endif
#ifdef STYLEOS_NATIVE_FILE_95_TEST
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_READ,0,513,&request)==PO64_OK);
 assert(nf95_wait(&queue,pid,80,327680,request)==PO64_BUSY);
 assert(nf95_wait(&queue,peer,96,393216,request)==PO64_DENIED);
 assert(usr_lock(&users)==USR_OK&&nf95_wait(&queue,pid,80,327680,request)==PO64_BUSY&&!nf89_pump(&queue));
 assert(usr_select(&users,1)==USR_OK&&nf89_pump(&queue));
 assert(nf95_wait(&queue,pid,80,327680,request)==PO64_OK);
 assert(usr_lock(&users)==USR_OK&&nf95_wait(&queue,pid,80,327680,request)==PO64_BUSY);
 assert(usr_select(&users,1)==USR_OK&&nf95_wait(&queue,pid,80,327680,request)==PO64_OK);
 assert(nf89_take(&queue,pid,80,327680,request,out,sizeof out,&n,&result)==PO64_OK&&n==513&&result==PO64_OK);
 assert(nf95_wait(&queue,pid,80,327680,request)==PO64_DENIED);
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_READ,0,513,&request)==PO64_OK);
 assert(nf89_cancel(&queue,pid,80,327680,request)==PO64_OK);
 assert(nf95_wait(&queue,pid,80,327680,request)==PO64_OK);
 assert(nf89_take(&queue,pid,80,327680,request,0,0,&n,&result)==PO64_OK&&!n&&result==PO64_CANCELLED);
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_READ,0,513,&request)==PO64_OK);
 assert(nf85_close(&nf,pid,80,327680,token)==PO64_OK);
 assert(nf95_wait(&queue,pid,80,327680,request)==PO64_OK);
 assert(nf89_take(&queue,pid,80,327680,request,0,0,&n,&result)==PO64_OK&&!n&&result==PO64_STALE);
 assert(nf85_grant_read(&nf,pid,80,327680,&a,4,5,&token)==PO64_OK);
 puts("Native file 95: wait readiness, lock, cancellation and revocation PASS");
#endif
#ifdef STYLEOS_NATIVE_FILE_97_TEST
 /* Small-file read now spans metadata admission and data step. */
 n=999;memset(out,0x5a,sizeof out);
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_READ,0,513,&request)==PO64_OK);
 assert(nf97_pump(&queue)&&queue.read97_id==request&&service.busy);
 assert(nf89_take(&queue,pid,80,327680,request,out,sizeof out,&n,&result)==PO64_BUSY&&n==999);
 assert(nf89_cancel(&queue,peer,96,393216,request)==PO64_DENIED);
 assert(usr_lock(&users)==USR_OK);
 assert(nf97_reconcile(&queue)==1&&!queue.read97_id&&!service.busy);
 assert(!nf97_pump(&queue));
 assert(usr_select(&users,1)==USR_OK&&nf97_pump(&queue)&&service.busy);
 unsigned read_before=reads;
 assert(nf97_pump(&queue)&&reads-read_before==2&&!service.busy);
 assert(nf89_take(&queue,pid,80,327680,request,out,sizeof out,&n,&result)==PO64_OK&&n==513&&result==PO64_OK);
 for(unsigned i=0;i<513;++i)assert(out[i]==byte(77,i));
 assert(out[513]==0x5a);
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_READ,0,512,&request)==PO64_OK);
 assert(nf97_pump(&queue)&&!service.busy&&!queue.read97_id);
 assert(nf89_take(&queue,pid,80,327680,request,0,0,&n,&result)==PO64_OK&&!n&&result==PO64_FULL);
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_READ,0,513,&request)==PO64_OK);
 assert(nf97_pump(&queue)&&service.busy);
 assert(nf89_cancel(&queue,pid,80,327680,request)==PO64_OK&&!service.busy);
 assert(nf89_take(&queue,pid,80,327680,request,0,0,&n,&result)==PO64_OK&&!n&&result==PO64_CANCELLED);
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_READ,0,513,&request)==PO64_OK);
 assert(nf97_pump(&queue)&&service.busy);
 assert(pr_transition(&procs,pid,80,PR_FAULTED)==PR_OK);
 assert(nf97_reconcile(&queue)==1&&!queue.read97_id&&!service.busy);
 assert(nf85_reconcile(&nf)==1);
 for(unsigned i=0;i<NF89_SLOTS;++i){
  const uint8_t *p=(const void *)&queue.slots[i];
  for(size_t j=0;j<sizeof queue.slots[i];++j)assert(!p[j]);
 }
 const uint8_t *cp=(const void *)&queue.read97;
 for(size_t i=0;i<sizeof queue.read97;++i)assert(!cp[i]);
 puts("Native file 97: queued metadata/data steps, lock restart, size refusal, cancel and fault cleanup PASS");
#else
 assert(nf89_submit(&queue,pid,80,327680,token,NF89_READ,0,513,&request)==PO64_OK);
 assert(nf89_pump(&queue));
 assert(pr_transition(&procs,pid,80,PR_FAULTED)==PR_OK);
 assert(nf89_reconcile(&queue)==1&&nf89_reconcile(&queue)==0);
 for(unsigned i=0;i<NF89_SLOTS;++i){
  const uint8_t *p=(const void *)&queue.slots[i];
  for(size_t j=0;j<sizeof queue.slots[i];++j)assert(!p[j]);
 }
 assert(nf85_reconcile(&nf)==1);
#endif
 assert(po64_unmount(&service)==PO64_OK);reopen();
 check('a',4,5,513,77);check('h',5,1,0,0);
 puts("Native file 89: deferred immutable requests, owner isolation, cancellation, completion and cleanup PASS");
}
#endif
#endif
#endif
#endif
#ifdef STYLEOS_NATIVE_FILE_98_TEST
static struct pp98_writer writer98;
static bool source98(void *ctx,uint64_t at,uint8_t *p,size_t n){
 assert(po98_cancel(&service,&writer98)==PO64_BUSY);
 assert(po64_unmount(&service)==PO64_BUSY);
 return source(ctx,at,p,n);
}
static void clean98(void){
 const uint8_t *p=(const void *)&writer98;
 for(size_t i=0;i<sizeof writer98;++i)assert(!p[i]);
 assert(!service.busy&&!service.writer);
}
static void write98(void){
 struct sd64_key a=key('a'),h=key('h');
 uint64_t grant=0,ro=0,bytes=SFS64_WINDOW_BYTES+17;
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(po64_grant_access(&service,999,&a,4,5,PO64_READ|PO64_WRITE,&grant)==PO64_OK);
 assert(po64_grant(&service,999,&h,5,1,&ro)==PO64_OK);
 assert(po98_begin(&service,999,ro,bytes,&writer98)==PO64_DENIED);
 assert(po98_begin(&service,998,grant,bytes,&writer98)==PO64_DENIED);
 assert(po98_begin(&service,999,grant,bytes,&writer98)==PO64_OK);
 struct pp98_writer wrong={0};bool done=true;
 struct pattern v={201,UINT64_MAX,0};
 assert(po98_step(&service,&wrong,source98,&v,&done)==PO64_DENIED);
 assert(po98_commit(&service,&writer98)==PO64_INVALID);
 unsigned start=writes,read_start=reads;
 assert(po98_step(&service,&writer98,source98,&v,&done)==PO64_OK&&!done);
 assert(writes-start==8&&reads-read_start<=16&&writer98.offset==4096);
 assert(po64_revoke(&service,999,grant)==PO64_BUSY);
 assert(po98_cancel(&service,&writer98)==PO64_CANCELLED);clean98();
 assert(po64_unmount(&service)==PO64_OK);reopen();
 check('a',4,5,513,77);check('h',5,1,0,0);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(po64_grant_access(&service,999,&a,4,5,PO64_WRITE,&grant)==PO64_OK);
 assert(po98_begin(&service,999,grant,bytes,&writer98)==PO64_OK);
 fail=writes+1;done=true;
 assert(po98_step(&service,&writer98,source98,&v,&done)==PO64_UNCERTAIN&&done&&!service.active);
 clean98();reopen();check('a',4,5,513,77);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(po64_grant_access(&service,999,&a,4,5,PO64_WRITE,&grant)==PO64_OK);
 assert(po98_begin(&service,999,grant,bytes,&writer98)==PO64_OK);
 unsigned steps=0;
 do{
  start=writes;read_start=reads;
  assert(po98_step(&service,&writer98,source98,&v,&done)==PO64_OK);
  assert(writes-start>0&&writes-start<=8&&reads-read_start<=16);
  ++steps;
 }while(!done);
 assert(steps==9&&writer98.offset==bytes&&service.pair.sequence==writer98.pair.sequence);
 assert(po98_commit(&service,&writer98)==PO64_OK);clean98();
 assert(po64_write(&service,999,grant,0,0,0)==PO64_DENIED);
 assert(po64_unmount(&service)==PO64_OK);reopen();
 check('a',4,6,bytes,201);check('h',5,1,0,0);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(po64_grant_access(&service,999,&a,4,6,PO64_WRITE,&grant)==PO64_OK);
 assert(po98_begin(&service,999,grant,0,&writer98)==PO64_OK);
 start=writes;done=false;
 assert(po98_step(&service,&writer98,0,0,&done)==PO64_OK&&done&&writes==start);
 assert(po98_commit(&service,&writer98)==PO64_OK);clean98();
 assert(po64_unmount(&service)==PO64_OK);reopen();check('a',4,7,0,0);check('h',5,1,0,0);
 puts("Native file 98: bounded staged write, cancellation, uncertain recovery, durable commit and empty replacement PASS");
}
#ifdef STYLEOS_NATIVE_FILE_99_TEST
static void write99(void){
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 uint32_t pid=0;
 struct pr_admission spec={PR_NATIVE_USER,0,112,458752,4096,"Writer99"};
 assert(pr_admit(&procs,&spec,&pid)==PR_OK&&usr_bind(&users,1,pid,112)==USR_OK);
 struct sd64_key a=key('a');
 uint64_t grant=0,request=0;
 assert(nf85_grant_access(&nf,pid,112,458752,&a,4,7,PO64_READ|PO64_WRITE,&grant)==PO64_OK);
 uint8_t data[513];for(unsigned i=0;i<sizeof data;++i)data[i]=byte(55,i);
 size_t n=999;enum po64_status result=PO64_INVALID;
 assert(nf89_submit(&queue,pid,112,458752,grant,NF89_WRITE,data,sizeof data,&request)==PO64_OK);
 assert(nf99_pump(&queue)&&service.busy&&queue.write99_id==request);
 unsigned before=writes,read_before=reads;
 assert(nf99_pump(&queue)&&queue.write99_staged&&writes-before==2);
 assert(nf89_take(&queue,pid,112,458752,request,0,0,&n,&result)==PO64_BUSY&&n==999);
 before=writes;read_before=reads;
 assert(nf89_cancel(&queue,pid,112,458752,request)==PO64_OK&&queue.write99_cancel);
 assert(writes==before&&reads==read_before&&service.busy);
 assert(nf99_pump(&queue)&&!service.busy&&!queue.write99_id);
 assert(nf89_take(&queue,pid,112,458752,request,0,0,&n,&result)==PO64_OK&&!n&&result==PO64_CANCELLED);
 assert(po64_unmount(&service)==PO64_OK);reopen();check('a',4,7,0,0);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(nf85_grant_access(&nf,pid,112,458752,&a,4,7,PO64_WRITE,&grant)==PO64_OK);
 assert(nf89_submit(&queue,pid,112,458752,grant,NF89_WRITE,data,sizeof data,&request)==PO64_OK);
 assert(nf99_pump(&queue)&&service.busy);
 assert(usr_lock(&users)==USR_OK&&nf99_reconcile(&queue)==1&&!service.busy);
 assert(usr_select(&users,1)==USR_OK);
 assert(nf89_take(&queue,pid,112,458752,request,0,0,&n,&result)==PO64_OK&&result==PO64_CANCELLED);
 assert(nf89_submit(&queue,pid,112,458752,grant,NF89_WRITE,data,sizeof data,&request)==PO64_OK);
 memset(data,0,sizeof data);
 uint64_t sequence=service.pair.sequence;
 assert(nf99_pump(&queue)&&service.busy);
 before=writes;
 assert(nf99_pump(&queue)&&queue.write99_staged&&writes-before==2&&service.pair.sequence==sequence);
 assert(nf95_wait(&queue,pid,112,458752,request)==PO64_BUSY);
 assert(nf99_pump(&queue)&&!service.busy&&service.pair.sequence>sequence);
 assert(nf95_wait(&queue,pid,112,458752,request)==PO64_OK);
 assert(nf89_take(&queue,pid,112,458752,request,0,0,&n,&result)==PO64_OK&&!n&&result==PO64_OK);
 assert(nf89_submit(&queue,pid,112,458752,grant,NF89_WRITE,data,sizeof data,&request)==PO64_STALE);
 assert(nf85_grant_access(&nf,pid,112,458752,&a,4,8,PO64_WRITE,&grant)==PO64_OK);
 assert(nf89_submit(&queue,pid,112,458752,grant,NF89_WRITE,data,sizeof data,&request)==PO64_OK);
 assert(nf99_pump(&queue)&&service.busy);
 assert(pr_transition(&procs,pid,112,PR_FAULTED)==PR_OK);
 assert(nf99_reconcile(&queue)==1&&!service.busy&&!queue.write99_id);
 assert(nf85_reconcile(&nf)==1);
 const uint8_t *p=(const void *)&queue.write99;
 for(size_t i=0;i<sizeof queue.write99;++i)assert(!p[i]);
 assert(po64_unmount(&service)==PO64_OK);reopen();check('a',4,8,513,55);check('h',5,1,0,0);
 puts("Native file 99: queued staged save, deferred cancellation, lock, fault and durable publication PASS");
}
#ifdef STYLEOS_NATIVE_FILE_100_TEST
static struct nf100_stream stream100;
static void write100(void){
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 uint32_t pid=0,peer=0;
 struct pr_admission spec={PR_NATIVE_USER,0,128,524288,4096,"Stream100"};
 assert(pr_admit(&procs,&spec,&pid)==PR_OK&&usr_bind(&users,1,pid,128)==USR_OK);
 spec.scheduler_handle=144;spec.page_root=589824;
 assert(pr_admit(&procs,&spec,&peer)==PR_OK&&usr_bind(&users,1,peer,144)==USR_OK);
 struct sd64_key a=key('a');uint64_t grant=0,id=0,old=0;
 const uint64_t total=SFS64_WINDOW_BYTES+17;
 uint8_t data[NF85_READ_MAX];enum po64_status result=PO64_INVALID;
 assert(nf100_start(&stream100,&nf)==PO64_OK&&nf100_start(&stream100,&nf)==PO64_INVALID);
 assert(nf85_grant_access(&nf,pid,128,524288,&a,4,8,PO64_WRITE,&grant)==PO64_OK);
 unsigned before=writes,rd_before=reads;
 assert(nf100_open(&stream100,peer,144,589824,grant,total,&id)==PO64_DENIED&&!id);
 assert(nf100_open(&stream100,pid,128,524288,grant,total,&id)==PO64_OK&&id);
 assert(writes==before&&reads==rd_before);
 assert(nf100_finish(&stream100,pid,128,524288,id)==PO64_BUSY);
 assert(nf100_pump(&stream100)&&stream100.state==NF100_NEED&&service.busy);
 for(unsigned i=1;i<NF100_IDLE_LIMIT;++i)assert(!nf100_pump(&stream100));
 assert(nf100_pump(&stream100)&&!service.busy&&stream100.state==NF100_DONE);
 assert(nf100_result(&stream100,pid,128,524288,id,&result)==PO64_OK&&result==PO64_CANCELLED);
 old=id;
 assert(nf100_open(&stream100,pid,128,524288,grant,total,&id)==PO64_OK&&id>old);
 assert(nf100_pump(&stream100));
 assert(nf100_feed(&stream100,pid,128,524288,id,1,data,sizeof data)==PO64_INVALID);
 assert(nf100_feed(&stream100,peer,144,589824,id,0,data,sizeof data)==PO64_DENIED);
 assert(nf100_feed(&stream100,pid,128,524288,old,0,data,sizeof data)==PO64_DENIED);
 assert(nf100_feed(&stream100,pid,128,524288,id,0,data,sizeof data-1)==PO64_INVALID);
 for(unsigned i=0;i<sizeof data;++i)data[i]=byte(88,i);
 before=writes;rd_before=reads;
 assert(nf100_feed(&stream100,pid,128,524288,id,0,data,sizeof data)==PO64_OK);
 assert(writes==before&&reads==rd_before);
 assert(nf100_pump(&stream100)&&stream100.offset==4096&&writes-before==8);
 assert(usr_lock(&users)==USR_OK&&nf100_pump(&stream100)&&!service.busy);
 assert(usr_select(&users,1)==USR_OK);
 assert(nf100_result(&stream100,pid,128,524288,id,&result)==PO64_OK&&result==PO64_CANCELLED);
 assert(po64_unmount(&service)==PO64_OK);reopen();check('a',4,8,513,55);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(nf85_grant_access(&nf,pid,128,524288,&a,4,8,PO64_WRITE,&grant)==PO64_OK);
 assert(nf100_open(&stream100,pid,128,524288,grant,total,&id)==PO64_OK);
 assert(nf100_pump(&stream100));
 uint64_t sequence=service.pair.sequence;
 for(uint64_t at=0;at<total;){
  size_t n=total-at>sizeof data?sizeof data:(size_t)(total-at);
  for(size_t i=0;i<n;++i)data[i]=byte(88,at+i);
  before=writes;rd_before=reads;
  assert(nf100_feed(&stream100,pid,128,524288,id,at,data,n)==PO64_OK);
  memset(data,0,sizeof data);
  assert(writes==before&&reads==rd_before);
  assert(nf100_pump(&stream100)&&writes-before>0&&writes-before<=8&&stream100.offset==at+n);
  assert(service.pair.sequence==sequence);
  for(unsigned i=0;i<sizeof stream100.data;++i)assert(!stream100.data[i]);
  at+=n;
 }
 assert(stream100.state==NF100_STAGED&&service.busy);
 assert(nf100_result(&stream100,pid,128,524288,id,&result)==PO64_BUSY);
 before=writes;rd_before=reads;
 assert(nf100_finish(&stream100,pid,128,524288,id)==PO64_OK&&writes==before&&reads==rd_before);
 assert(nf100_pump(&stream100)&&!service.busy&&service.pair.sequence>sequence);
 assert(nf100_result(&stream100,pid,128,524288,id,&result)==PO64_OK&&result==PO64_OK);
 assert(po64_unmount(&service)==PO64_OK);reopen();check('a',4,9,total,88);check('h',5,1,0,0);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(nf85_grant_access(&nf,pid,128,524288,&a,4,9,PO64_WRITE,&grant)==PO64_OK);
 assert(nf100_open(&stream100,pid,128,524288,grant,0,&id)==PO64_OK);
 assert(nf100_pump(&stream100)&&stream100.state==NF100_STAGED);
 assert(nf100_finish(&stream100,pid,128,524288,id)==PO64_OK);
 assert(nf100_cancel(&stream100,pid,128,524288,id)==PO64_OK);
 assert(nf100_pump(&stream100)&&!service.busy);
 assert(nf100_result(&stream100,pid,128,524288,id,&result)==PO64_OK&&result==PO64_CANCELLED);
 assert(nf100_open(&stream100,pid,128,524288,grant,total,&id)==PO64_OK);
 assert(nf100_pump(&stream100)&&service.busy);
 assert(pr_transition(&procs,pid,128,PR_FAULTED)==PR_OK);
 assert(nf100_pump(&stream100)&&stream100.state==NF100_IDLE&&!service.busy);
 assert(nf85_reconcile(&nf)==1);
 assert(po64_unmount(&service)==PO64_OK);reopen();check('a',4,9,total,88);
 puts("Native file 100: bounded write stream, explicit commit, idle cancel, lock and owner cleanup PASS");
}
#ifdef STYLEOS_NATIVE_FILE_101_TEST
static struct nf101_stream read101;
static void stream_read101(void){
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 uint32_t pid=0,peer=0;
 struct pr_admission spec={PR_NATIVE_USER,0,160,655360,4096,"Reader101"};
 assert(pr_admit(&procs,&spec,&pid)==PR_OK&&usr_bind(&users,1,pid,160)==USR_OK);
 spec.scheduler_handle=176;spec.page_root=720896;
 assert(pr_admit(&procs,&spec,&peer)==PR_OK&&usr_bind(&users,1,peer,176)==USR_OK);
 struct sd64_key a=key('a'),h=key('h');
 uint64_t grant=0,empty=0,id=0,old=0;
 const uint64_t total=SFS64_WINDOW_BYTES+17;
 uint8_t data[NF85_READ_MAX];size_t n=999;enum po64_status result=PO64_INVALID;
 assert(nf101_start(&read101,&nf)==PO64_OK&&nf101_start(&read101,&nf)==PO64_INVALID);
 assert(nf85_grant_read(&nf,pid,160,655360,&a,4,9,&grant)==PO64_OK);
 assert(nf85_grant_read(&nf,pid,160,655360,&h,5,1,&empty)==PO64_OK);
 unsigned before=writes,rd_before=reads;
 assert(nf101_open(&read101,peer,176,720896,grant,&id)==PO64_DENIED&&!id);
 assert(nf101_open(&read101,pid,160,655360,grant,&id)==PO64_OK&&id);
 assert(writes==before&&reads==rd_before);
 assert(nf101_pump(&read101)&&read101.state==NF101_PULL);
 rd_before=reads;
 assert(nf101_pump(&read101)&&read101.state==NF101_READY&&reads-rd_before==8);
 for(unsigned i=1;i<NF100_IDLE_LIMIT;++i)assert(!nf101_pump(&read101));
 assert(nf101_pump(&read101)&&!service.busy);
 assert(nf101_result(&read101,pid,160,655360,id,&result)==PO64_OK&&result==PO64_CANCELLED);
 old=id;
 assert(nf101_open(&read101,pid,160,655360,grant,&id)==PO64_OK&&id>old);
 assert(nf101_pump(&read101)&&nf101_pump(&read101));
 assert(nf101_take(&read101,peer,176,720896,id,0,data,sizeof data,&n)==PO64_DENIED&&n==999);
 assert(nf101_take(&read101,pid,160,655360,old,0,data,sizeof data,&n)==PO64_DENIED&&n==999);
 assert(nf101_take(&read101,pid,160,655360,id,1,data,sizeof data,&n)==PO64_INVALID&&n==999);
 assert(nf101_take(&read101,pid,160,655360,id,0,data,sizeof data-1,&n)==PO64_FULL&&n==999);
 assert(usr_lock(&users)==USR_OK&&nf101_pump(&read101)&&!service.busy);
 assert(usr_select(&users,1)==USR_OK);
 assert(nf101_result(&read101,pid,160,655360,id,&result)==PO64_OK&&result==PO64_CANCELLED);
 assert(nf101_open(&read101,pid,160,655360,grant,&id)==PO64_OK);
 assert(nf101_pump(&read101));
 for(uint64_t at=0;at<total;){
  rd_before=reads;
  assert(nf101_pump(&read101)&&reads-rd_before>0&&reads-rd_before<=8);
  assert(read101.state==NF101_READY&&read101.verified==(total-at<=sizeof data));
  rd_before=reads;
  assert(nf101_take(&read101,pid,160,655360,id,at,data,sizeof data,&n)==PO64_OK);
  assert(n&&n<=sizeof data&&reads==rd_before&&writes==before);
  for(size_t i=0;i<n;++i)assert(data[i]==byte(88,at+i));
  for(size_t i=0;i<sizeof read101.data;++i)assert(!read101.data[i]);
  at+=n;
 }
 assert(nf101_result(&read101,pid,160,655360,id,&result)==PO64_OK&&result==PO64_OK);
 assert(nf101_open(&read101,pid,160,655360,empty,&id)==PO64_OK);
 assert(nf101_pump(&read101));
 rd_before=reads;
 assert(nf101_pump(&read101)&&reads==rd_before&&!service.busy);
 assert(nf101_result(&read101,pid,160,655360,id,&result)==PO64_OK&&result==PO64_OK);
 /* Corrupt the last data sector after begin; earlier delivered chunks are
  * provisional, final chunk must never be released on failed checksum. */
 assert(nf101_open(&read101,pid,160,655360,grant,&id)==PO64_OK);
 assert(nf101_pump(&read101));
 struct sfs64_map map;struct sfs_extent64 ex;uint64_t local;size_t take;
 assert(sfs64_map_decode(read101.reader.manifest,read101.reader.map_bytes,l.data_sectors,l.data_start,&map));
 assert(sfs64_map_chunk(&map,total-1,1,&ex,&local,&take));
 disk[2+D+I+ex.first+local/512][local%512]^=1;
 for(uint64_t at=0;at<total-17;at+=4096){
  assert(nf101_pump(&read101));
  assert(nf101_take(&read101,pid,160,655360,id,at,data,sizeof data,&n)==PO64_OK&&n==4096);
 }
 assert(nf101_pump(&read101)&&!service.active&&!service.busy&&read101.state==NF101_DONE);
 memset(data,0x5a,sizeof data);n=999;
 assert(nf101_take(&read101,pid,160,655360,id,total-17,data,sizeof data,&n)==PO64_END&&n==999&&data[0]==0x5a);
 assert(nf101_result(&read101,pid,160,655360,id,&result)==PO64_OK&&result==PO64_CORRUPT);
 reopen();check('a',4,9,total,88);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(nf85_grant_read(&nf,pid,160,655360,&a,4,9,&grant)==PO64_OK);
 assert(nf101_open(&read101,pid,160,655360,grant,&id)==PO64_OK);
 assert(nf101_pump(&read101)&&nf101_pump(&read101)&&service.busy);
 assert(pr_transition(&procs,pid,160,PR_FAULTED)==PR_OK);
 assert(nf101_pump(&read101)&&!service.busy&&read101.state==NF101_IDLE);
 assert(nf85_reconcile(&nf)==1);
 assert(po64_unmount(&service)==PO64_OK);reopen();check('a',4,9,total,88);
 puts("Native file 101: bounded provisional read stream, final CRC, idle cancel and owner cleanup PASS");
}
#ifdef STYLEOS_NATIVE_FILE_102_TEST
static struct nf102_service stream102;
static void packet102(uint8_t *p,uint64_t at){
 for(unsigned i=0;i<8;++i)p[i]=(uint8_t)(at>>(i*8));
}
static void stream_test102(void){
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 uint32_t pid=0,peer=0;
 struct pr_admission spec={PR_NATIVE_USER,0,192,786432,4096,"Stream102"};
 assert(pr_admit(&procs,&spec,&pid)==PR_OK&&usr_bind(&users,1,pid,192)==USR_OK);
 spec.scheduler_handle=208;spec.page_root=851968;
 assert(pr_admit(&procs,&spec,&peer)==PR_OK&&usr_bind(&users,1,peer,208)==USR_OK);
 struct sd64_key a=key('a'),h=key('h');
 uint64_t grant=0,other=0,id=0,value=0,req=0;
 const uint64_t total=SFS64_WINDOW_BYTES+17;
 uint8_t packet[NF102_PACKET_MAX];
 assert(nf102_start(&stream102,&queue)==PO64_OK&&nf102_start(&stream102,&queue)==PO64_INVALID);
 assert(nf85_grant_access(&nf,pid,192,786432,&a,4,9,PO64_READ|PO64_WRITE,&grant)==PO64_OK);
 assert(nf85_grant_read(&nf,peer,208,851968,&h,5,1,&other)==PO64_OK);
 assert(nf102_submit(&stream102,peer,208,851968,other,NF89_READ,0,1,&req)==PO64_OK);
 assert(nf102_call(&stream102,pid,192,786432,NF102_ROPEN,grant,0,0,&id)==PO64_BUSY&&!id);
 assert(nf102_pump(&stream102)&&nf102_pump(&stream102));
 size_t n=999;enum po64_status result=PO64_INVALID;
 assert(nf89_take(&queue,peer,208,851968,req,0,0,&n,&result)==PO64_OK&&!n&&result==PO64_OK);
 unsigned before=writes,rd_before=reads;
 assert(nf102_call(&stream102,pid,192,786432,NF102_ROPEN,grant,0,0,&id)==PO64_OK&&id);
 assert(writes==before&&reads==rd_before);
 assert(nf102_submit(&stream102,peer,208,851968,other,NF89_READ,0,1,&req)==PO64_BUSY);
 assert(nf102_call(&stream102,pid,192,786432,NF102_WOPEN,grant,0,total,&value)==PO64_BUSY);
 assert(nf102_pump(&stream102));
 for(uint64_t at=0;at<total;){
  assert(nf102_pump(&stream102));
  packet102(packet,at+1);
  assert(nf102_call(&stream102,pid,192,786432,NF102_RTAKE,id,packet,sizeof packet,&value)==PO64_INVALID&&!value);
  packet102(packet,at);
  rd_before=reads;
  assert(nf102_call(&stream102,peer,208,851968,NF102_RTAKE,id,packet,sizeof packet,&value)==PO64_DENIED&&!value);
  assert(nf102_call(&stream102,pid,192,786432,NF102_RTAKE,id,packet,sizeof packet,&value)==PO64_OK&&value&&value<=4096);
  assert(reads==rd_before&&writes==before);
  for(uint64_t i=0;i<value;++i)assert(packet[8+i]==byte(88,at+i));
  at+=value;
 }
 assert(nf102_call(&stream102,pid,192,786432,NF102_RRESULT,id,0,0,&value)==PO64_OK&&value==PO64_OK);
 assert(nf102_call(&stream102,pid,192,786432,NF102_RRESULT,id,0,0,&value)==PO64_DENIED);
 assert(nf102_call(&stream102,pid,192,786432,NF102_WOPEN,grant,0,total,&id)==PO64_OK);
 assert(nf102_pump(&stream102));
 for(uint64_t at=0;at<total;){
  size_t bytes=total-at>4096?4096:(size_t)(total-at);
  packet102(packet,at);
  for(size_t i=0;i<bytes;++i)packet[8+i]=byte(93,at+i);
  before=writes;rd_before=reads;
  assert(nf102_call(&stream102,pid,192,786432,NF102_WFEED,id,packet,8+bytes,&value)==PO64_OK&&!value);
  assert(writes==before&&reads==rd_before);
  assert(nf102_pump(&stream102));
  at+=bytes;
 }
 assert(nf102_call(&stream102,pid,192,786432,NF102_WFINISH,id,0,0,&value)==PO64_OK);
 assert(nf102_pump(&stream102));
 assert(nf102_call(&stream102,pid,192,786432,NF102_WRESULT,id,0,0,&value)==PO64_OK&&value==PO64_OK);
 assert(po64_unmount(&service)==PO64_OK);reopen();check('a',4,10,total,93);check('h',5,1,0,0);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(nf85_grant_access(&nf,pid,192,786432,&a,4,10,PO64_WRITE,&grant)==PO64_OK);
 assert(nf102_call(&stream102,pid,192,786432,NF102_WOPEN,grant,0,total,&id)==PO64_OK);
 assert(nf102_pump(&stream102)&&service.busy);
 assert(nf102_reconcile(&stream102)==0&&stream102.write.idle==0);
 assert(pr_transition(&procs,pid,192,PR_FAULTED)==PR_OK);
 assert(nf102_reconcile(&stream102)==1&&!service.busy&&stream102.write.state==NF100_IDLE);
 assert(nf85_reconcile(&nf)==1);
 assert(po64_unmount(&service)==PO64_OK);reopen();check('a',4,10,total,93);
 puts("Native file 102: stream register transport, arbitration, exact offsets, durable reopen and fault cleanup PASS");
}
#endif
#endif
#endif
#endif
#endif
int main(void){
 attach();assert(dc64_provision(&b,7,l.dir,D)==DC64_OK);
 assert(ir64_provision(&b,7,l.inode,I,F,1)==IP64_OK);
 assert(pp64_provision(&b,&l,&w.pair,nodes,128,current,128,&pair)==PP64_OK);reopen();
 struct pattern p={31,UINT64_MAX,0};
 assert(put('a',2,0,513,&p)==PP64_OK);reopen();check('a',2,1,513,31);
 p.seed=44;assert(put('b',3,0,513,&p)==PP64_OK);reopen();check('b',3,1,513,44);
 assert(po64_mount(&service,&b,&l,&w,nodes,128,current,128)==PO64_DENIED);
 read_only();unsigned before=writes;
 assert(po64_mount(&service,&b,&l,&w,nodes,128,current,128)==PO64_OK);
#ifdef STYLEOS_NATIVE_FILE_85_TEST
 native_file85();
#endif
 struct sd64_key a=key('a'),k=key('b');uint64_t other=0;
 assert(po64_grant(&service,10,&a,3,1,&token)==PO64_DENIED);
 assert(po64_grant(&service,10,&a,2,2,&token)==PO64_STALE);
 assert(po64_grant(&service,10,&a,2,1,&token)==PO64_OK);
 assert(po64_grant(&service,20,&k,3,1,&other)==PO64_OK&&other!=token);
 p=(struct pattern){31,UINT64_MAX,0};
 assert(po64_read(&service,20,token,sink,&p)==PO64_DENIED&&!p.seen);
 assert(po64_read(&service,10,other,sink,&p)==PO64_DENIED);
 assert(po64_read(&service,10,token,reenter,&p)==PO64_OK&&p.seen==513);
 assert(po64_read(&service,10,token,cancel,0)==PO64_CANCELLED);
 p=(struct pattern){44,UINT64_MAX,0};
 assert(po64_read(&service,20,other,sink,&p)==PO64_OK&&p.seen==513);
 for(unsigned i=2;i<PO64_HANDLES;i++){uint64_t tmp;assert(po64_grant(&service,30,&a,2,1,&tmp)==PO64_OK);}
 uint64_t tmp=0;assert(po64_grant(&service,30,&a,2,1,&tmp)==PO64_FULL&&!tmp);
 assert(po64_revoke_owner(&service,30)==PO64_OK);
 assert(po64_revoke(&service,20,token)==PO64_DENIED);
 assert(po64_revoke(&service,10,token)==PO64_OK);
 assert(po64_read(&service,10,token,sink,&p)==PO64_DENIED);
 assert(po64_unmount(&service)==PO64_OK);
 assert(po64_read(&service,20,other,sink,&p)==PO64_STALE);
 assert(po64_mount(&service,&b,&l,&w,nodes,128,current,128)==PO64_OK);
 assert(po64_grant(&service,10,&a,2,1,&tmp)==PO64_OK&&tmp>other);
 assert(bs_revoke(&b,7,l.data)==BS_OK);
 assert(po64_read(&service,10,tmp,sink,&p)==PO64_STALE&&!service.active);
 assert(writes==before);
 /* Restore the grant using a cold attach. A valid metadata snapshot with
  * damaged file contents must not return a successful read. */
 reopen();read_only();
 assert(po64_mount(&service,&b,&l,&w,nodes,128,current,128)==PO64_OK);
 assert(po64_grant(&service,10,&a,2,1,&tmp)==PO64_OK);
 size_t len=0;struct sfs64_map m;
 assert(ip64_export(&b,7,l.inode,&w.pair.inode.root,2,&w.source,&len)==IP64_OK);
 assert(sfs64_map_decode(w.source.map,len,F,1,&m));
 struct sfs_extent64 extent;uint64_t local;size_t take;
 assert(sfs64_map_chunk(&m,0,1,&extent,&local,&take));
 disk[2+D+I+extent.first][0]^=1;
 assert(po64_read(&service,10,tmp,accept,0)==PO64_CORRUPT&&!service.active);
 assert(po64_read(&service,10,tmp,accept,0)==PO64_STALE);
 assert(writes==0);
 /* Cold attach restores intact durable bytes and RW grants. */
 reopen();
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,current,128)==PO64_INVALID);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(po64_grant(&service,10,&a,2,1,&tmp)==PO64_OK);
 p=(struct pattern){55,UINT64_MAX,0};before=writes;
 assert(po64_write(&service,10,tmp,513,source,&p)==PO64_DENIED&&writes==before);
 assert(po64_grant_access(&service,10,&a,2,1,PO64_WRITE,&token)==PO64_OK);
 assert(po64_read(&service,10,token,sink,&p)==PO64_DENIED);
 assert(po64_write(&service,20,token,513,source,&p)==PO64_DENIED&&writes==before);
 assert(po64_grant(&service,20,&k,3,1,&other)==PO64_OK);
 p.cancel=0;
 assert(po64_write(&service,10,token,513,source,&p)==PO64_CANCELLED);
 p.cancel=UINT64_MAX;
 assert(po64_write(&service,10,token,513,write_source,&p)==PO64_OK);
 assert(po64_read(&service,20,other,sink,&p)==PO64_DENIED);
 assert(po64_write(&service,10,token,513,source,&p)==PO64_DENIED);
 assert(po64_grant(&service,10,&a,2,1,&tmp)==PO64_STALE);
 assert(po64_grant_access(&service,10,&a,2,2,PO64_READ|PO64_WRITE,&token)==PO64_OK);
 p.seen=0;assert(po64_read(&service,10,token,sink,&p)==PO64_OK&&p.seen==513);
 assert(po64_grant(&service,20,&k,3,1,&other)==PO64_OK);
 p=(struct pattern){44,UINT64_MAX,0};
 assert(po64_read(&service,20,other,sink,&p)==PO64_OK&&p.seen==513);
 fail=writes+1;p.seed=66;
 assert(po64_write(&service,10,token,513,source,&p)==PO64_UNCERTAIN&&!service.active);
 assert(po64_read(&service,20,other,sink,&p)==PO64_STALE);
 reopen();check('a',2,2,513,55);check('b',3,1,513,44);
 /* Stage 45: exact namespace snapshot; no old grants survive commits. */
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 struct po64_snapshot snap,old_snap;
 struct po64_row row,created;
 struct sd64_key c=key('c'),d=key('d');
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 old_snap=snap;before=writes;
 assert(po64_create_empty(&service,snap,&a,4)==PO64_DENIED&&writes==before);
 assert(po64_create_empty(&service,snap,&c,2)==PO64_DENIED&&writes==before);
 assert(po64_grant(&service,20,&k,3,1,&other)==PO64_OK);
 assert(po64_create_empty(&service,snap,&c,4)==PO64_OK);
 assert(po64_read(&service,20,other,accept,0)==PO64_DENIED);
 assert(po64_create_empty(&service,old_snap,&d,5)==PO64_STALE);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,1,2,&created)==PO64_OK);
 assert(created.entry.target==4&&created.revision==1&&created.bytes==0);
 assert(po64_child_at(&service,snap,1,0,&row)==PO64_OK&&row.entry.target==2);
 before=writes;
 assert(po64_change_file(&service,snap,&row,&k)==PO64_DENIED&&writes==before);
 row.revision++;
 assert(po64_change_file(&service,snap,&row,&d)==PO64_STALE&&writes==before);
 row.revision--;
 assert(po64_change_file(&service,snap,&row,&d)==PO64_OK);
 assert(po64_change_file(&service,snap,&created,0)==PO64_STALE);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,1,1,&created)==PO64_OK&&created.entry.target==4);
 assert(po64_change_file(&service,snap,&created,0)==PO64_OK);
 assert(po64_unmount(&service)==PO64_OK);
 reopen();check('d',2,2,513,55);check('b',3,1,513,44);
 read_only();assert(po64_mount(&service,&b,&l,&w,nodes,128,current,128)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);before=writes;
 assert(po64_create_empty(&service,snap,&c,5)==PO64_DENIED);
 assert(po64_child_at(&service,snap,1,2,&created)==PO64_END);
 assert(po64_child_at(&service,snap,1,0,&row)==PO64_OK);
 assert(po64_change_file(&service,snap,&row,0)==PO64_DENIED&&writes==before);
 assert(po64_unmount(&service)==PO64_OK);
 /* Stage 47: directories, cross-parent move, cycle and nonempty refusal. */
 reopen();
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 struct sd64_key folder=key('f'),inner=key('i'),moved=key('m'),nested=key('n');
 struct po64_row folder_row,inner_row,file_row;
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 before=writes;
 assert(po64_mkdir(&service,snap,&folder,2)==PO64_DENIED&&writes==before);
 assert(po64_mkdir(&service,snap,&folder,5)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,1,2,&folder_row)==PO64_OK&&folder_row.entry.target==5);
 assert(folder_row.entry.kind==SD64_DIRECTORY&&!folder_row.bytes&&!folder_row.revision);
 inner.parent=5;
 assert(po64_mkdir(&service,snap,&inner,6)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,5,0,&inner_row)==PO64_OK&&inner_row.entry.target==6);
 before=writes;
 nested.parent=6;
 assert(po64_change_directory(&service,snap,&folder_row,&nested)==PO64_DENIED&&writes==before);
 assert(po64_change_directory(&service,snap,&folder_row,0)==PO64_DENIED&&writes==before);
 assert(po64_child_at(&service,snap,1,1,&file_row)==PO64_OK&&file_row.entry.target==2);
 moved.parent=6;
 assert(po64_grant(&service,20,&k,3,1,&other)==PO64_OK);
 assert(po64_change_file(&service,snap,&file_row,&moved)==PO64_OK);
 assert(po64_read(&service,20,other,accept,0)==PO64_DENIED);
 assert(po64_change_directory(&service,snap,&inner_row,0)==PO64_STALE);
 assert(po64_unmount(&service)==PO64_OK);
 reopen();
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,6,0,&file_row)==PO64_OK);
 assert(file_row.entry.target==2&&file_row.bytes==513&&file_row.revision==2);
 assert(po64_grant(&service,10,&moved,2,2,&token)==PO64_OK);
 p=(struct pattern){55,UINT64_MAX,0};
 assert(po64_read(&service,10,token,sink,&p)==PO64_OK&&p.seen==513);
 assert(po64_child_at(&service,snap,5,0,&inner_row)==PO64_OK);
 before=writes;
 assert(po64_change_directory(&service,snap,&inner_row,0)==PO64_DENIED&&writes==before);
 nested.parent=1;
 assert(po64_change_directory(&service,snap,&inner_row,&nested)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,6,0,&file_row)==PO64_OK&&file_row.entry.target==2);
#ifdef STYLEOS_EXPLORER_111_TEST
 {
  struct ex111_scope scope={0};
  struct po64_row actual={0},saved={0};
  uint32_t peer=0;
  for(unsigned i=0;i<PR_MAX_PROCESSES;++i)
   if(users.owners[i].session_id==2)peer=users.owners[i].pid;
  assert(peer&&usr_select(&users,2)==USR_OK);
  unsigned initial_writes=writes;
  assert(ex111_open(&scope,&service,&users,peer,33,131072,5)==PO64_DENIED&&!scope.active);
  assert(ex111_open(&scope,&service,&users,peer,32,131072,5)==PO64_OK);
  assert(ex111_open(&scope,&service,&users,peer,32,131072,1)==PO64_INVALID);
  assert(ex111_up(&scope,peer,32,131072)==PO64_DENIED);
  assert(ex111_row(&scope,peer,32,131072,0,&actual)==PO64_OK);
  assert(actual.entry.target==6&&actual.entry.kind==SD64_DIRECTORY);
  memcpy(&saved,&actual,sizeof saved);
  assert(ex111_row(&scope,peer,32,135168,0,&actual)==PO64_DENIED&&!memcmp(&actual,&saved,sizeof actual));
  assert(ex111_row(&scope,peer,32,131072,1,&actual)==PO64_END&&!memcmp(&actual,&saved,sizeof actual));
  assert(ex111_enter(&scope,peer,32,131072,0)==PO64_OK);
  assert(ex111_row(&scope,peer,32,131072,0,&actual)==PO64_OK&&actual.entry.target==2&&actual.bytes==513&&actual.revision==2);
  assert(ex111_enter(&scope,peer,32,131072,0)==PO64_DENIED);
  assert(usr_lock(&users)==USR_OK);
  assert(ex111_row(&scope,peer,32,131072,0,&actual)==PO64_DENIED);
  assert(usr_select(&users,2)==USR_OK);
  assert(ex111_up(&scope,peer,32,131072)==PO64_OK);
  assert(ex111_up(&scope,peer,32,131072)==PO64_DENIED);
  assert(writes==initial_writes);
  assert(po64_create_empty(&service,snap,&(struct sd64_key){.parent=5,.length=1,.name={'x'}},7)==PO64_OK);
  assert(ex111_row(&scope,peer,32,131072,0,&actual)==PO64_STALE);
  ex111_close(&scope);
  assert(po64_snapshot(&service,&snap)==PO64_OK);
  assert(ex111_open(&scope,&service,&users,peer,32,131072,5)==PO64_OK);
  assert(ex111_row(&scope,peer,32,131072,1,&actual)==PO64_OK&&actual.entry.target==7);
  ex111_close(&scope);
  assert(po64_change_file(&service,snap,&actual,0)==PO64_OK);
  assert(po64_snapshot(&service,&snap)==PO64_OK);
  puts("EXPLORER111 real scoped directory navigation, owner denial, mutation refresh PASS");
 }
#endif

 assert(po64_change_file(&service,snap,&file_row,&d)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,1,3,&inner_row)==PO64_OK&&inner_row.entry.target==6);
 assert(po64_change_directory(&service,snap,&inner_row,0)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,1,2,&folder_row)==PO64_OK&&folder_row.entry.target==5);
 assert(po64_change_directory(&service,snap,&folder_row,0)==PO64_OK);
 assert(po64_unmount(&service)==PO64_OK);
 reopen();check('d',2,2,513,55);check('b',3,1,513,44);
 read_only();assert(po64_mount(&service,&b,&l,&w,nodes,128,current,128)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);before=writes;
 assert(po64_child_at(&service,snap,1,2,&folder_row)==PO64_END);
 assert(po64_mkdir(&service,snap,&folder,7)==PO64_DENIED&&writes==before);
 assert(po64_unmount(&service)==PO64_OK);
 /* Stage 48: multi-window copy, independent destination, corruption refusal. */
 reopen();
 p=(struct pattern){77,UINT64_MAX,0};
 const uint64_t big=SFS64_WINDOW_BYTES+17;
 assert(put('g',7,0,big,&p)==PP64_OK);
 reopen();
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 struct sd64_key g=key('g'),h=key('h'),j=key('j'),z=key('z');
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,1,2,&file_row)==PO64_OK&&file_row.entry.target==7);
 before=writes;
 assert(po64_copy_file(&service,snap,&file_row,&k,8)==PO64_DENIED&&writes==before);
 assert(po64_copy_file(&service,snap,&file_row,&h,2)==PO64_DENIED&&writes==before);
 file_row.bytes++;
 assert(po64_copy_file(&service,snap,&file_row,&h,8)==PO64_STALE&&writes==before);
 file_row.bytes--;
 assert(po64_grant(&service,10,&g,7,1,&token)==PO64_OK);
 assert(po64_copy_file(&service,snap,&file_row,&h,8)==PO64_OK);
 assert(po64_read(&service,10,token,accept,0)==PO64_DENIED);
 assert(po64_copy_file(&service,snap,&file_row,&j,9)==PO64_STALE);
 assert(po64_unmount(&service)==PO64_OK);
 reopen();check('g',7,1,big,77);check('h',8,1,big,77);
 /* A write to a copy must not modify the source. */
 p=(struct pattern){88,UINT64_MAX,0};
 assert(put('h',8,1,513,&p)==PP64_OK);reopen();
 check('g',7,1,big,77);check('h',8,2,513,88);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_create_empty(&service,snap,&z,9)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,1,4,&created)==PO64_OK&&created.entry.target==9);
 assert(po64_copy_file(&service,snap,&created,&j,10)==PO64_OK);
 assert(po64_unmount(&service)==PO64_OK);
 reopen();check('z',9,1,0,0);check('j',10,1,0,0);
 /* Corrupt the final window of the source, not its metadata. A provisional
  * first window may have been written, but no destination is published. */
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,1,2,&file_row)==PO64_OK&&file_row.entry.target==7);
 assert(ip64_export(&b,7,l.inode,&w.pair.inode.root,7,&w.source,&len)==IP64_OK);
 assert(sfs64_map_decode(w.source.map,len,F,1,&m));
 assert(sfs64_map_chunk(&m,big-1,1,&extent,&local,&take));
 disk[2+D+I+extent.first+local/512][local%512]^=1;
 struct sd64_key bad=key('q');uint64_t seq=snap.sequence;
 assert(po64_copy_file(&service,snap,&file_row,&bad,11)==PO64_CORRUPT&&!service.active);
 reopen();assert(pair.sequence==seq);
 uint64_t absent=0;uint8_t absent_kind=0;
 assert(sd64_lookup(&b,7,l.dir,&w.pair.dir.root,&bad,w.pair.dw.page,&absent,&absent_kind)==SD64_MISSING);
 check('h',8,2,513,88);
 /* Stage 49: explicitly new ID-managed volume, not an in-place upgrade. */
 memset(disk,0,sizeof disk);memset(durable,0,sizeof durable);attach();
 assert(dc64_provision(&b,7,l.dir,D)==DC64_OK);
 assert(ir64_provision(&b,7,l.inode,I,F,1)==IP64_OK);
 assert(pp64_provision_ids(&b,&l,&w.pair,nodes,128,current,128,&pair)==PP64_OK);
 reopen();assert(pair.id_floor==1);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);before=writes;
 assert(po64_create_empty(&service,snap,&a,8)==PO64_DENIED&&writes==before);
 assert(po64_create_empty(&service,snap,&a,0)==PO64_OK&&service.pair.id_floor==2);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,1,0,&row)==PO64_OK&&row.entry.target==2);
 assert(po64_change_file(&service,snap,&row,0)==PO64_OK&&service.pair.id_floor==2);
 assert(po64_unmount(&service)==PO64_OK);reopen();assert(pair.id_floor==2);
 assert(po64_mount_rw(&service,&b,&l,&w,nodes,128,current,previous,128)==PO64_OK);
 assert(po64_snapshot(&service,&snap)==PO64_OK);before=writes;
 assert(po64_create_empty(&service,snap,&a,2)==PO64_DENIED&&writes==before);
 assert(po64_mkdir(&service,snap,&folder,0)==PO64_OK&&service.pair.id_floor==3);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_create_empty(&service,snap,&a,0)==PO64_OK&&service.pair.id_floor==4);
 assert(po64_snapshot(&service,&snap)==PO64_OK);
 assert(po64_child_at(&service,snap,1,0,&row)==PO64_OK&&row.entry.target==4);
 assert(po64_copy_file(&service,snap,&row,&h,0)==PO64_OK&&service.pair.id_floor==5);
 assert(po64_unmount(&service)==PO64_OK);reopen();assert(pair.id_floor==5);
 check('a',4,1,0,0);check('h',5,1,0,0);
 /* Damage to latest receipt is not allowed to roll the watermark back. */
 disk[pair.slot][112]^=1;
 assert(pp64_mount(&b,&l,&w.pair,nodes,128,current,128,&pair)==PP64_CORRUPT);
 reopen();assert(pair.id_floor==5);
#ifdef STYLEOS_NATIVE_FILE_86_TEST
 native_file86();
#ifdef STYLEOS_NATIVE_FILE_87_TEST
 native_file87();
#ifdef STYLEOS_NATIVE_FILE_89_TEST
 native_file89();
#ifdef STYLEOS_NATIVE_FILE_98_TEST
 write98();
#ifdef STYLEOS_NATIVE_FILE_99_TEST
 write99();
#ifdef STYLEOS_NATIVE_FILE_100_TEST
 write100();
#ifdef STYLEOS_NATIVE_FILE_101_TEST
 stream_read101();
#ifdef STYLEOS_NATIVE_FILE_102_TEST
 stream_test102();
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
 puts("STYLEFS 49 ID SERVICE PASS: explicit v2, auto allocate, no reuse after unlink and cold boot, folder and copy, damaged receipt refusal");
 puts("STYLEFS 48 COPY PASS: 32KiB+17, exact source, independent data, zero file, CRC rejection, no partial publication");
 puts("STYLEFS 47 DIRECTORY SERVICE PASS: mkdir, cross-parent move, cold reopen, cycle and nonempty refusal, empty rmdir");
 puts("STYLEFS 45 NAMESPACE SERVICE PASS: create, rename, unlink, stale snapshot, no overwrite, token invalidation, cold reopen, RO denial");
 puts("STYLEFS 36 OBJECT WRITE PASS: authorized replacement, readonly/wrong principal denial, revocation, cancellation, uncertain close");
 puts("STYLEFS 35 OBJECT SERVICE PASS: exact revision, principal isolation, revoke, reentry, stale grant, readonly media");
 return 0;
}
