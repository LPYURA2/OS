/* Included only in the separately selected post-EBS test profile.
 * Existing safe-root/PCI/DMA guard remains authoritative. No general grant. */
#include <styleos/ata_runtime_48.h>
#include <styleos/stylefs_paged_pair64.h>
#define S34_N (UINT64_C(4096)<<21)
#define S34_CAP UINT64_C(512)
#define S34_START (S34_N-S34_CAP)
#define S34_D UINT64_C(66)
#define S34_I UINT64_C(66)
#define S34_F (S34_CAP-2-S34_D-S34_I)
#define S34_BYTES 1041u
#ifdef STYLEOS_LIVE_VOLUME_39
#include <styleos/stylefs_paged_volume64.h>
#include <styleos/desktop_paged_view.h>
#include <styleos/native_tea_stylefs.h>
#if defined(STYLEOS_LIVE_VOLUME_39) && defined(STYLEOS_OWNED_SHELL_865)
#include <styleos/native_file_85.h>
#include "native_executor_865.h"
#endif
#include <styleos/native_runtime_865.h>
#ifdef STYLEOS_LIVE_VOLUME_50
#define S34_SERIAL "STYLEOS-SFS50-DATA  "
#define S34_TAG "STYLEOS-SFS50-PRIVATE-V2"
#elif defined(STYLEOS_LIVE_VOLUME_46)
#define S34_SERIAL "STYLEOS-SFS46-DATA  "
#define S34_TAG "STYLEOS-SFS46-PRIVATE-V1"
#elif defined(STYLEOS_LIVE_VOLUME_44)
#define S34_SERIAL "STYLEOS-SFS44-DATA  "
#define S34_TAG "STYLEOS-SFS44-PRIVATE-V1"
#else
#define S34_SERIAL "STYLEOS-SFS39-DATA  "
#define S34_TAG "STYLEOS-SFS39-PRIVATE-V1"
#endif
#elif defined(STYLEOS_OBJECT_GUEST_37)
#include <styleos/stylefs_paged_object64.h>
#define S34_SERIAL "STYLEOS-SFS37-DATA  "
#define S34_TAG "STYLEOS-SFS37-PRIVATE-V1"
#else
#define S34_SERIAL "STYLEOS-SFS34-DATA  "
#define S34_TAG "STYLEOS-SFS34-PRIVATE-V1"
#endif
enum s34_action { S34_IDENT,S34_READ,S34_WRITE,S34_FLUSH };
struct s34_guard {
 struct ata857_live *live; struct ata_runtime_48 ata;
 struct {unsigned r;uint8_t v;} seq[13];
 unsigned len,pos,words;enum s34_action action;
 bool issued,tagged,denied;
};
static bool s34_deny(struct s34_guard *g){g->denied=true;return false;}
static bool s34_live(struct s34_guard *g){
 if(!g||g->denied||g->live!=&disk863.s.live||!ata857_live_check(g->live))
  return g?s34_deny(g):false;
 return true;
}
static void s34_add(struct s34_guard *g,unsigned r,uint8_t v){
 g->seq[g->len].r=r;g->seq[g->len++].v=v;
}
static bool s34_begin(struct s34_guard *g,enum s34_action a,uint64_t lba){
 if(g->denied||g->pos!=g->len||(g->issued&&g->words!=(g->action==S34_FLUSH?0u:256u)))return s34_deny(g);
 if((a==S34_WRITE||a==S34_FLUSH)&&!g->tagged)return s34_deny(g);
 if(a==S34_WRITE&&!(lba>=S34_START&&lba<S34_N))return s34_deny(g);
 if(a==S34_READ&&lba!=0&&!(lba>=S34_START&&lba<S34_N))return s34_deny(g);
 if((a==S34_READ||a==S34_WRITE)&&g->ata.bound&&!g->ata.busy&&!g->ata.pending_flush&&!g->ata.quarantined){
  g->live->ops=0;
  if(!same_cache(&shell_cache863)||!s34_live(g))return s34_deny(g);
 }
 g->len=g->pos=g->words=0;g->issued=false;g->action=a;
 s34_add(g,8,2);s34_add(g,6,a==S34_IDENT?0xa0:0xe0);
 if(a==S34_IDENT){for(unsigned i=1;i<=5;i++)s34_add(g,i,0);s34_add(g,7,0xec);}
 else if(a==S34_FLUSH)s34_add(g,7,0xea);
 else{
  s34_add(g,1,0);s34_add(g,2,0);s34_add(g,3,(uint8_t)(lba>>24));
  s34_add(g,4,(uint8_t)(lba>>32));s34_add(g,5,(uint8_t)(lba>>40));
  s34_add(g,1,0);s34_add(g,2,1);s34_add(g,3,(uint8_t)lba);
  s34_add(g,4,(uint8_t)(lba>>8));s34_add(g,5,(uint8_t)(lba>>16));
  s34_add(g,7,a==S34_READ?0x24:0x34);
 }
 return true;
}
static bool s34_status(void *ctx,uint8_t *v){
 struct s34_guard *g=ctx;
 return v&&s34_live(g)&&ata857_alt(g->live,v);
}
static bool s34_reg(void *ctx,unsigned r,uint8_t v){
 struct s34_guard *g=ctx;
 if(!s34_live(g)||g->pos>=g->len||g->seq[g->pos].r!=r||g->seq[g->pos].v!=v)return s34_deny(g);
 uint16_t port=r==8?0x376:(uint16_t)(0x170+r);
 __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(port):"memory");
 ++g->pos;if(r==7)g->issued=true;return true;
}
static bool s34_rd(void *ctx,uint16_t *v){
 struct s34_guard *g=ctx;
 if(!v||!s34_live(g)||!g->issued||g->pos!=g->len||g->words>=256||
    (g->action!=S34_IDENT&&g->action!=S34_READ))return s34_deny(g);
 *v=in16(0x170);++g->words;return true;
}
static bool s34_wr(void *ctx,uint16_t v){
 struct s34_guard *g=ctx;
 if(!s34_live(g)||!g->issued||g->pos!=g->len||g->words>=256||g->action!=S34_WRITE||!g->tagged)return s34_deny(g);
 out16(0x170,v);++g->words;return true;
}
static bool s34_pause(void *ctx){
 struct s34_guard *g=ctx;if(!s34_live(g))return false;
 for(unsigned i=0;i<256;i++)__asm__ volatile("pause":::"memory");
 return true;
}
static const struct ata_runtime_ops_863 s34_ops={{s34_status,s34_reg,s34_rd},s34_wr,s34_pause};
static bool s34_read(void *ctx,uint64_t lba,uint8_t *p){
 struct s34_guard *g=ctx;
 return g&&g->tagged&&lba<S34_CAP&&s34_begin(g,S34_READ,S34_START+lba)&&ata48_read(&g->ata,S34_START+lba,p);
}
static bool s34_write(void *ctx,uint64_t lba,const uint8_t *p){
 struct s34_guard *g=ctx;
 return g&&lba<S34_CAP&&s34_begin(g,S34_WRITE,S34_START+lba)&&ata48_write(&g->ata,S34_START+lba,p);
}
static bool s34_flush(void *ctx){
 struct s34_guard *g=ctx;
 return g&&s34_begin(g,S34_FLUSH,0)&&ata48_flush(&g->ata);
}


