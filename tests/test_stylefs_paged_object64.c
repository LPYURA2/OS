#include <styleos/stylefs_paged_object64.h>
#ifdef STYLEOS_NATIVE_FILE_85_TEST
#include <styleos/native_file_85.h>
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
 assert(pr_admit(&procs,&spec,&peer)==PR_OK&&usr_bind(&users,z,peer,32)==USR_OK);
 assert(nf85_start(&nf,&service,&users)==PO64_OK);
 struct sd64_key k=key('a');uint64_t grant=0,other=0;
 unsigned before=writes;
 assert(nf85_grant_read(&nf,pid,17,65536,&k,2,1,&grant)==PO64_DENIED&&!grant);
 assert(nf85_grant_read(&nf,pid,16,65536,&k,3,1,&grant)==PO64_DENIED&&!grant);
 assert(nf85_grant_read(&nf,pid,16,65536,&k,2,1,&grant)==PO64_OK&&grant);
 uint8_t data[514];memset(data,0x5a,sizeof data);size_t n=999;
 assert(nf85_read(&nf,pid,16,65536,grant,data,512,&n)==PO64_FULL&&n==999);
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
 assert(pr_transition(&procs,pid,16,PR_FAULTED)==PR_OK);
 assert(nf85_reconcile(&nf)==1&&nf85_reconcile(&nf)==0);
 assert(po64_read(&service,UINT64_C(0x8000000000000000)|pid,grant,accept,0)==PO64_DENIED);
 assert(writes==before);
 puts("Native file 85: real StyleFS object, owner/session, bounded read and cleanup PASS");
}
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
 puts("STYLEFS 49 ID SERVICE PASS: explicit v2, auto allocate, no reuse after unlink and cold boot, folder and copy, damaged receipt refusal");
 puts("STYLEFS 48 COPY PASS: 32KiB+17, exact source, independent data, zero file, CRC rejection, no partial publication");
 puts("STYLEFS 47 DIRECTORY SERVICE PASS: mkdir, cross-parent move, cold reopen, cycle and nonempty refusal, empty rmdir");
 puts("STYLEFS 45 NAMESPACE SERVICE PASS: create, rename, unlink, stale snapshot, no overwrite, token invalidation, cold reopen, RO denial");
 puts("STYLEFS 36 OBJECT WRITE PASS: authorized replacement, readonly/wrong principal denial, revocation, cancellation, uncertain close");
 puts("STYLEFS 35 OBJECT SERVICE PASS: exact revision, principal isolation, revoke, reentry, stale grant, readonly media");
 return 0;
}
