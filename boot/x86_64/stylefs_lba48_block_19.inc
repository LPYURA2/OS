/* Included only in the separately selected post-EBS test profile.
 * Existing safe-root/PCI/DMA guard remains authoritative. No general grant. */
#include <styleos/ata_runtime_48.h>
#define S19_N (UINT64_C(4096)<<21)
#define S19_HIGH (UINT64_C(1)<<28)
#define S19_SERIAL "STYLEOS-SFS18-DATA  "
#define S19_TAG "STYLEOS-SFS18-PRIVATE-V1"
enum s19_action { S19_IDENT,S19_READ,S19_WRITE,S19_FLUSH };
struct s19_guard {
 struct ata857_live *live; struct ata_runtime_48 ata;
 struct {unsigned r;uint8_t v;} seq[13];
 unsigned len,pos,words;enum s19_action action;
 bool issued,tagged,denied;
};
static bool s19_deny(struct s19_guard *g){g->denied=true;return false;}
static bool s19_live(struct s19_guard *g){
 if(!g||g->denied||g->live!=&disk863.s.live||!ata857_live_check(g->live))
  return g?s19_deny(g):false;
 return true;
}
static void s19_add(struct s19_guard *g,unsigned r,uint8_t v){
 g->seq[g->len].r=r;g->seq[g->len++].v=v;
}
static bool s19_begin(struct s19_guard *g,enum s19_action a,uint64_t lba){
 if(g->denied||g->pos!=g->len||(g->issued&&g->words!=(g->action==S19_FLUSH?0u:256u)))return s19_deny(g);
 if((a==S19_WRITE||a==S19_FLUSH)&&!g->tagged)return s19_deny(g);
 if(a==S19_WRITE&&lba!=S19_HIGH&&lba!=S19_N-1)return s19_deny(g);
 if(a==S19_READ&&lba!=0&&lba!=S19_HIGH&&lba!=S19_N-1)return s19_deny(g);
 g->len=g->pos=g->words=0;g->issued=false;g->action=a;
 s19_add(g,8,2);s19_add(g,6,a==S19_IDENT?0xa0:0xe0);
 if(a==S19_IDENT){for(unsigned i=1;i<=5;i++)s19_add(g,i,0);s19_add(g,7,0xec);}
 else if(a==S19_FLUSH)s19_add(g,7,0xea);
 else{
  s19_add(g,1,0);s19_add(g,2,0);s19_add(g,3,(uint8_t)(lba>>24));
  s19_add(g,4,(uint8_t)(lba>>32));s19_add(g,5,(uint8_t)(lba>>40));
  s19_add(g,1,0);s19_add(g,2,1);s19_add(g,3,(uint8_t)lba);
  s19_add(g,4,(uint8_t)(lba>>8));s19_add(g,5,(uint8_t)(lba>>16));
  s19_add(g,7,a==S19_READ?0x24:0x34);
 }
 return true;
}
static bool s19_status(void *ctx,uint8_t *v){
 struct s19_guard *g=ctx;
 return v&&s19_live(g)&&ata857_alt(g->live,v);
}
static bool s19_reg(void *ctx,unsigned r,uint8_t v){
 struct s19_guard *g=ctx;
 if(!s19_live(g)||g->pos>=g->len||g->seq[g->pos].r!=r||g->seq[g->pos].v!=v)return s19_deny(g);
 uint16_t port=r==8?0x376:(uint16_t)(0x170+r);
 __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(port):"memory");
 ++g->pos;if(r==7)g->issued=true;return true;
}
static bool s19_rd(void *ctx,uint16_t *v){
 struct s19_guard *g=ctx;
 if(!v||!s19_live(g)||!g->issued||g->pos!=g->len||g->words>=256||
    (g->action!=S19_IDENT&&g->action!=S19_READ))return s19_deny(g);
 *v=in16(0x170);++g->words;return true;
}
static bool s19_wr(void *ctx,uint16_t v){
 struct s19_guard *g=ctx;
 if(!s19_live(g)||!g->issued||g->pos!=g->len||g->words>=256||g->action!=S19_WRITE||!g->tagged)return s19_deny(g);
 out16(0x170,v);++g->words;return true;
}
static bool s19_pause(void *ctx){
 struct s19_guard *g=ctx;if(!s19_live(g))return false;
 for(unsigned i=0;i<256;i++)__asm__ volatile("pause":::"memory");
 return true;
}
static const struct ata_runtime_ops_863 s19_ops={{s19_status,s19_reg,s19_rd},s19_wr,s19_pause};
static bool s19_read(void *ctx,uint64_t lba,uint8_t *p){
 struct s19_guard *g=ctx;
 return g&&g->tagged&&s19_begin(g,S19_READ,lba)&&ata48_read(&g->ata,lba,p);
}
static bool s19_write(void *ctx,uint64_t lba,const uint8_t *p){
 struct s19_guard *g=ctx;
 return g&&s19_begin(g,S19_WRITE,lba)&&ata48_write(&g->ata,lba,p);
}
static bool s19_flush(void *ctx){
 struct s19_guard *g=ctx;
 return g&&s19_begin(g,S19_FLUSH,0)&&ata48_flush(&g->ata);
}
static uint8_t s19_marker(unsigned slot,unsigned i){return (uint8_t)(i*37u+slot*91u+13u);}
static bool s19_match(const uint8_t *p,unsigned slot){
 for(unsigned i=0;i<512;i++)if(p[i]!=s19_marker(slot,i))return false;
 return true;
}
static bool s19_zero(const uint8_t *p){for(unsigned i=0;i<512;i++)if(p[i])return false;return true;}
static _Noreturn void s19_end(const char *s){
 arch_puts(s);arch_putc('\n');arch_halt();for(;;)__asm__ volatile("hlt");
}
static _Noreturn void s19_run(void){
 s863_shell_guard();
 struct s19_guard g={.live=&disk863.s.live};
 uint8_t buf[512],a[512],b[512];uint64_t gen=0,low=0,hi=0,last=0;
 if(!s19_begin(&g,S19_IDENT,0)||!ata48_bind(&g.ata,&s19_ops,&g,S19_SERIAL,S19_N))s19_end("SFS19 REFUSE identity");
 if(!s19_begin(&g,S19_READ,0)||!ata48_read(&g.ata,0,buf))fail("sfs19-tag-read");
 static const char tag[]=S19_TAG;
 for(unsigned i=0;i<512;i++)if(buf[i]!=(i<sizeof(tag)-1?(uint8_t)tag[i]:0))s19_end("SFS19 REFUSE tag");
 g.tagged=true;
 struct bs_service *svc=&disk863.s.block;
 const struct bs_backend backend={&g,UINT64_C(0x701080862819),S19_N,true,false,s19_read,s19_write,s19_flush};
 if(bs_attach(svc,&backend,&gen)!=BS_OK||
    bs_grant_range(svc,gen,BLOCK_SHELL_OWNER,0,1,BS_READ,&low)!=BS_OK||
    bs_grant_range(svc,gen,BLOCK_SHELL_OWNER,S19_HIGH,1,BS_READ|BS_WRITE,&hi)!=BS_OK||
    bs_grant_range(svc,gen,BLOCK_SHELL_OWNER,S19_N-1,1,BS_READ|BS_WRITE,&last)!=BS_OK)fail("sfs19-grants");
 unsigned n=g.live->ops;
 if(bs_write_sector(svc,BLOCK_SHELL_OWNER,low,0,buf)!=BS_DENIED||
    bs_write_sector(svc,BLOCK_SHELL_OWNER,hi,1,buf)!=BS_RANGE||
    bs_read_sector(svc,BLOCK_SHELL_OWNER+1,last,0,buf)!=BS_DENIED||g.live->ops!=n)fail("sfs19-denials");
 if(bs_read_sector(svc,BLOCK_SHELL_OWNER,hi,0,a)!=BS_OK||
    bs_read_sector(svc,BLOCK_SHELL_OWNER,last,0,b)!=BS_OK)fail("sfs19-read");
 if(s19_match(a,1)&&s19_match(b,2))s19_end("SFS19 COLD PASS");
 if(!s19_zero(a)||!s19_zero(b))s19_end("SFS19 REFUSE nonblank");
 for(unsigned i=0;i<512;i++)buf[i]=s19_marker(1,i);
 if(bs_write_sector(svc,BLOCK_SHELL_OWNER,hi,0,buf)!=BS_OK)fail("sfs19-write-high");
 for(unsigned i=0;i<512;i++)buf[i]=s19_marker(2,i);
 if(bs_write_sector(svc,BLOCK_SHELL_OWNER,last,0,buf)!=BS_OK)fail("sfs19-write-last");
 if(bs_read_sector(svc,BLOCK_SHELL_OWNER,hi,0,a)!=BS_OK||
    bs_read_sector(svc,BLOCK_SHELL_OWNER,last,0,b)!=BS_OK||
    !s19_match(a,1)||!s19_match(b,2))fail("sfs19-readback");
 s863_shell_guard();s19_end("SFS19 WRITE PASS");
}