#define S34_ROUND(n) (((n)+4095u)&~(size_t)4095u)
struct s34_work {
 struct pp64_stream_workspace stream;
 struct dc64_node nodes[32];
 struct sfs64_run current[32],previous[32];
#ifdef STYLEOS_LIVE_VOLUME_39
 struct pv64_volume volume;
#ifdef STYLEOS_OWNED_SHELL_865
 struct nf85_service native_files;
 struct nf89_queue native_queue;
#endif
 struct s34_guard guard;
 uint64_t token;
 unsigned observations;
 struct dp41_view root_view;
 uint64_t ancestors[DC64_PATH_MAX+1];
 unsigned depth;
#endif
#ifdef STYLEOS_OBJECT_GUEST_37
 struct po64_service object;
#endif
};
static _Alignas(4096) union {
 struct s34_work value;
 uint8_t pages[S34_ROUND(sizeof(struct s34_work))];
} s34_storage;
_Static_assert(sizeof s34_storage%4096==0,"whole pages");
#define s34_work (s34_storage.value)
static _Noreturn void s34_end(const char *s){
 arch_puts(s);arch_putc('\n');arch_halt();for(;;)__asm__ volatile("hlt");
}
static uint8_t s34_byte(uint64_t i){return (uint8_t)(i*37u+i/512u*91u+13u);}
static bool s34_source(void *ctx,uint64_t at,uint8_t *p,size_t n){
 (void)ctx;for(size_t i=0;i<n;i++)p[i]=s34_byte(at+i);return true;
}
static bool s34_cancel(void *ctx,uint64_t at,uint8_t *p,size_t n){(void)ctx;(void)at;(void)p;(void)n;return false;}
static bool s34_sink(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 uint64_t *seen=ctx;if(*seen!=at)return false;
 for(size_t i=0;i<n;i++)if(p[i]!=s34_byte(at+i))return false;
 *seen+=n;return true;
}
static struct sd64_key s34_key(uint64_t parent,uint8_t name){
 struct sd64_key k={.parent=parent,.length=1};k.name[0]=name;return k;
}
static bool s34_check(struct bs_service *svc,const struct pp64_layout *l){
 struct sd64_key k=s34_key(2,'b');uint64_t seen=0;
 return pp64_get(svc,l,&s34_work.stream,s34_work.nodes,32,s34_work.current,32,4,&k,3,1,s34_sink,&seen)==PP64_OK&&seen==S34_BYTES;
}
#ifdef STYLEOS_OBJECT_GUEST_37
static bool s37_source(void *ctx,uint64_t at,uint8_t *p,size_t n){
 (void)ctx;for(size_t i=0;i<n;i++)p[i]=s34_byte(at+i)^0xffu;return true;
}
static bool s37_sink(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 uint64_t *seen=ctx;if(*seen!=at)return false;
 for(size_t i=0;i<n;i++)if(p[i]!=(uint8_t)(s34_byte(at+i)^0xffu))return false;
 *seen+=n;return true;
}
static void s37_mount(struct bs_service *svc,const struct pp64_layout *l){
 if(po64_mount_rw(&s34_work.object,svc,l,&s34_work.stream,s34_work.nodes,32,
  s34_work.current,s34_work.previous,32)!=PO64_OK)fail("sfs37-mount");
}
static void s37_read_new(void){
 struct po64_service *s=&s34_work.object;struct sd64_key k=s34_key(2,'b');
 uint64_t t=0,seen=0;
 if(po64_grant(s,101,&k,3,2,&t)!=PO64_OK||
    po64_read(s,202,t,s37_sink,&seen)!=PO64_DENIED||seen||
    po64_read(s,101,t,s37_sink,&seen)!=PO64_OK||seen!=S34_BYTES||
    po64_revoke_owner(s,101)!=PO64_OK||po64_read(s,101,t,s37_sink,&seen)!=PO64_DENIED)fail("sfs37-read-new");
}
static void s37_replace(struct bs_service *svc,const struct pp64_layout *l){
 s37_mount(svc,l);
 struct po64_service *s=&s34_work.object;struct sd64_key k=s34_key(2,'b');
 uint64_t t=0,ro=0,seen=0;
 if(po64_grant(s,202,&k,3,1,&ro)!=PO64_OK||
    po64_grant_access(s,101,&k,3,1,PO64_READ|PO64_WRITE,&t)!=PO64_OK||
    po64_write(s,202,ro,S34_BYTES,s37_source,0)!=PO64_DENIED||
    po64_write(s,202,t,S34_BYTES,s37_source,0)!=PO64_DENIED||
    po64_read(s,101,t,s34_sink,&seen)!=PO64_OK||seen!=S34_BYTES||
    po64_write(s,101,t,S34_BYTES,s34_cancel,0)!=PO64_CANCELLED||
    po64_write(s,101,t,S34_BYTES,s37_source,0)!=PO64_OK||
    po64_read(s,202,ro,s34_sink,&seen)!=PO64_DENIED||
    po64_grant(s,101,&k,3,1,&t)!=PO64_STALE)fail("sfs37-replace");
 s37_read_new();
 if(po64_unmount(s)!=PO64_OK)fail("sfs37-unmount");
}
#endif
#ifdef STYLEOS_LIVE_VOLUME_39
/* Long-lived backend and volume are in the attested writable image, never
 * on the boot stack. Read-only grants; no legacy raw-sector exposure. */
