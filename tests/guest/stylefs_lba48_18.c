/* Dedicated EFI diagnostic, never linked into the ordinary kernel.
 * Run only against a disposable Q35/PIIX3 fixture created by the runner. */
#include <styleos/ata_runtime_48.h>
#ifndef FIXTURE_SECTORS
#error explicit geometry required
#endif
#define N ((uint64_t)FIXTURE_SECTORS)
#define BOUNDARY UINT64_C(0x10000000)
#define SERIAL "STYLEOS-SFS18-DATA  "
#define TAG "STYLEOS-SFS18-PRIVATE-V1"
enum action { IDENTIFY, READ, WRITE, FLUSH };
struct step { unsigned r; uint8_t v; };
struct guard {
    struct step seq[13];
    unsigned len, pos, words;
    enum action action;
    uint16_t bm;
    bool tagged, command, denied;
};
static struct guard g;
static struct ata_runtime_48 disk;
static inline void out8(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p):"memory");}
static inline void out16(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p):"memory");}
static inline void out32(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p):"memory");}
static inline uint8_t in8(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p):"memory");return v;}
static inline uint16_t in16(uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p):"memory");return v;}
static inline uint32_t in32(uint16_t p){uint32_t v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p):"memory");return v;}
static uint32_t cfg(unsigned off){out32(0xcf8,0x80002800u|off);return in32(0xcfc);}
static bool idle_dma(uint16_t b){
    return !(in8(b)&1u)&&!(in8(b+2)&1u)&&!(in8(b+8)&1u)&&!(in8(b+10)&1u);
}
static bool live(struct guard *p){
    if(p!=&g||p->denied||cfg(0)!=0x70108086u||
       (cfg(8)>>16)!=0x0101u||(cfg(8)&0x500u)||
       (cfg(4)&5u)!=1u||cfg(0x20)!=((uint32_t)p->bm|1u)||
       !idle_dma(p->bm)){g.denied=true;return false;}
    return true;
}
static bool deny(void){g.denied=true;return false;}
static void emit(const char *s){while(*s)out8(0xe9,(uint8_t)*s++);}
static void end(const char *s){emit(s);emit("\n");for(;;)__asm__ volatile("hlt");}
static void add(unsigned r,uint8_t v){g.seq[g.len++]=(struct step){r,v};}
static bool begin(enum action a,uint64_t lba){
    if(g.denied||g.pos!=g.len||(g.command&&g.words!=((g.action==FLUSH)?0u:256u)))return deny();
    if(a==WRITE&&(!g.tagged||(lba!=BOUNDARY&&lba!=N-1)))return deny();
    if(a==FLUSH&&!g.tagged)return deny();
    if(a==READ&&(lba!=0&&lba!=BOUNDARY&&lba!=N-1))return deny();
    g.len=g.pos=g.words=0;g.command=false;g.action=a;
    add(8,2);add(6,a==IDENTIFY?0xa0:0xe0);
    if(a==IDENTIFY){for(unsigned i=1;i<=5;i++)add(i,0);add(7,0xec);}
    else if(a==FLUSH)add(7,0xea);
    else{
        add(1,0);add(2,0);add(3,(uint8_t)(lba>>24));add(4,(uint8_t)(lba>>32));add(5,(uint8_t)(lba>>40));
        add(1,0);add(2,1);add(3,(uint8_t)lba);add(4,(uint8_t)(lba>>8));add(5,(uint8_t)(lba>>16));
        add(7,a==READ?0x24:0x34);
    }
    return true;
}
static bool status(void *ctx,uint8_t *v){
    if(!v||!live(ctx))return false;
    *v=in8(0x376);return true;
}
static bool reg(void *ctx,unsigned r,uint8_t v){
    if(!live(ctx)||g.pos>=g.len||g.seq[g.pos].r!=r||g.seq[g.pos].v!=v)return deny();
    out8(r==8?0x376:(uint16_t)(0x170+r),v);g.pos++;
    if(r==7)g.command=true;
    return true;
}
static bool rd(void *ctx,uint16_t *v){
    if(!v||!live(ctx)||!g.command||g.pos!=g.len||g.words>=256||
       (g.action!=IDENTIFY&&g.action!=READ))return deny();
    *v=in16(0x170);g.words++;return true;
}
static bool wr(void *ctx,uint16_t v){
    if(!live(ctx)||!g.command||g.pos!=g.len||g.words>=256||g.action!=WRITE||!g.tagged)return deny();
    out16(0x170,v);g.words++;return true;
}
static bool pause_io(void *ctx){
    if(!live(ctx))return false;
    for(unsigned i=0;i<256;i++)__asm__ volatile("pause");
    return true;
}
static const struct ata_runtime_ops_863 ops={{status,reg,rd},wr,pause_io};
static bool read_at(uint64_t lba,uint8_t *p){return begin(READ,lba)&&ata48_read(&disk,lba,p);}
static bool write_at(uint64_t lba,const uint8_t *p){
    return begin(WRITE,lba)&&ata48_write(&disk,lba,p)&&begin(FLUSH,0)&&ata48_flush(&disk);
}
static uint8_t marker(unsigned slot,unsigned i){return (uint8_t)(i*37u+slot*91u+13u);}
static bool match(const uint8_t *p,unsigned slot){
    for(unsigned i=0;i<512;i++)if(p[i]!=marker(slot,i))return false;
    return true;
}
static bool zero(const uint8_t *p){for(unsigned i=0;i<512;i++)if(p[i])return false;return true;}
static bool tag(const uint8_t *p){
    static const char t[]=TAG;
    for(unsigned i=0;i<512;i++)if(p[i]!=(i<sizeof(t)-1?(uint8_t)t[i]:0))return false;
    return true;
}
void *memset(void *d,int v,__SIZE_TYPE__ n){uint8_t *p=d;while(n--)*p++=(uint8_t)v;return d;}
void *memcpy(void *d,const void *s,__SIZE_TYPE__ n){uint8_t *p=d;const uint8_t *q=s;while(n--)*p++=*q++;return d;}
/* Terminal firmware diagnostic: do not return to firmware after direct I/O.
 * No services, interrupts, DMA, arbitrary disk selection, or reset. */
