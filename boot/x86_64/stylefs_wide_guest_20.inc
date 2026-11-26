/* Included only in the separately selected post-EBS test profile.
 * Existing safe-root/PCI/DMA guard remains authoritative. No general grant. */
#include <styleos/ata_runtime_48.h>
#include <styleos/stylefs_stream64.h>
#define S20_N (UINT64_C(4096)<<21)
#define S20_CAP (S20_N-1)
#define S20_START (S20_CAP-32)
#define S20_BYTES 1041u
#define S20_SERIAL "STYLEOS-SFS20-DATA  "
#define S20_TAG "STYLEOS-SFS20-PRIVATE-V1"
enum s20_action { S20_IDENT,S20_READ,S20_WRITE,S20_FLUSH };
struct s20_guard {
 struct ata857_live *live; struct ata_runtime_48 ata;
 struct {unsigned r;uint8_t v;} seq[13];
 unsigned len,pos,words;enum s20_action action;
 bool issued,tagged,denied;
};
static bool s20_deny(struct s20_guard *g){g->denied=true;return false;}
static bool s20_live(struct s20_guard *g){
 if(!g||g->denied||g->live!=&disk863.s.live||!ata857_live_check(g->live))
  return g?s20_deny(g):false;
 return true;
}
static void s20_add(struct s20_guard *g,unsigned r,uint8_t v){
 g->seq[g->len].r=r;g->seq[g->len++].v=v;
}
static bool s20_begin(struct s20_guard *g,enum s20_action a,uint64_t lba){
 if(g->denied||g->pos!=g->len||(g->issued&&g->words!=(g->action==S20_FLUSH?0u:256u)))return s20_deny(g);
 if((a==S20_WRITE||a==S20_FLUSH)&&!g->tagged)return s20_deny(g);
 if(a==S20_WRITE&&!(lba>=2&&lba<=19)&&!(lba>=S20_START+1&&lba<S20_N))return s20_deny(g);
 if(a==S20_READ&&!(lba<=19)&&!(lba>=S20_START+1&&lba<S20_N))return s20_deny(g);
 if((a==S20_READ||a==S20_WRITE)&&g->ata.bound&&!g->ata.busy&&!g->ata.pending_flush&&!g->ata.quarantined){
  g->live->ops=0;
  if(!same_cache(&shell_cache863)||!s20_live(g))return s20_deny(g);
 }
 g->len=g->pos=g->words=0;g->issued=false;g->action=a;
 s20_add(g,8,2);s20_add(g,6,a==S20_IDENT?0xa0:0xe0);
 if(a==S20_IDENT){for(unsigned i=1;i<=5;i++)s20_add(g,i,0);s20_add(g,7,0xec);}
 else if(a==S20_FLUSH)s20_add(g,7,0xea);
 else{
  s20_add(g,1,0);s20_add(g,2,0);s20_add(g,3,(uint8_t)(lba>>24));
  s20_add(g,4,(uint8_t)(lba>>32));s20_add(g,5,(uint8_t)(lba>>40));
  s20_add(g,1,0);s20_add(g,2,1);s20_add(g,3,(uint8_t)lba);
  s20_add(g,4,(uint8_t)(lba>>8));s20_add(g,5,(uint8_t)(lba>>16));
  s20_add(g,7,a==S20_READ?0x24:0x34);
 }
 return true;
}
static bool s20_status(void *ctx,uint8_t *v){
 struct s20_guard *g=ctx;
 return v&&s20_live(g)&&ata857_alt(g->live,v);
}
static bool s20_reg(void *ctx,unsigned r,uint8_t v){
 struct s20_guard *g=ctx;
 if(!s20_live(g)||g->pos>=g->len||g->seq[g->pos].r!=r||g->seq[g->pos].v!=v)return s20_deny(g);
 uint16_t port=r==8?0x376:(uint16_t)(0x170+r);
 __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(port):"memory");
 ++g->pos;if(r==7)g->issued=true;return true;
}
static bool s20_rd(void *ctx,uint16_t *v){
 struct s20_guard *g=ctx;
 if(!v||!s20_live(g)||!g->issued||g->pos!=g->len||g->words>=256||
    (g->action!=S20_IDENT&&g->action!=S20_READ))return s20_deny(g);
 *v=in16(0x170);++g->words;return true;
}
static bool s20_wr(void *ctx,uint16_t v){
 struct s20_guard *g=ctx;
 if(!s20_live(g)||!g->issued||g->pos!=g->len||g->words>=256||g->action!=S20_WRITE||!g->tagged)return s20_deny(g);
 out16(0x170,v);++g->words;return true;
}
static bool s20_pause(void *ctx){
 struct s20_guard *g=ctx;if(!s20_live(g))return false;
 for(unsigned i=0;i<256;i++)__asm__ volatile("pause":::"memory");
 return true;
}
static const struct ata_runtime_ops_863 s20_ops={{s20_status,s20_reg,s20_rd},s20_wr,s20_pause};
static bool s20_read(void *ctx,uint64_t lba,uint8_t *p){
 struct s20_guard *g=ctx;
 return g&&g->tagged&&lba<S20_CAP&&s20_begin(g,S20_READ,lba+1)&&ata48_read(&g->ata,lba+1,p);
}
static bool s20_write(void *ctx,uint64_t lba,const uint8_t *p){
 struct s20_guard *g=ctx;
 return g&&lba<S20_CAP&&s20_begin(g,S20_WRITE,lba+1)&&ata48_write(&g->ata,lba+1,p);
}
static bool s20_flush(void *ctx){
 struct s20_guard *g=ctx;
 return g&&s20_begin(g,S20_FLUSH,0)&&ata48_flush(&g->ata);
}