#ifdef STYLEOS_OWNED_SHELL_865
/* Borrow the existing live object service: no second mount, no new grants
 * to blocks and no file capability issued merely because a process exists.
 * Called only at serialized safe-root supervisor/retirement boundaries. */
void s88_native_file_reconcile(void){
 struct nf85_service *s=&s34_work.native_files;
 struct nf89_queue *q=&s34_work.native_queue;
 if(!s->ready){
  if(!s34_work.volume.attached||!s34_work.volume.objects.active)return;
  const struct usr_runtime *u=usr_global();
  if(!u||!u->ready)return;
  if(nf85_start(s,&s34_work.volume.objects,u)!=PO64_OK)return;
  arch_puts("STYLEOS NATIVE FILE88 READY\n");
 }
 (void)nf85_reconcile(s);
 if(!q->files){
  if(!s34_work.volume.objects.active||nf89_start(q,s)!=PO64_OK)return;
  arch_puts("STYLEOS NATIVE FILE90 QUEUE-READY\n");
 }
 (void)nf89_reconcile(q);
}
/* Only the safe supervisor poll may run one bounded request. No user
 * callback or disk I/O in the trap continuation. A transaction can still
 * block the supervisor on slow I/O; not a preemptible disk worker yet. */
void s90_native_file_pump(void){
 if(!s865_executor_idle())return;
 if(s34_work.native_queue.files)(void)nf89_pump(&s34_work.native_queue);
}
/* Trusted callers only. Queue owns copies; no app VAs or direct syscall. */
enum po64_status s90_native_file_submit(uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t token,enum nf89_op op,const uint8_t *data,size_t bytes,uint64_t *request){
 if(!s865_executor_idle())return PO64_DENIED;
 if(!s34_work.native_queue.files)return PO64_STALE;
 return nf89_submit(&s34_work.native_queue,pid,handle,root,token,op,data,bytes,request);
}
enum po64_status s90_native_file_take(uint32_t pid,uint64_t handle,uint64_t root,
 uint64_t request,uint8_t *out,size_t capacity,size_t *bytes,enum po64_status *result){
 if(!s865_executor_idle())return PO64_DENIED;
 if(!s34_work.native_queue.files)return PO64_STALE;
 return nf89_take(&s34_work.native_queue,pid,handle,root,request,out,capacity,bytes,result);
}
enum po64_status s90_native_file_cancel(uint32_t pid,uint64_t handle,uint64_t root,uint64_t request){
 if(!s865_executor_idle())return PO64_DENIED;
 if(!s34_work.native_queue.files)return PO64_STALE;
 return nf89_cancel(&s34_work.native_queue,pid,handle,root,request);
}
/* Trusted picker/launcher entry, not reachable from application code.
 * Exact row and rights must already be approved by the supervisor;
 * no selection, authorization or raw path lookup occurs here.
 * Synchronous grant validates the exact underlying revision. */
