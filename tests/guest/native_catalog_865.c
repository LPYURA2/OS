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
  if(lba<3072u||lba>=4095u||bytes<120u||bytes>8192u||(bytes+511)/512>4095u-lba)return false;
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
 if(!win)return s865_native_launch(&request,pid);
 struct s865_window_launch result={0};
 enum ps_status status=s865_native_launch_window(&request,stop_session,"Stop fixture",
   (struct ws_bounds){0,0,160,120},&result);
 if(status==PS_OK){*pid=result.pid;*win=result.window;}
 return status;
}
static bool stop_fail(void){
 arch_puts("STYLEOS NATIVE FAIL forced-stop acceptance\n");return false;
}
static bool stop_step(void){
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
