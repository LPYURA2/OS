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
#define S34_SERIAL "STYLEOS-SFS34-DATA  "
#define S34_TAG "STYLEOS-SFS34-PRIVATE-V1"
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
 if(pair.sequence==4&&s34_check(svc,&l))s34_end("SFS34 COLD PASS");
 if(pair.sequence!=1||s34_work.stream.pair.dir.count||s34_work.stream.pair.inode.root.files) s34_end("SFS34 REFUSE nonblank");
 struct dc64_edit e={.op=DC64_CREATE,.entry={s34_key(1,'d'),2,SD64_DIRECTORY}};
 if(pp64_edit(svc,&l,&s34_work.stream,s34_work.nodes,32,s34_work.current,32,1,&e,0,&pair)!=PP64_OK)fail("sfs34-mkdir");
 struct sd64_key k=s34_key(2,'a');
 if(pp64_put(svc,&l,&s34_work.stream,s34_work.nodes,32,s34_work.current,s34_work.previous,32,2,&k,3,0,S34_BYTES,s34_cancel,0,&pair)!=PP64_CANCELLED)fail("sfs34-cancel");
 if(pp64_put(svc,&l,&s34_work.stream,s34_work.nodes,32,s34_work.current,s34_work.previous,32,2,&k,3,0,S34_BYTES,s34_source,0,&pair)!=PP64_OK)fail("sfs34-put");
 e=(struct dc64_edit){.op=DC64_RENAME,.entry={k,3,SD64_FILE},.destination=s34_key(2,'b')};
 if(pp64_edit(svc,&l,&s34_work.stream,s34_work.nodes,32,s34_work.current,32,3,&e,1,&pair)!=PP64_OK||!s34_check(svc,&l))fail("sfs34-rename-read");
 s863_shell_guard();s34_end("SFS34 WRITE PASS");
}