enum po64_status s88_native_file_grant(uint32_t pid,uint64_t handle,uint64_t root,
 const struct po64_row *row,unsigned rights,uint64_t *token){
 if(!row||row->entry.kind!=SD64_FILE||!s865_executor_idle())return PO64_DENIED;
 s88_native_file_reconcile();
 if(!s34_work.native_files.ready||!s34_work.volume.objects.active)return PO64_STALE;
 return nf85_grant_access(&s34_work.native_files,pid,handle,root,&row->entry.key,
                          row->entry.target,row->revision,rights,token);
}
#endif
static void s39_read(void){
 struct po64_service *s=&s34_work.volume.objects;
 if(!s->active)return;
 uint64_t seen=0;
 if(po64_read(s,101,s34_work.token,s34_sink,&seen)!=PO64_OK||seen!=S34_BYTES){
  (void)pv64_close(&s34_work.volume);
  arch_puts("SFS39 UNAVAILABLE\n");return;
 }
 if(po64_read(s,202,s34_work.token,s34_sink,&seen)!=PO64_DENIED)fail("sfs39-principal");
 if(s34_work.observations<2){
  ++s34_work.observations;
  arch_puts(s34_work.observations==1?"SFS39 DESKTOP READ PASS\n":"SFS39 INPUT READ PASS\n");
 }
}
const struct dp41_view *s41_root_view(void){
 return s34_work.volume.objects.active&&s34_work.root_view.valid?&s34_work.root_view:0;
}
static bool s42_load(uint64_t parent,uint64_t first){
 struct dp41_view *v=&s34_work.root_view;
 struct po64_service *s=&s34_work.volume.objects;
 struct po64_snapshot snap;
 v->valid=false;v->count=0;v->more=false;v->selected=0;v->parent=parent;v->first=first;
 if(po64_snapshot(s,&snap)!=PO64_OK)return false;
 for(unsigned i=0;i<=DP41_ROWS;i++){
  struct po64_row row;
  if(first>UINT64_MAX-i)return false;
  enum po64_status rc=po64_child_at(s,snap,parent,first+i,&row);
  if(rc==PO64_END){v->valid=true;return true;}
  if(rc!=PO64_OK)return false;
  if(i==DP41_ROWS){v->more=true;v->valid=true;return true;}
  v->rows[v->count++]=row;
 }
 return false;
}
static bool s42_page(uint64_t parent,uint64_t first){
 if(s42_load(parent,first))return true;
 (void)pv64_close(&s34_work.volume);
 arch_puts("SFS42 UNAVAILABLE\n");return true;
}
bool s43_text_supported(const struct po64_row *row){
 if(!row||row->entry.kind!=SD64_FILE||row->bytes>64u*1024u)return false;
 unsigned n=row->entry.key.length;
 if(n<4||n>SD64_NAME)return false;
 const uint8_t *p=row->entry.key.name+n-4;
 return p[0]=='.'&&(p[1]|32)=='t'&&(p[2]|32)=='x'&&(p[3]|32)=='t';
}
struct s43_copy {char *out;size_t cap,used;};
static bool s43_sink(void *ctx,uint64_t at,const uint8_t *p,size_t n){
 struct s43_copy *c=ctx;
 if(at!=c->used||n>c->cap-c->used)return false;
 for(size_t i=0;i<n;i++)c->out[c->used+i]=(char)p[i];
 c->used+=n;return true;
}
bool s43_read_text(const struct po64_row *row,char *out,size_t cap,size_t *bytes){
 if(!s41_root_view()||!out||!bytes||!s43_text_supported(row)||row->bytes>cap)return false;
 struct po64_service *s=&s34_work.volume.objects;
 struct s43_copy copy={out,cap,0};
 uint64_t token=0;
 /* Principal is the trusted diagnostic shell, not an application PID.
  * Do not expose it or the token to user mode. */
 if(po64_grant(s,303,&row->entry.key,row->entry.target,row->revision,&token)!=PO64_OK)return false;
 enum po64_status rc=po64_read(s,303,token,s43_sink,&copy);
 if(s->active&&po64_revoke(s,303,token)!=PO64_OK)return false;
 if(rc!=PO64_OK||copy.used!=row->bytes)return false;
 *bytes=copy.used;return true;
}
/* Separate principal for an explicit, exact-revision package read.
 * Scratch never belongs to Explorer or a user process. */
