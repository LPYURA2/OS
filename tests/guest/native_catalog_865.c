/* Acceptance-only external launch provider; never compiled into ordinary image. */
#include <styleos/native_runtime_865.h>
#include <styleos/arch.h>
#include <styleos/user_session_runtime.h>
#include <styleos/native_window_channel.h>
#include "../../boot/x86_64/native_executor_865.h"
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
#error catalog driver requires explicit acceptance profile
#endif
static void native_wipe(void *p,size_t n){volatile unsigned char *b=p;while(n--)*b++=0;}
static void field(const char *key,uint64_t v){arch_putc(' ');arch_puts(key);arch_puts("=0x");for(int j=60;j>=0;j-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)j)&15]);}
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
#include "native_catalog_digest.h"
/* Explicitly provisioned allowlist. Digest is trusted build input, not read
 * from the medium. No executable bytes or test scheduling policy linked.
 * All record bounds are checked before admitting even the first app. */
#define NC_FIRST 3000u
#define NC_SECTORS 4u
#define NC_RECORD 48u
static uint8_t catalog[NC_SECTORS*512];
static bool native_catalog_busy;
/* Only this acceptance translation unit sees the explicitly pinned stop catalog.
 * No automatic kill policy or key binding is added to the ordinary kernel. */
static unsigned stop_phase;
static uint32_t stop_pid,peer_pid;
static uint64_t stop_gen,peer_gen;
static uint32_t stop_session,stop_window,peer_window;
struct stop_owned {
 const struct usr_owner *owner;
 const struct ws_window *window;
 const struct nwc_port *port;
};
static struct usr_runtime *sessions(void){return (struct usr_runtime *)usr_global();}
static struct nwc_service *channels(void){return (struct nwc_service *)nwc_global();}
static bool clear_bytes(const void *p,size_t n){
 const unsigned char *b=p;for(size_t i=0;i<n;i++)if(b[i])return false;return true;
}
static bool owned_refs(uint32_t pid,uint32_t win,struct stop_owned *out){
 const struct usr_runtime *s=sessions();const struct nwc_service *c=channels();
 if(!s||!c||!out)return false;
 *out=(struct stop_owned){0};
 for(unsigned i=0;i<PR_MAX_PROCESSES;i++)if(s->owners[i].pid==pid)out->owner=&s->owners[i];
 for(unsigned i=0;i<NWC_PORTS;i++)if(c->ports[i].window_id==win)out->port=&c->ports[i];
 out->window=ws_find(ws_global(),win);
 return out->owner&&out->window&&out->port;
}
static bool owned_zero(const struct stop_owned *r){
 return clear_bytes(r->owner,sizeof *r->owner)&&clear_bytes(r->window,sizeof *r->window)&&
        clear_bytes(r->port,sizeof *r->port);
}
static bool input_probe,focus_probe,frame_probe;
static bool grant_owned(uint32_t pid,uint64_t gen,uint32_t *win){
 const struct pr_entry *p=pr_find(pr_global(),pid);
 struct usr_runtime *s=sessions();struct nwc_service *c=channels();
 if(!p||!s||!c||!win||p->scheduler_handle!=gen)return false;
 if(frame_probe){
  struct stop_owned owned;
  if(!owned_refs(pid,*win,&owned)||owned.owner->session_id!=stop_session||
     owned.owner->scheduler_handle!=gen||owned.port->owner_pid!=pid||
     owned.port->scheduler_handle!=gen||owned.port->user_root!=p->page_root||
     owned.port->length||owned.port->has_tile||owned.port->frame.count)return false;
 }else if(usr_bind(s,stop_session,pid,gen)!=USR_OK||
    ws_open_app(ws_global(),pr_global(),pid,"Stop fixture",(struct ws_bounds){0,0,160,120},win)!=WS_OK||
    nwc_claim(c,pid,gen,p->page_root,*win)!=NWC_OK)return false;
 /* Nonempty IPC queue and tile must be scrubbed, not just hidden. */
 uint32_t pixel=UINT32_C(0x00a00000)|pid;uint8_t data[2]={0x5a,(uint8_t)pid};
 const uint8_t probe[2]={0x5a,0x70};
 if(input_probe&&nwc_send_text(c,*win,probe,sizeof probe)!=NWC_OK)return false;
 return nwc_send_text(c,*win,data,sizeof data)==NWC_OK&&
        nwc_publish_tile(c,pid,gen,p->page_root,*win,&pixel,1,1)==NWC_OK;
}
static unsigned char peer_owner_copy[sizeof(struct usr_owner)];
static unsigned char peer_window_copy[sizeof(struct ws_window)];
static unsigned char peer_port_copy[sizeof(struct nwc_port)];
static void copy_bytes(unsigned char *dst,const void *src,size_t n){
 const unsigned char *p=src;for(size_t i=0;i<n;i++)dst[i]=p[i];
}
static bool same_bytes(const void *src,const unsigned char *expected,size_t n){
 const unsigned char *p=src;for(size_t i=0;i<n;i++)if(p[i]!=expected[i])return false;return true;
}
static void save_peer(const struct stop_owned *r){
 copy_bytes(peer_owner_copy,r->owner,sizeof *r->owner);
 copy_bytes(peer_window_copy,r->window,sizeof *r->window);
 copy_bytes(peer_port_copy,r->port,sizeof *r->port);
}
static bool same_peer(const struct stop_owned *r){
 return same_bytes(r->owner,peer_owner_copy,sizeof *r->owner)&&
        same_bytes(r->window,peer_window_copy,sizeof *r->window)&&
        same_bytes(r->port,peer_port_copy,sizeof *r->port);
}
static bool revoked(uint32_t pid,uint64_t gen,uint64_t root,uint32_t win,const struct stop_owned *r){
 struct nwc_event event={0};struct nwc_tile tile={0};size_t total=0;
 return owned_zero(r)&&!ws_find(ws_global(),win)&&
        usr_processes(sessions(),pid,0,0,&total)==USR_DENIED&&
        nwc_receive(channels(),pid,gen,root,win,&event)==NWC_MISSING&&
        nwc_visible_tile(channels(),win,&tile)==NWC_MISSING&&
        !nwc_visible_frame(channels(),win);
}
bool s863_disk_read(uint32_t,uint8_t[512]);
static bool catalog_read(void *unused,uint32_t lba,uint8_t out[512]){
 (void)unused;return lba>=3072u&&lba<4095u&&s863_disk_read(lba,out);
}
static uint32_t nc_u32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static bool nc_equal(const uint8_t *a,const uint8_t *b,size_t n){unsigned diff=0;for(size_t j=0;j<n;j++)diff|=a[j]^b[j];return !diff;}
/* Dedicated expanded fixture records. Only accepted under this explicitly
 * pinned test catalog, never in the ordinary kernel or generic installer. */
