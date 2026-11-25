#include <styleos/desktop_gop_850.h>
#include <styleos/runtime_roots_861.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BASE UINT64_C(0x80000000)
static uint64_t t[4][512];
static unsigned char raw[80];
static struct boot_info b;
static struct boot_memory_map m;
static struct boot_memory_evidence e;
static struct desktop_surface s;
static struct pci794_bar bar;
static struct s842_cache c;
static unsigned count;
static bool read_table(void *p,uint64_t pa,uint16_t i,uint64_t *out){(void)p;if(pa<0x1000||pa>0x4000||(pa&4095)||i>=512)return false;*out=t[pa/4096-1][i];return true;}
static void setle(unsigned char *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(unsigned char)(v>>(8*i));}
static void descriptor(unsigned n,unsigned type,uint64_t start,uint64_t pages,uint64_t attr){unsigned char *p=raw+40*n;memset(p,0,40);setle(p,type,4);setle(p+8,start,8);setle(p+24,pages,8);setle(p+32,attr,8);}
static struct s850_gop_request reset(void){
 memset(t,0,sizeof t);memset(&m,0,sizeof m);memset(raw,0,sizeof raw);
 t[0][0]=0x2003;t[1][2]=0x3003;t[2][0]=BASE|0x83;t[2][1]=(BASE+0x200000)|0x83;
 descriptor(0,2,0x100000,0x7000,8);descriptor(1,11,BASE,0x1000,1);
 b=(struct boot_info){.magic=STYLEOS_BOOT_MAGIC,.version=1,.size=sizeof b,.arch=1,.platform=1,.flags=1,.memory_map=(uintptr_t)raw,.memory_map_size=sizeof raw,.descriptor_size=40,.descriptor_version=1};
 m.nr_ram=1;m.ram[0]=(struct boot_range){0x100000,0x7100000};
 e=(struct boot_memory_evidence){.live={{0x200000,0x300000},{0x210000,0x220000},{(uintptr_t)&b,(uintptr_t)&b+sizeof b},{(uintptr_t)raw,(uintptr_t)raw+sizeof raw}},.workspace={0x230000,0x240000},.map=&m};
 s=(struct desktop_surface){BASE,4096000,1280,800,1280,DESKTOP_BGR_BYTES};
 bar=(struct pci794_bar){.base=BASE,.raw_lo=0x80000008,.width=32,.memory=true,.prefetch=true};
 c=(struct s842_cache){.pat=0x0007040600070406,.cap=1,.def=0x806,.count=1,.bits=48,.pairs={{0x80000000,0xffff80000800}}};
 return (struct s850_gop_request){&b,&e,&s,&bar,0xff000008,0,&c,0x1000,read_table,0};
}
#define CHECK(x) do{assert(x);count++;}while(0)
static _Alignas(4096) uint64_t original[PG_TABLE_PAGES][512],dt[PG_TABLE_PAGES][512],ta[PG_TABLE_PAGES][512],tb[PG_TABLE_PAGES][512];
static _Alignas(4096) unsigned char mem[S844_BYTES];
static struct s842_root base;
static struct s850_desktop_root desktop;
static struct s846_root a,broot;
static struct s844_arena arena;
static struct s844_lease leases[2];
static struct s846_guard guard={{0x280000,0x281000,0x282000}};
static struct s850_gop_request req;
static uint64_t *leaf(const struct pg_plan *p,uint64_t va){
 size_t n=0;
 for(unsigned l=4;l>1;l--){uint64_t v=p->tables[n][(va>>(12+9*(l-1)))&511];if(!(v&1))return 0;n=((v&UINT64_C(0x000ffffffffff000))-p->tables_pa)/4096;assert(n<p->capacity);}
 return &p->tables[n][(va>>12)&511];
}
static bool valid(void){return rt861_roots_audit(&base,&req,&arena,leases,&guard,&desktop,&a,&broot);}
static unsigned refused;
#define REJECT(x) do{assert(!(x));refused++;}while(0)
static void corrupt_leaf(const struct pg_plan *p,uint64_t va){
 uint64_t *x=leaf(p,va);assert(x&&*x);uint64_t old=*x;
 for(unsigned bit=0;bit<64;bit++)if(bit!=5&&bit!=6){*x=old^(UINT64_C(1)<<bit);REJECT(valid());}
 *x=old|0x60;assert(valid());*x=old;
}
int main(void){
 req=reset();uint64_t pa=(uintptr_t)mem;
 assert(s844_init(&arena,pa)&&s844_acquire(&arena,S846_OWNER_A,3,&leases[0])&&s844_acquire(&arena,S846_OWNER_B,3,&leases[1]));
 struct pg_plan p={.arch=PG_X86,.pa_bits=48,.tables=original,.tables_pa=0x200000,.capacity=PG_TABLE_PAGES,.count=3,.probe=0x400000001000,.maps={{0x200000,0x200000,4*PG_TABLE_BYTES+3*4096,PG_WRITE},{0x100000,0x100000,4096,PG_EXEC},{pa,pa,S844_BYTES,PG_WRITE}}};
 assert(s842_build(&p,&m,&c,0xfee00000,0xfec00000,&base));
 assert(s850_desktop_build_guarded(&base,&req,&guard,dt,0x220000,&desktop));
 assert(s846_build(&base,&c,&arena,leases,0,&guard,ta,0x240000,&a));
 assert(s846_build(&base,&c,&arena,leases,1,&guard,tb,0x260000,&broot));
 CHECK(valid());
 for(unsigned i=0;i<3;i++){
  uint64_t va=S846_IST+i*3*4096;
  CHECK(*leaf(&desktop.plan,va)==*leaf(&a.plan,va)&&*leaf(&a.plan,va)==*leaf(&broot.plan,va));
  corrupt_leaf(&desktop.plan,va);corrupt_leaf(&a.plan,va);corrupt_leaf(&broot.plan,va);
 }
 for(uint64_t off=0;off<desktop.gop.mapping.bytes;off+=4096){
  uint64_t va=BASE+off;uint64_t *x=leaf(&a.plan,va),*y=leaf(&broot.plan,va);
  CHECK((!x||!*x)&&(!y||!*y));
 }
 for(uint64_t off=0;off<S844_BYTES;off+=4096){uint64_t *x=leaf(&a.plan,pa+off),*y=leaf(&broot.plan,pa+off);CHECK((!x||!*x)&&(!y||!*y));}
 corrupt_leaf(&desktop.plan,BASE);corrupt_leaf(&desktop.plan,BASE+999*4096);
 corrupt_leaf(&a.plan,S846_CODE);corrupt_leaf(&broot.plan,S846_STACK);
 corrupt_leaf(&a.plan,0xfee00000);corrupt_leaf(&desktop.plan,0xfec00000);
 /* Individually valid trees can still claim the same physical table pages.
  * Build a separate Desktop buffer with the PA of A, then reject the join. */
 assert(s850_desktop_build_guarded(&base,&req,&guard,dt,0x240000,&desktop));
 assert(s850_desktop_audit_guarded(&base,&req,&guard,&desktop));
 assert(s846_audit_pair(&base,&c,&arena,leases,&guard,&a,&broot));REJECT(valid());
 assert(s850_desktop_build_guarded(&base,&req,&guard,dt,0x220000,&desktop));CHECK(valid());
 REJECT(rt861_roots_audit(&base,&req,&arena,leases,0,&desktop,&a,&broot));
 REJECT(rt861_roots_audit(&base,&req,&arena,leases,&guard,&desktop,&broot,&a));
 REJECT(rt861_roots_audit(&base,&req,&arena,leases,&guard,&desktop,&a,&a));
 assert(s844_release(&arena,S846_OWNER_A,leases[0]));REJECT(valid());
 printf("S861 detached joint root audit: %u positive checks / %u refusals PASS (host model only)\n",count,refused);
 return 0;
}