/* Explicitly admitted, static bounded workspace; no file-sized allocation. */
#define S20_PAGE_ROUND(n) (((n)+4095u)&~(size_t)4095u)
static _Alignas(4096) union {
 struct sfs64_journal value;
 uint8_t pages[S20_PAGE_ROUND(sizeof(struct sfs64_journal))];
} s20_journal_storage;
static _Alignas(4096) union {
 struct sfs64_stream value;
 uint8_t pages[S20_PAGE_ROUND(sizeof(struct sfs64_stream))];
} s20_stream_storage;
_Static_assert(sizeof s20_journal_storage%4096==0,"journal whole pages");
_Static_assert(sizeof s20_stream_storage%4096==0,"stream whole pages");
#define s20_journal (s20_journal_storage.value)
#define s20_stream (s20_stream_storage.value)
static uint8_t s20_byte(unsigned i){return (uint8_t)(i*37u+i/512u*91u+13u);}
static _Noreturn void s20_end(const char *s){
 arch_puts(s);arch_putc('\n');arch_halt();for(;;)__asm__ volatile("hlt");
}
static bool s20_check(struct sfs64_journal *j,uint64_t id){
 struct sfs64_file f;uint8_t buf[512];uint32_t crc=UINT32_MAX;
 if(sfs64_stream_stat(j,id,&f)!=SJ_OK||f.id!=id||f.revision!=1||f.bytes!=S20_BYTES)return false;
 for(unsigned off=0;off<S20_BYTES;){
  size_t want=S20_BYTES-off<sizeof buf?S20_BYTES-off:sizeof buf,done=0;
  if(sfs64_stream_read(j,id,f.revision,off,buf,want,&done)!=BS_OK||done!=want)return false;
  for(size_t i=0;i<done;i++){
   if(buf[i]!=s20_byte(off+(unsigned)i))return false;
   crc^=buf[i];for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^((crc&1)?UINT32_C(0xedb88320):0);
  }
  off+=(unsigned)done;
 }
 return ~crc==f.checksum;
}
static _Noreturn void s20_run(void){
 s863_shell_guard();
 struct s20_guard g={.live=&disk863.s.live};
 uint8_t buf[512];uint64_t gen=0,token=0;
 if(!s20_begin(&g,S20_IDENT,0)||!ata48_bind(&g.ata,&s20_ops,&g,S20_SERIAL,S20_N))s20_end("SFS20 REFUSE identity");
 if(!s20_begin(&g,S20_READ,0)||!ata48_read(&g.ata,0,buf))fail("sfs20-tag-read");
 static const char tag[]=S20_TAG;
 for(unsigned i=0;i<512;i++)if(buf[i]!=(i<sizeof(tag)-1?(uint8_t)tag[i]:0))s20_end("SFS20 REFUSE tag");
 g.tagged=true;
 struct bs_service *svc=&disk863.s.block;
 const struct bs_backend backend={&g,UINT64_C(0x701080862820),S20_CAP,true,false,s20_read,s20_write,s20_flush};
 if(bs_attach(svc,&backend,&gen)!=BS_OK||
    bs_grant_range(svc,gen,BLOCK_SHELL_OWNER,0,S20_CAP,BS_READ|BS_WRITE,&token)!=BS_OK)fail("sfs20-grant");
 unsigned ops=g.live->ops;
 if(bs_read_sector(svc,BLOCK_SHELL_OWNER+1,token,0,buf)!=BS_DENIED||
    bs_write_sector(svc,BLOCK_SHELL_OWNER,token,S20_CAP,buf)!=BS_RANGE||g.live->ops!=ops)fail("sfs20-denials");
 enum sj_status mount=sfs64_journal_mount(&s20_journal,svc,BLOCK_SHELL_OWNER,token,S20_CAP,S20_START);
 if(mount==SJ_SIGNATURE||mount==SJ_CORRUPT)s20_end("SFS20 REFUSE journal");
 if(mount!=SJ_OK)fail("sfs20-mount");
 uint64_t generation=s20_journal.generation[s20_journal.active];
 struct sfs64_file info;
 if(generation==2&&s20_check(&s20_journal,1)&&sfs64_stream_stat(&s20_journal,2,&info)==SJ_STALE)s20_end("SFS20 COLD PASS");
 if(generation!=1||s20_journal.length[s20_journal.active])s20_end("SFS20 REFUSE nonblank");
 /* Cancel an unpublished staged write, prove no file or generation change. */
 if(sfs64_stream_begin(&s20_stream,&s20_journal,2,0,S20_BYTES)!=SJ_OK)fail("sfs20-cancel-begin");
 for(unsigned i=0;i<sizeof buf;i++)buf[i]=s20_byte(i)^0xffu;
 if(sfs64_stream_append(&s20_stream,buf,sizeof buf)!=SJ_OK)fail("sfs20-cancel-write");
 sfs64_stream_cancel(&s20_stream);
 if(sfs64_stream_stat(&s20_journal,2,&info)!=SJ_STALE||s20_journal.generation[s20_journal.active]!=1)fail("sfs20-cancel-visible");
 if(sfs64_stream_begin(&s20_stream,&s20_journal,1,0,S20_BYTES)!=SJ_OK)fail("sfs20-begin");
 for(unsigned off=0;off<S20_BYTES;){
  size_t n=S20_BYTES-off<sizeof buf?S20_BYTES-off:sizeof buf;
  for(size_t i=0;i<n;i++)buf[i]=s20_byte(off+(unsigned)i);
  if(sfs64_stream_append(&s20_stream,buf,n)!=SJ_OK)fail("sfs20-append");
  off+=(unsigned)n;
 }
 if(sfs64_stream_stat(&s20_journal,1,&info)!=SJ_STALE||
    sfs64_stream_finish(&s20_stream)!=SJ_OK||
    s20_journal.generation[s20_journal.active]!=2||!s20_check(&s20_journal,1))fail("sfs20-commit");
 s863_shell_guard();s20_end("SFS20 WRITE PASS");
}