static bool wide_record(const uint8_t *r){
 static const uint8_t a[8]={'w','i','d','e','7','5',0,0},b[8]={'f','a','u','l','t','7','5',0};
 static const uint8_t x[8]="ipc76",y[8]="wide76";
 return nc_equal(r+40,a,8)||nc_equal(r+40,b,8)||nc_equal(r+40,x,8)||nc_equal(r+40,y,8);
}
/* Only this pinned three-program workload waits for a second F9. Keep the
 * admission identities so a recycled slot can never receive the release. */
static struct wide_owner {
 unsigned slot;
 uint32_t pid;
 uint64_t generation,root;
} wide_owners[3];
static bool wide_waiting,wide_stop76;
static bool wide_catalog(void){
 static const uint8_t names[3][8]={"wide75","fault75","peer75"};
 for(unsigned i=0;i<3;i++)
  if(!nc_equal(catalog+16+i*NC_RECORD+40,names[i],8))return false;
 return true;
}
static bool stop76_catalog(void){
 static const uint8_t names[3][8]={"ipc76","wide76","peer76"};
 for(unsigned i=0;i<3;i++)
  if(!nc_equal(catalog+16+i*NC_RECORD+40,names[i],8))return false;
 return true;
}
static bool remember_wide_owners(const struct pa_row *rows){
 for(unsigned i=0;i<3;i++){
  unsigned slot=0;
  while(slot<S846_ROOT_COUNT&&bank862.slots[slot].pid!=rows[i].pid)++slot;
  if(slot==S846_ROOT_COUNT)return false;
  const struct rt862_bank_slot *s=&bank862.slots[slot];
  wide_owners[i]=(struct wide_owner){slot,s->pid,s->handle,s->root};
 }
 wide_waiting=true;
 return true;
}
static bool release_wide_workload(void){
 volatile uint8_t *flags[3];
 uint64_t ticks[3];
 for(unsigned i=0;i<3;i++){
  const struct wide_owner *o=&wide_owners[i];
  const struct pr_entry *p=pr_find(pr_global(),o->pid);
  if(!p||p->state!=PR_READY||p->scheduler_handle!=o->generation||
     p->page_root!=o->root||p->cpu_ticks<2)return false;
  ticks[i]=p->cpu_ticks;
  uint64_t address=S846_DATA+(i==2?0u:2u*4096u)+8u;
  flags[i]=s865_native_data_span(o->slot,o->pid,o->generation,o->root,address,1);
  if(!flags[i]||*flags[i])return false;
 }
 /* Preflight all three before touching any flag. This serialized desktop
  * action only ends user work; LAPIC delivery and scheduling are ordinary. */
 for(unsigned i=0;i<3;i++)*flags[i]=1;
 wide_waiting=false;
 arch_puts("STYLEOS WIDE75 RELEASE");field("first",ticks[0]);field("second",ticks[1]);
 field("peer",ticks[2]);arch_putc('\n');
 return true;
}
/* Separate pinned acceptance: actual IPC into two private pages, then
 * trusted forced termination while two other processes remain runnable. */