__attribute__((ms_abi)) uint64_t efi_main(void *image,void *system){
    (void)image;(void)system;
    __asm__ volatile("cli":::"memory");
    uint32_t cls=cfg(8),bar=cfg(0x20),cmd=cfg(4);
    if(cfg(0)!=0x70108086u||(cls>>16)!=0x0101u||(cls&0x500u)||
       !(cmd&1u)||(bar&15u)!=1u||bar>0xffffu||bar<0x1000u)end("SFS18 FAIL controller");
    g.bm=(uint16_t)(bar&~15u);
    if(!idle_dma(g.bm))end("SFS18 FAIL active DMA");
    /* Only this fixture's PCI command register, 16-bit write avoids status W1C. */
    out32(0xcf8,0x80002804u);out16(0xcfc,(uint16_t)(cmd&~4u));
    if(!live(&g))end("SFS18 FAIL guard");
    if(!begin(IDENTIFY,0)||!ata48_bind(&disk,&ops,&g,SERIAL,N))end("SFS18 REFUSE identity");
    uint8_t buf[512],first[512],last[512];
    if(!read_at(0,buf))end("SFS18 FAIL tag read");
    if(!tag(buf))end("SFS18 REFUSE tag");
    g.tagged=true;
    if(!read_at(BOUNDARY,first)||!read_at(N-1,last))end("SFS18 FAIL data read");
    if(match(first,1)&&match(last,2))end("SFS18 COLD PASS");
    if(!zero(first)||!zero(last))end("SFS18 REFUSE nonblank");
    for(unsigned i=0;i<512;i++)buf[i]=marker(1,i);
    if(!write_at(BOUNDARY,buf))end("SFS18 FAIL boundary write");
    for(unsigned i=0;i<512;i++)buf[i]=marker(2,i);
    if(!write_at(N-1,buf))end("SFS18 FAIL end write");
    if(!read_at(BOUNDARY,first)||!read_at(N-1,last)||!match(first,1)||!match(last,2))end("SFS18 FAIL readback");
    end("SFS18 WRITE PASS");
    return 0;
}