static uint8_t s69_package_scratch[NTP_MAX_PACKAGE];
bool s69_use_tea_file(const struct po64_row *row,const struct nts_pin *pin,
 ntsfs_consumer consumer,void *ctx){
 if(!s41_root_view()||!row||!pin||row->entry.kind!=SD64_FILE||
    row->bytes!=pin->package_bytes)return false;
 struct po64_service *s=&s34_work.volume.objects;
 uint64_t token=0;
 if(po64_grant(s,404,&row->entry.key,row->entry.target,row->revision,&token)!=PO64_OK)return false;
 enum ntsfs_status status=ntsfs_use(s,404,token,pin,s69_package_scratch,consumer,ctx);
 if(s->active){
  if(po64_revoke(s,404,token)!=PO64_OK)fail("tea69-stylefs-revoke");
 }else (void)pv64_close(&s34_work.volume);
 if(status!=NTSFS_OK)arch_puts("STYLEOS TEA69 STYLEFS REFUSED\n");
 return status==NTSFS_OK;
}
#ifdef STYLEOS_LIVE_VOLUME_44
bool s44_writable(void){
 struct po64_service *s=&s34_work.volume.objects;
 return s41_root_view()&&s->writable;
}
struct s44_source_ctx {const char *text;size_t bytes;};
static bool s44_source(void *ctx,uint64_t at,uint8_t *out,size_t n){
 const struct s44_source_ctx *c=ctx;
 if(at>c->bytes||n>c->bytes-at)return false;
 for(size_t i=0;i<n;i++)out[i]=(uint8_t)c->text[(size_t)at+i];
 return true;
}
bool s44_save_text(struct po64_row *row,const char *text,size_t bytes){
 if(!s44_writable()||!row||!text||bytes>64u*1024u||
    !s43_text_supported(row)||row->revision==UINT64_MAX)return false;
 struct po64_service *s=&s34_work.volume.objects;
 struct s44_source_ctx source={text,bytes};
 uint64_t token=0;
 if(po64_grant_access(s,303,&row->entry.key,row->entry.target,row->revision,
                     PO64_WRITE,&token)!=PO64_OK)return false;
 enum po64_status rc=po64_write(s,303,token,bytes,s44_source,&source);
 if(rc!=PO64_OK){
  if(s->active)(void)po64_revoke(s,303,token);
  else (void)pv64_close(&s34_work.volume);
  return false;
 }
 /* A durable commit invalidates every old token. Reauthorize the independent
  * diagnostic object and rebuild the Explorer snapshot, never reuse a grant. */
 row->revision++;row->bytes=bytes;
 struct sd64_key k=s34_key(1,'a');
 uint64_t parent=s34_work.root_view.parent,first=s34_work.root_view.first;
 if(po64_grant(s,101,&k,2,1,&s34_work.token)!=PO64_OK||
    !s42_load(parent,first)){
  (void)pv64_close(&s34_work.volume);
  arch_puts("SFS44 POSTCOMMIT VIEW UNAVAILABLE\n");
 }
 return true;
}
#endif
#ifdef STYLEOS_LIVE_VOLUME_46
/* Private diagnostic fixture: IDs 1..4 provisioned; new IDs use the
 * high half and next committed sequence. Not a general rollback-safe allocator. */