static bool stop76_release(void){
 const struct wide_owner *a=&wide_owners[0],*b=&wide_owners[1],*z=&wide_owners[2];
 const struct pr_entry *p=pr_find(pr_global(),a->pid),*q=pr_find(pr_global(),b->pid),
                       *r=pr_find(pr_global(),z->pid);
 if(!p||!q||!r||p->state!=PR_READY||q->state!=PR_READY||r->state!=PR_READY||
    p->scheduler_handle!=a->generation||q->scheduler_handle!=b->generation||
    r->scheduler_handle!=z->generation||p->page_root!=a->root||
    q->page_root!=b->root||r->page_root!=z->root||
    p->cpu_ticks<2||q->cpu_ticks<2||r->cpu_ticks<2)return false;
 struct stop_owned target,peer;
 if(!owned_refs(a->pid,stop_window,&target)||!owned_refs(b->pid,peer_window,&peer)||
    target.port->length!=0||target.port->next_sequence!=2||
    peer.port->length!=1||!target.port->has_tile||!peer.port->has_tile)return false;
 uint8_t wire[64],guard[4],marker[4],neighbor[8];
 if(!s865_native_copy_data(a->slot,a->pid,a->generation,a->root,S846_DATA+4096u-32u,wire,sizeof wire)||
    !s865_native_copy_data(a->slot,a->pid,a->generation,a->root,S846_DATA+3u*4096u-4u,guard,sizeof guard)||
    !s865_native_copy_data(a->slot,a->pid,a->generation,a->root,S846_DATA+2u*4096u,marker,sizeof marker)||
    !s865_native_copy_data(b->slot,b->pid,b->generation,b->root,S846_DATA+2u*4096u,neighbor,sizeof neighbor))return false;
 const uint32_t expected[8]={1,1,stop_window,NWC_TEXT,0,0,0,2};
 for(unsigned i=0;i<8;++i)if(nc_u32(wire+4*i)!=expected[i])return false;
 if(wire[32]!=0x5a||wire[33]!=(uint8_t)a->pid||!clear_bytes(wire+34,30)||
    nc_u32(guard)!=0x11223344u||nc_u32(marker)!=0x66778899u||
    nc_u32(neighbor)!=0x1234567u||nc_u32(neighbor+4))return false;
 save_peer(&peer);
 const uint64_t ticks=q->cpu_ticks,zticks=r->cpu_ticks;
 const uint32_t serial=pr_global()->change_serial;
 if(s865_native_stop(a->pid,a->generation,false)!=PS_EVIDENCE||
    s865_native_stop(a->pid,b->generation,true)!=PS_EVIDENCE||
    pr_global()->change_serial!=serial)return false;
 if(s865_native_stop(a->pid,a->generation,true)!=PS_OK||
    s865_native_stop(a->pid,a->generation,true)!=PS_EVIDENCE||
    pr_find(pr_global(),a->pid)||!revoked(a->pid,a->generation,a->root,stop_window,&target)||
    !same_peer(&peer)||nwc_port_count(channels())!=1||
    pr_snapshot(pr_global(),0,0,true)!=2)return false;
 q=pr_find(pr_global(),b->pid);r=pr_find(pr_global(),z->pid);
 if(!q||!r||q->cpu_ticks!=ticks||r->cpu_ticks!=zticks||q->state!=PR_READY||
    r->state!=PR_READY||q->page_root!=b->root||r->page_root!=z->root||
    q->resident_bytes!=7u*4096u||r->resident_bytes!=3u*4096u)return false;
 uint8_t after[8];
 if(!s865_native_copy_data(b->slot,b->pid,b->generation,b->root,S846_DATA+2u*4096u,after,sizeof after)||
    !nc_equal(after,neighbor,sizeof after))return false;
 volatile uint8_t *bf=s865_native_data_span(b->slot,b->pid,b->generation,b->root,S846_DATA+2u*4096u+8u,1);
 volatile uint8_t *zf=s865_native_data_span(z->slot,z->pid,z->generation,z->root,S846_DATA+8u,1);
 if(!bf||!zf||*bf||*zf)return false;
 *bf=1;*zf=1;wide_waiting=false;
 arch_puts("STYLEOS WIDE76 STOP");field("pid",a->pid);field("gen",a->generation);
 field("root",a->root);field("peer",b->pid);field("legacy",z->pid);
 field("resident",2);field("bytes",q->resident_bytes+r->resident_bytes);
 field("wire",64);field("queue",peer.port->length);arch_putc('\n');
 return true;
}
static bool catalog_valid(unsigned *out){
 for(unsigned j=0;j<NC_SECTORS;j++)if(!s863_disk_read(NC_FIRST+j,catalog+512*j))return false;
 uint8_t digest[32];nte_sha256(catalog,sizeof catalog,digest);
 static const uint8_t magic[8]={'S','T','Y','L','N','A','T','1'};
 if(!nc_equal(digest,native_catalog_digest,32)||!nc_equal(catalog,magic,8)||nc_u32(catalog+8)!=1)return false;
 unsigned count=nc_u32(catalog+12);
 if(!count||count>S846_ROOT_COUNT||16+count*NC_RECORD>sizeof catalog)return false;
 for(unsigned j=0;j<count;j++){
  const uint8_t *r=catalog+16+j*NC_RECORD;
  uint32_t lba=nc_u32(r),bytes=nc_u32(r+4);
  if(lba<3072u||lba>=4095u||bytes<120u||
     bytes>(count==3&&wide_record(r)?20480u:8192u)||(bytes+511)/512>4095u-lba)return false;
  bool named=false,ended=false,hashed=false;
  for(unsigned k=0;k<32;k++)if(r[8+k])hashed=true;
  for(unsigned k=0;k<8;k++){
   unsigned ch=r[40+k];if(!ch){ended=true;continue;}
   if(ended||!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='-'||ch=='_'))return false;
   named=true;
  }
  if(!named||!hashed)return false;
  for(unsigned k=0;k<j;k++){
   const uint8_t *q=catalog+16+k*NC_RECORD;
   uint32_t first=nc_u32(q),last=first+(nc_u32(q+4)+511)/512;
   if(lba<last&&first<lba+(bytes+511)/512)return false;
  }
 }
 for(size_t j=16+count*NC_RECORD;j<sizeof catalog;j++)if(catalog[j])return false;
 *out=count;return true;
}
static struct rt865_launch_request record_request(const uint8_t *r){
 struct rt865_launch_request request={.source={.read=catalog_read,.lba=nc_u32(r),.sectors=4095u,.bytes=nc_u32(r+4)}};
 for(unsigned k=0;k<32;k++)request.source.sha256[k]=r[8+k];
 for(unsigned k=0;k<8;k++)request.name[k]=(char)r[40+k];
 return request;
}
static enum ps_status launch_record(const uint8_t *r,uint32_t *pid,uint32_t *win){
 struct rt865_launch_request request=record_request(r);
 if(!win&&wide_record(r)){
  static uint8_t image[20480];
  const size_t bytes=request.source.bytes,blocks=(bytes+511u)/512u;
  if(bytes>sizeof image||!blocks)return PS_EVIDENCE;
  for(size_t i=0;i<blocks;++i)if(!catalog_read(0,request.source.lba+(uint32_t)i,image+i*512u)){
   native_wipe(image,sizeof image);return PS_EVIDENCE;
  }
  enum ps_status status=s875_native_launch_image(image,bytes,request.source.sha256,2,request.name,pid);
  native_wipe(image,sizeof image);
  if(status==PS_OK){
   const struct pr_entry *p=pr_find(pr_global(),*pid);
   if(!p)return PS_EVIDENCE;
   arch_puts("STYLEOS WIDE75 ADMIT");field("pid",*pid);field("gen",p->scheduler_handle);
   field("root",p->page_root);field("bytes",p->resident_bytes);arch_putc('\n');
  }
  return status;
 }
 if(!win)return s865_native_launch(&request,pid);
 struct s865_window_launch result={0};
 enum ps_status status=s865_native_launch_window(&request,stop_session,"Stop fixture",
   (struct ws_bounds){0,0,160,120},&result);
 if(status==PS_OK){*pid=result.pid;*win=result.window;}
 return status;
}
/* Run the same externally pinned executable twice, with actual user
 * fault then fresh launch. The initial ordinary admission of record 2
 * attempts the same syscall without controller authority. */
bool s865_native_shell_probe_available(void){
 return frame_probe&&stop_phase==1&&pr_snapshot(pr_global(),0,0,true)==2;
}
bool s865_native_probe_shell_controller(void){
 if(!frame_probe)return true;
 unsigned count=0;
 if(pr_snapshot(pr_global(),0,0,true)!=2||!catalog_valid(&count)||count!=32)return false;
 struct rt865_launch_request request=record_request(catalog+16+2*NC_RECORD);
 native_wipe(catalog,sizeof catalog);
 struct stop_owned target,peer;
 if(!owned_refs(stop_pid,stop_window,&target)||!owned_refs(peer_pid,peer_window,&peer))return false;
 static unsigned char target_owner[sizeof(struct usr_owner)],target_window[sizeof(struct ws_window)],target_port[sizeof(struct nwc_port)];
 copy_bytes(target_owner,target.owner,sizeof target_owner);
 copy_bytes(target_window,target.window,sizeof target_window);
 copy_bytes(target_port,target.port,sizeof target_port);
 save_peer(&peer);
 if(s865_shell_configure_recovery(&request)!=PS_OK||
    s865_shell_configure_recovery(&request)!=PS_STATE)return false;
 for(unsigned step=0;step<96&&s865_shell_recovery_busy();++step)s865_native_pump();
 if(s865_shell_recovery_busy()||s865_shell_recovery_exhausted()||
    pr_snapshot(pr_global(),0,0,true)!=2||
    !same_peer(&peer)||!same_bytes(target.owner,target_owner,sizeof target_owner)||
    !same_bytes(target.window,target_window,sizeof target_window)||
    !same_bytes(target.port,target_port,sizeof target_port))return false;
 return true;
}
static bool stop_fail(void){
 arch_puts("STYLEOS NATIVE FAIL forced-stop acceptance\n");return false;
}
/* Exercise the same supervisor with the same authenticated source after
 * the successful real F10 command has been consumed. Epochs >=3 deliberately
 * spin in CPL3 without committing. No additional privileged fault hook. */