static struct {
 unsigned op,n;
 uint64_t parent;
 struct po64_snapshot snap;
 struct po64_row row;
 char name[SD64_NAME+1];
 const char *message;
} s46;
void s46_cancel(void){s46.op=0;s46.n=0;s46.name[0]=0;s46.message=0;}
const char *s46_footer(void){
 if(s46.op==3)return "Удалить файл? Enter — да, Esc — отмена";
 if(s46.op==1||s46.op==2)return s46.n?s46.name:"Введите имя — Enter / Esc";
 return s46.message?s46.message:"Ctrl+N — создать / Ctrl+R — имя / Del";
}
static bool s46_begin(unsigned op){
 const struct dp41_view *v=s41_root_view();
 if(!v||!s44_writable())return false;
 s46_cancel();
 if(op!=1&&(!v->count||v->selected>=v->count||
    v->rows[v->selected].entry.kind!=SD64_FILE||v->rows[v->selected].entry.target==2)){
  s46.message="Выберите обычный файл";return true;
 }
 if(po64_snapshot(&s34_work.volume.objects,&s46.snap)!=PO64_OK)return false;
 s46.parent=v->parent;
 if(op!=1)s46.row=v->rows[v->selected];
 s46.op=op;
 return true;
}
bool s46_begin_create(void){return s46_begin(1);}
static bool s46_commit(uint64_t protected_id){
 struct po64_service *s=&s34_work.volume.objects;
 unsigned op=s46.op;
 enum po64_status rc=PO64_DENIED;
 if(!s44_writable()||s34_work.root_view.parent!=s46.parent||
    (op!=1&&s46.row.entry.target==protected_id)){
  s46_cancel();s46.message="Операция запрещена";arch_puts("SFS46 REFUSED\n");return true;
 }
 struct sd64_key key={.parent=s46.parent,.length=(uint16_t)s46.n};
 for(unsigned i=0;i<s46.n;i++)key.name[i]=(uint8_t)s46.name[i];
 if(op==1){
#ifdef STYLEOS_LIVE_VOLUME_50
  if(sd64_key_valid(&key))rc=po64_create_empty(s,s46.snap,&key,0);
#else
  if(s46.snap.sequence<UINT64_C(0x7fffffffffffffff)&&sd64_key_valid(&key))
   rc=po64_create_empty(s,s46.snap,&key,UINT64_C(0x8000000000000000)|(s46.snap.sequence+1));
#endif
 }else if(op==2&&sd64_key_valid(&key)){
  rc=po64_change_file(s,s46.snap,&s46.row,&key);
 }else if(op==3)rc=po64_change_file(s,s46.snap,&s46.row,0);
 uint64_t parent=s46.parent;
 s46_cancel();
 if(rc!=PO64_OK){
  if(!s->active)(void)pv64_close(&s34_work.volume);
  s46.message="Операция не выполнена";arch_puts("SFS46 REFUSED\n");return true;
 }
 struct sd64_key diagnostic=s34_key(1,'a');
 if(po64_grant(s,101,&diagnostic,2,1,&s34_work.token)!=PO64_OK||
    !s42_load(parent,0)){
  (void)pv64_close(&s34_work.volume);
  arch_puts("SFS46 POSTCOMMIT VIEW UNAVAILABLE\n");
 }
 s46.message=op==1?"Файл создан":op==2?"Имя изменено":"Файл удалён";
 arch_puts(op==1?"SFS46 CREATE PASS\n":op==2?"SFS46 RENAME PASS\n":"SFS46 DELETE PASS\n");
 return true;
}
bool s46_input(unsigned key,unsigned mods,uint32_t cp,uint64_t protected_id){
 if(s46.op){
  if(key==0x01){s46_cancel();return true;}
  if(!mods&&(key==0x1c||key==0x11c))return s46_commit(protected_id);
  if(s46.op==3)return true;
  if(!mods&&key==0x0e){if(s46.n)s46.name[--s46.n]=0;return true;}
  if(!(mods&(DESK_CTRL|DESK_ALT))&&cp>=32&&cp<127&&cp!='/'&&cp!='\\'&&s46.n<SD64_NAME){
   s46.name[s46.n++]=(char)cp;s46.name[s46.n]=0;
  }
  return true;
 }
 if(mods==DESK_CTRL&&key==0x31)return s46_begin(1);
 if(mods==DESK_CTRL&&key==0x13)return s46_begin(2);
 if(!mods&&key==0x153)return s46_begin(3);
 return false;
}
#endif
bool s42_select(unsigned row,bool open){
 struct dp41_view *v=&s34_work.root_view;
 if(!s41_root_view()||row>=v->count)return false;
 v->selected=row;
 if(!open)return true;
 if(v->rows[row].entry.kind==SD64_FILE){
  if(s43_text_supported(&v->rows[row]))(void)s43_open_editor(&v->rows[row]);
  else (void)s865_launch_native_file(&v->rows[row]);
  return true;
 }
 if(v->rows[row].entry.kind!=SD64_DIRECTORY)return true;
 if(s34_work.depth>=DC64_PATH_MAX)return true;
 uint64_t id=v->rows[row].entry.target;
 s34_work.ancestors[s34_work.depth++]=v->parent;
 bool changed=s42_page(id,0);
 if(v->valid)arch_puts("SFS42 ENTER DIRECTORY\n");
 return changed;
}
bool s42_navigate(unsigned key){
 struct dp41_view *v=&s34_work.root_view;
 if(!s41_root_view())return false;
 switch(key){
 case 0x148:if(v->selected)v->selected--;return true;
 case 0x150:if(v->selected+1<v->count)v->selected++;return true;
 case 0x1c:case 0x11c:return s42_select(v->selected,true);
 case 0x0e:
  if(!s34_work.depth)return false;
  {uint64_t parent=s34_work.ancestors[--s34_work.depth];
   bool changed=s42_page(parent,0);
   if(v->valid)arch_puts("SFS42 UP DIRECTORY\n");
   return changed;}
 case 0x149:if(v->first>=DP41_ROWS)return s42_page(v->parent,v->first-DP41_ROWS);return false;
 case 0x151:if(v->more&&v->first<=UINT64_MAX-DP41_ROWS)return s42_page(v->parent,v->first+DP41_ROWS);return false;
 default:return false;
 }
}
static void s39_admit(void){
 struct s34_guard *g=&s34_work.guard;
 g->live=&disk863.s.live;uint8_t buf[512];uint64_t gen=0;
 if(!s34_begin(g,S34_IDENT,0)||!ata48_bind(&g->ata,&s34_ops,g,S34_SERIAL,S34_N)){
  arch_puts("SFS39 REFUSE identity\n");return;
 }
 if(!s34_begin(g,S34_READ,0)||!ata48_read(&g->ata,0,buf)){arch_puts("SFS39 REFUSE tag-read\n");return;}
 static const char tag[]=S34_TAG;
 for(unsigned i=0;i<512;i++)if(buf[i]!=(i<sizeof(tag)-1?(uint8_t)tag[i]:0)){
  arch_puts("SFS39 REFUSE tag\n");return;
 }
 g->tagged=true;
 struct bs_service *svc=&disk863.s.block;
#ifdef STYLEOS_LIVE_VOLUME_44
 const bool writable=true;
 const struct bs_backend backend={g,UINT64_C(0x701080862844),S34_CAP,true,false,s34_read,s34_write,s34_flush};
#else
 const bool writable=false;
 const struct bs_backend backend={g,UINT64_C(0x701080862839),S34_CAP,false,false,s34_read,0,0};
#endif
 const struct pv64_geometry geometry={0,S34_CAP,S34_D,S34_I,1};
 if(bs_attach(svc,&backend,&gen)!=BS_OK){arch_puts("SFS39 REFUSE attach\n");return;}
 if(pv64_mount(&s34_work.volume,svc,gen,BLOCK_SHELL_OWNER,&geometry,writable,&s34_work.stream,
    s34_work.nodes,32,s34_work.current,writable?s34_work.previous:0,32)!=PO64_OK){
  (void)bs_detach(svc,gen);arch_puts("SFS39 REFUSE metadata\n");return;
 }
#ifdef STYLEOS_LIVE_VOLUME_50
 if(!s34_work.volume.objects.pair.id_floor){
  (void)pv64_close(&s34_work.volume);(void)bs_detach(svc,gen);arch_puts("SFS39 REFUSE metadata\n");return;
 }
#endif
 struct sd64_key k=s34_key(1,'a');
 if(po64_grant(&s34_work.volume.objects,101,&k,2,1,&s34_work.token)!=PO64_OK){
  (void)pv64_close(&s34_work.volume);(void)bs_detach(svc,gen);arch_puts("SFS39 REFUSE object\n");return;
 }
 if(!s42_load(1,0)){
  (void)pv64_close(&s34_work.volume);(void)bs_detach(svc,gen);
  arch_puts("SFS39 REFUSE directory\n");return;
 }
 arch_puts("SFS39 MOUNT PASS\n");
}
#endif
static _Noreturn void s34_run(void){
 s863_shell_guard();
 struct s34_guard g={.live=&disk863.s.live};
 uint8_t buf[512];uint64_t gen=0;
 if(!s34_begin(&g,S34_IDENT,0)||!ata48_bind(&g.ata,&s34_ops,&g,S34_SERIAL,S34_N))s34_end("SFS34 REFUSE identity");
 if(!s34_begin(&g,S34_READ,0)||!ata48_read(&g.ata,0,buf))fail("sfs34-tag-read");
 static const char tag[]=S34_TAG;
 for(unsigned i=0;i<512;i++)if(buf[i]!=(i<sizeof(tag)-1?(uint8_t)tag[i]:0))s34_end("SFS34 REFUSE tag");
 g.tagged=true;
 struct bs_service *svc=&disk863.s.block;
 const struct bs_backend backend={&g,UINT64_C(0x701080862834),S34_CAP,true,false,s34_read,s34_write,s34_flush};
 struct pp64_layout l={.owner=BLOCK_SHELL_OWNER,.dir_sectors=S34_D,.inode_sectors=S34_I,.data_sectors=S34_F,.data_start=1};
 if(bs_attach(svc,&backend,&gen)!=BS_OK||
    bs_grant_range(svc,gen,l.owner,0,2,3,&l.receipt)!=BS_OK||
    bs_grant_range(svc,gen,l.owner,2,S34_D,3,&l.dir)!=BS_OK||
    bs_grant_range(svc,gen,l.owner,2+S34_D,S34_I,3,&l.inode)!=BS_OK||
    bs_grant_range(svc,gen,l.owner,2+S34_D+S34_I,S34_F,3,&l.data)!=BS_OK)fail("sfs34-grants");
 unsigned ops=g.live->ops;
 if(bs_read_sector(svc,l.owner+1,l.data,0,buf)!=BS_DENIED||
    bs_write_sector(svc,l.owner,l.data,S34_F,buf)!=BS_RANGE||g.live->ops!=ops)fail("sfs34-denials");
 struct pp64_pair pair;
 enum pp64_status rc=pp64_mount(svc,&l,&s34_work.stream.pair,s34_work.nodes,32,s34_work.current,32,&pair);
 if(rc==PP64_CORRUPT)s34_end("SFS34 REFUSE metadata");
 if(rc!=PP64_OK)fail("sfs34-mount");
#ifdef STYLEOS_OBJECT_GUEST_37
 if(pair.sequence==5){
  s37_mount(svc,&l);s37_read_new();
  if(po64_unmount(&s34_work.object)!=PO64_OK)fail("sfs37-cold-unmount");
  s34_end("SFS34 OBJECT COLD PASS");
 }
#else
 if(pair.sequence==4&&s34_check(svc,&l))s34_end("SFS34 COLD PASS");
#endif
 if(pair.sequence!=1||s34_work.stream.pair.dir.count||s34_work.stream.pair.inode.root.files) s34_end("SFS34 REFUSE nonblank");
 struct dc64_edit e={.op=DC64_CREATE,.entry={s34_key(1,'d'),2,SD64_DIRECTORY}};
 if(pp64_edit(svc,&l,&s34_work.stream,s34_work.nodes,32,s34_work.current,32,1,&e,0,&pair)!=PP64_OK)fail("sfs34-mkdir");
 struct sd64_key k=s34_key(2,'a');
 if(pp64_put(svc,&l,&s34_work.stream,s34_work.nodes,32,s34_work.current,s34_work.previous,32,2,&k,3,0,S34_BYTES,s34_cancel,0,&pair)!=PP64_CANCELLED)fail("sfs34-cancel");
 if(pp64_put(svc,&l,&s34_work.stream,s34_work.nodes,32,s34_work.current,s34_work.previous,32,2,&k,3,0,S34_BYTES,s34_source,0,&pair)!=PP64_OK)fail("sfs34-put");
 e=(struct dc64_edit){.op=DC64_RENAME,.entry={k,3,SD64_FILE},.destination=s34_key(2,'b')};
 if(pp64_edit(svc,&l,&s34_work.stream,s34_work.nodes,32,s34_work.current,32,3,&e,1,&pair)!=PP64_OK||!s34_check(svc,&l))fail("sfs34-rename-read");
#ifdef STYLEOS_OBJECT_GUEST_37
 s37_replace(svc,&l);s863_shell_guard();s34_end("SFS34 OBJECT WRITE PASS");
#else
 s863_shell_guard();s34_end("SFS34 WRITE PASS");
#endif
}