static bool probe_shell_exhaustion(void){
 struct stop_owned target,peer;
 if(!owned_refs(stop_pid,stop_window,&target)||!owned_refs(peer_pid,peer_window,&peer))return false;
 static unsigned char owner[sizeof(struct usr_owner)],window[sizeof(struct ws_window)],port[sizeof(struct nwc_port)];
 copy_bytes(owner,target.owner,sizeof owner);
 copy_bytes(window,target.window,sizeof window);
 copy_bytes(port,target.port,sizeof port);
 save_peer(&peer);
 if(s865_shell_recovery_busy()||s865_shell_recovery_exhausted()||s865_shell_take_refresh())return false;
 uint64_t start=runtime860->dispatcher.dispatches;
 arch_puts("STYLEOS NATIVE SHELL-EXHAUST-BEGIN");field("dispatches",start);arch_putc('\n');
 if(!s865_shell_submit_refresh()||!s865_shell_recovery_busy())return false;
 for(unsigned step=0;step<160&&s865_shell_recovery_busy();++step)s865_native_pump();
 if(s865_shell_recovery_busy()||!s865_shell_recovery_exhausted()||s865_shell_take_refresh())return false;
 /* Repeated input must neither fall through to the privileged legacy
  * restart nor launch an unlimited fourth controller. */
 if(!s865_shell_submit_refresh()||s865_shell_recovery_busy())return false;
 uint64_t exhausted=runtime860->dispatcher.dispatches;
 for(unsigned step=0;step<12;++step)s865_native_pump();
 if(runtime860->dispatcher.dispatches!=exhausted+12||s865_shell_recovery_busy()||
    !s865_shell_recovery_exhausted()||s865_shell_take_refresh()||
    pr_snapshot(pr_global(),0,0,true)!=2||!same_peer(&peer)||
    !same_bytes(target.owner,owner,sizeof owner)||!same_bytes(target.window,window,sizeof window)||
    !same_bytes(target.port,port,sizeof port))return false;
 arch_puts("STYLEOS NATIVE SHELL-EXHAUST-END");field("dispatches",runtime860->dispatcher.dispatches);
 field("resident",2);arch_putc('\n');
 return true;
}
static bool stop_step(void){
 if(frame_probe&&stop_phase==1&&!probe_shell_exhaustion())return stop_fail();
 const struct pr_entry *target=pr_find(pr_global(),stop_pid);
 const struct pr_entry *peer=pr_find(pr_global(),peer_pid);
 if(!target||!peer||target->state!=PR_READY||peer->state!=PR_READY||
    target->scheduler_handle!=stop_gen||peer->scheduler_handle!=peer_gen||
    target->cpu_ticks<2||peer->cpu_ticks<2||pr_snapshot(pr_global(),0,0,true)!=2)
  return stop_fail();
 uint64_t peer_ticks=peer->cpu_ticks,peer_root=peer->page_root;
 uint64_t old_root=target->page_root;
 uint32_t old_pid=stop_pid;uint64_t old_gen=stop_gen;
 struct stop_owned target_owned,peer_owned;
 if(!owned_refs(stop_pid,stop_window,&target_owned)||!owned_refs(peer_pid,peer_window,&peer_owned)||
    target_owned.port->length!=1||!target_owned.port->has_tile||
    peer_owned.port->length!=1||(!frame_probe&&!peer_owned.port->has_tile)||
    nwc_port_count(channels())!=2||usr_visible_windows(sessions(),0,0)!=2)return stop_fail();
 if(frame_probe){
  const struct nwc_frame *frame=nwc_visible_frame(channels(),peer_window);
  if(!frame||peer_owned.port->has_tile||frame->window_id!=peer_window||
     frame->generation!=3||frame->count!=2||
     frame->rect[0].x||frame->rect[0].y||frame->rect[0].w!=160||
     frame->rect[0].h!=120||frame->rect[0].rgb!=0x234567||
     frame->rect[1].x!=20||frame->rect[1].y!=30||
     frame->rect[1].w!=50||frame->rect[1].h!=40||
     frame->rect[1].rgb!=0x89abcd||
     !nwc_frame_valid(peer_owned.port,peer_owned.window))return stop_fail();
 }
 if(focus_probe){
  const struct nwc_event *ev=&peer_owned.port->events[peer_owned.port->head];
  if(peer_owned.port->next_sequence!=7||ev->sequence!=6||ev->kind!=NWC_KEY||
     ev->code!=0x30||!ev->down||ev->modifiers)return stop_fail();
 }
 save_peer(&peer_owned);
 uint32_t old_window=stop_window;
 uint32_t serial=pr_global()->change_serial;
 if(s865_native_stop(old_pid,old_gen,false)!=PS_EVIDENCE||
    s865_native_stop(old_pid,peer_gen,true)!=PS_EVIDENCE||
    pr_global()->change_serial!=serial||
    !pr_find(pr_global(),old_pid)||!pr_find(pr_global(),peer_pid))
  return stop_fail();
 arch_puts("STYLEOS NATIVE STOP-REFUSALS");field("pid",old_pid);field("gen",old_gen);arch_putc('\n');
 if(s865_native_stop(old_pid,old_gen,true)!=PS_OK||pr_find(pr_global(),old_pid)||
    s865_native_stop(old_pid,old_gen,true)!=PS_EVIDENCE||
    pr_snapshot(pr_global(),0,0,true)!=1)return stop_fail();
 peer=pr_find(pr_global(),peer_pid);
 if(!peer||peer->scheduler_handle!=peer_gen||peer->page_root!=peer_root||peer->cpu_ticks!=peer_ticks||
    peer->state!=PR_READY||!revoked(old_pid,old_gen,old_root,old_window,&target_owned)||
    !same_peer(&peer_owned)||nwc_port_count(channels())!=1||
    usr_visible_windows(sessions(),0,0)!=1)return stop_fail();
 arch_puts("STYLEOS NATIVE STOP-OWNERS-REVOKED");field("pid",old_pid);field("window",old_window);
 field("peer",peer_pid);field("ports",nwc_port_count(channels()));arch_putc('\n');
 if(stop_phase==1){
  unsigned count=0;uint32_t fresh=0;
  if(!catalog_valid(&count)||count!=32||launch_record(catalog+16,&fresh,frame_probe?&stop_window:0)!=PS_OK){
   native_wipe(catalog,sizeof catalog);return stop_fail();
  }
  native_wipe(catalog,sizeof catalog);
  const struct pr_entry *restarted=pr_find(pr_global(),fresh);
  if(!restarted||fresh==old_pid||restarted->scheduler_handle==old_gen||
     restarted->page_root!=old_root||restarted->cpu_ticks||restarted->state!=PR_READY||
     pr_snapshot(pr_global(),0,0,true)!=2||
     s865_native_stop(fresh,old_gen,true)!=PS_EVIDENCE)return stop_fail();
  if(!grant_owned(fresh,restarted->scheduler_handle,&stop_window)||stop_window==old_window||
     !same_peer(&peer_owned)||nwc_port_count(channels())!=2)return stop_fail();
  struct stop_owned fresh_owned;
  if(!owned_refs(fresh,stop_window,&fresh_owned)||fresh_owned.port->next_sequence!=(input_probe?3u:2u)||
     fresh_owned.port->tile.generation!=1||fresh_owned.port->owner_pid!=fresh||
     fresh_owned.port->scheduler_handle!=restarted->scheduler_handle)return stop_fail();
  arch_puts("STYLEOS NATIVE STOP-OWNERS-REBOUND");field("pid",fresh);field("window",stop_window);
  field("old-window",old_window);field("ports",nwc_port_count(channels()));arch_putc('\n');
  stop_pid=fresh;stop_gen=restarted->scheduler_handle;stop_phase=2;
  arch_puts("STYLEOS NATIVE STOP-RESTART");field("old-pid",old_pid);field("old-gen",old_gen);
  field("pid",stop_pid);field("gen",stop_gen);field("root",old_root);field("peer",peer_pid);
  field("peer-ticks",peer_ticks);arch_putc('\n');
  return true;
 }
 if(stop_phase!=2||s865_native_stop(peer_pid,peer_gen,true)!=PS_OK||
    pr_snapshot(pr_global(),0,0,true)||
    !revoked(peer_pid,peer_gen,peer_root,peer_window,&peer_owned)||
    nwc_port_count(channels())||usr_visible_windows(sessions(),0,0)||
    ws_snapshot(ws_global(),0,0))return stop_fail();
 if(frame_probe){
  unsigned count=0;
  if(!catalog_valid(&count)||count!=32){native_wipe(catalog,sizeof catalog);return stop_fail();}
  struct rt865_launch_request request=record_request(catalog+16);
  bool ok=s865_native_probe_window_rollback(&request,stop_session);
  native_wipe(catalog,sizeof catalog);
  if(!ok)return stop_fail();
 }
 if(usr_lock(sessions())!=USR_OK||usr_close(sessions(),stop_session)!=USR_OK||
    usr_find(sessions(),stop_session))return stop_fail();
 arch_puts("STYLEOS NATIVE STOP-OWNERS-EMPTY");field("pid",peer_pid);field("window",peer_window);
 field("ports",nwc_port_count(channels()));arch_putc('\n');
 stop_phase=3;
 arch_puts("STYLEOS NATIVE STOP-DONE");field("resident",pr_snapshot(pr_global(),0,0,true));
 field("dispatches",runtime860->dispatcher.dispatches);arch_putc('\n');
 return true;
}
bool s865_native_catalog_request(void){
 if(native_catalog_busy||!s865_executor_idle())return false;
 if(wide_waiting){
  native_catalog_busy=true;bool ok=wide_stop76?stop76_release():release_wide_workload();native_catalog_busy=false;return ok;
 }
 if(stop_phase==1||stop_phase==2){
  native_catalog_busy=true;bool ok=stop_step();native_catalog_busy=false;return ok;
 }
 if(stop_phase||pr_snapshot(pr_global(),0,0,true))return false;
 native_catalog_busy=true;
 unsigned count=0,accepted=0;
 bool valid=catalog_valid(&count);
 static const uint8_t marker[8]={'s','t','o','p','0','0',0,0};
 bool forced=valid&&count==32&&nc_equal(catalog+16+40,marker,8);
 static const uint8_t input_marker[8]={'i','n','p','u','t','1',0,0};
 static const uint8_t focus_marker[8]={'f','o','c','u','s','1',0,0};
 static const uint8_t frame_marker[8]={'f','r','a','m','e','1',0,0};
 frame_probe=forced&&nc_equal(catalog+16+NC_RECORD+40,frame_marker,8);
 focus_probe=frame_probe||(forced&&nc_equal(catalog+16+NC_RECORD+40,focus_marker,8));
 input_probe=focus_probe||(forced&&nc_equal(catalog+16+NC_RECORD+40,input_marker,8));
 if(frame_probe){
  uint32_t pid=0xabcdef,win=0x123456;
  if(launch_record(catalog+16,&pid,&win)!=PS_ARGUMENT||pid!=0xabcdef||win!=0x123456||
     pr_snapshot(pr_global(),0,0,true)||nwc_port_count(channels())||
     !sessions()||usr_create(sessions(),UINT64_C(0x865),&stop_session)!=USR_OK||
     launch_record(catalog+16,&pid,&win)!=PS_EVIDENCE||pid!=0xabcdef||win!=0x123456||
     pr_snapshot(pr_global(),0,0,true)||nwc_port_count(channels())||
     usr_select(sessions(),stop_session)!=USR_OK){
   native_wipe(catalog,sizeof catalog);native_catalog_busy=false;return stop_fail();
  }
  arch_puts("STYLEOS NATIVE WINDOW-LAUNCH-REFUSALS");field("session",stop_session);arch_putc('\n');
 }
 if(valid)for(unsigned j=0;j<count;j++){
  const uint8_t *r=catalog+16+j*NC_RECORD;
  uint32_t pid;
  if(launch_record(r,&pid,frame_probe&&j<2?(j?&peer_window:&stop_window):0)!=PS_OK)break;
  if(forced&&j<2){
   const struct pr_entry *owner=pr_find(pr_global(),pid);
   if(!owner)break;
   if(j==0){stop_pid=pid;stop_gen=owner->scheduler_handle;}
   else{peer_pid=pid;peer_gen=owner->scheduler_handle;}
  }
  ++accepted;
 }
 if(frame_probe&&accepted==count){
  uint32_t pid=0xabcdef,win=0x123456;
  if(launch_record(catalog+16,&pid,&win)!=PS_FULL||pid!=0xabcdef||win!=0x123456||
     pr_snapshot(pr_global(),0,0,true)!=32||nwc_port_count(channels())!=2){
   native_wipe(catalog,sizeof catalog);native_catalog_busy=false;return stop_fail();
  }
  arch_puts("STYLEOS NATIVE WINDOW-LAUNCH-CAPACITY");field("resident",32);arch_putc('\n');
 }
 /* Before the first dispatch, require the real supervisor process API to
  * report both expanded leases and the legacy peer's actual allocation.
  * This path is limited to the explicit three-record fixture. */
 wide_stop76=valid&&count==3&&accepted==3&&stop76_catalog();
 if(valid&&count==3&&accepted==3&&(wide_catalog()||wide_stop76)){
  static struct pa_row rows[PA_MAX_ROWS];
  struct pa_reply reply={0};
  const struct pa_request query={.version=PA_VERSION,.order=PI_BY_PID};
  if(s865_native_snapshot(&query,rows,PA_MAX_ROWS,&reply)!=PA_OK||
     reply.returned!=3||reply.total!=3||
     reply.measurement_flags!=(PA_MEASURE_CPU_TICKS|PA_MEASURE_MEMORY_BYTES)||
     rows[0].resident_bytes!=7u*4096u||rows[1].resident_bytes!=7u*4096u||
     rows[2].resident_bytes!=3u*4096u||!remember_wide_owners(rows)){
   native_wipe(catalog,sizeof catalog);native_catalog_busy=false;
   arch_puts("STYLEOS WIDE75 FAIL process accounting\n");return false;
  }
  if(wide_stop76){
   stop_pid=wide_owners[0].pid;stop_gen=wide_owners[0].generation;
   peer_pid=wide_owners[1].pid;peer_gen=wide_owners[1].generation;
   if(!sessions()||!channels()||usr_create(sessions(),UINT64_C(0x876),&stop_session)!=USR_OK||
      usr_select(sessions(),stop_session)!=USR_OK||
      !grant_owned(stop_pid,stop_gen,&stop_window)||!grant_owned(peer_pid,peer_gen,&peer_window)||
      nwc_port_count(channels())!=2){
    native_wipe(catalog,sizeof catalog);native_catalog_busy=false;return stop_fail();
   }
   arch_puts("STYLEOS WIDE76 BOUND");field("pid",stop_pid);field("window",stop_window);
   field("peer",peer_pid);field("peer-window",peer_window);arch_putc('\n');
  }
  arch_puts("STYLEOS WIDE75 ACCOUNT");field("first",rows[0].resident_bytes);
  field("second",rows[1].resident_bytes);field("peer",rows[2].resident_bytes);
  field("flags",reply.measurement_flags);arch_putc('\n');
 }
 native_wipe(catalog,sizeof catalog);native_catalog_busy=false;
 arch_puts("STYLEOS NATIVE CATALOG");field("valid",valid);field("requested",count);field("accepted",accepted);field("resident",pr_snapshot(pr_global(),0,0,true));arch_putc('\n');
 if(forced&&accepted==count){
  if(!sessions()||!channels()||(!frame_probe&&(usr_create(sessions(),UINT64_C(0x865),&stop_session)!=USR_OK||
     usr_select(sessions(),stop_session)!=USR_OK))||
     !grant_owned(stop_pid,stop_gen,&stop_window)||!grant_owned(peer_pid,peer_gen,&peer_window)||
     nwc_port_count(channels())!=2||usr_visible_windows(sessions(),0,0)!=2)return stop_fail();
  arch_puts("STYLEOS NATIVE STOP-OWNERS-BOUND");field("pid",stop_pid);field("window",stop_window);
  field("peer",peer_pid);field("peer-window",peer_window);field("ports",nwc_port_count(channels()));arch_putc('\n');
  stop_phase=1;
 }
 return valid&&accepted==count;
}
#endif
