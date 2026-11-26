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
static _Alignas(4096) uint64_t original[PG_TABLE_PAGES][512],dt[PG_TABLE_PAGES][512],ct[4][PG_TABLE_PAGES][512];
static _Alignas(4096) unsigned char mem[S844_BYTES];
static struct s842_root base;
static struct s850_desktop_root desktop;
static struct s846_root roots[4];
static struct s844_arena arena;
static struct s844_lease leases[4];
static struct s846_guard guard={{0x200000+6*PG_TABLE_BYTES,0x201000+6*PG_TABLE_BYTES,0x202000+6*PG_TABLE_BYTES}};
static struct s850_gop_request req;
static uint64_t *leaf(const struct pg_plan *p,uint64_t va){
 size_t n=0;
 for(unsigned l=4;l>1;l--){uint64_t v=p->tables[n][(va>>(12+9*(l-1)))&511];if(!(v&1))return 0;n=((v&UINT64_C(0x000ffffffffff000))-p->tables_pa)/4096;assert(n<p->capacity);}
 return &p->tables[n][(va>>12)&511];
}
static bool valid(void){return rt864_roots_audit_set(&base,&req,&arena,leases,&guard,&desktop,roots);}
static unsigned refused;
#define REJECT(x) do{assert(!(x));refused++;}while(0)
int main(void){
 req=reset();uint64_t pa=(uintptr_t)mem;
 CHECK(s844_init(&arena,pa));
 for(unsigned i=0;i<4;i++)CHECK(s844_acquire(&arena,S846_OWNER_A+i,3,&leases[i]));
 struct pg_plan p={.arch=PG_X86,.pa_bits=48,.tables=original,.tables_pa=0x200000,.capacity=PG_TABLE_PAGES,.count=3,.probe=0x400000001000,.maps={{0x200000,0x200000,6*PG_TABLE_BYTES+3*4096,PG_WRITE},{0x100000,0x100000,4096,PG_EXEC},{pa,pa,S844_BYTES,PG_WRITE}}};
 CHECK(s842_build(&p,&m,&c,0xfee00000,0xfec00000,&base));
 CHECK(s850_desktop_build_guarded(&base,&req,&guard,dt,0x200000+PG_TABLE_BYTES,&desktop));
 for(unsigned i=0;i<4;i++)CHECK(s846_build(&base,&c,&arena,leases,i,&guard,ct[i],0x200000+(i+2)*PG_TABLE_BYTES,&roots[i]));
 CHECK(valid());
 for(unsigned i=0;i<4;i++){
  for(uint64_t off=0;off<desktop.gop.mapping.bytes;off+=4096){uint64_t *x=leaf(&roots[i].plan,BASE+off);CHECK(!x||!*x);}
  for(unsigned k=0;k<3;k++){
   uint64_t va=S846_IST+k*3*4096,*x=leaf(&roots[i].plan,va),old=*x;
   CHECK(old==*leaf(&desktop.plan,va));*x|=4;REJECT(valid());*x=old;
  }
  for(unsigned k=0;k<4;k++){
   uint64_t *x=leaf(&roots[i].plan,S846_DATA),old=*x;
   if(k!=i){*x=(leases[k].pa+4096)|(old&UINT64_C(0x8000000000000fff));REJECT(valid());*x=old;}
  }
 }
 for(unsigned i=0;i<4;i++){
  CHECK(s850_desktop_build_guarded(&base,&req,&guard,dt,roots[i].plan.tables_pa,&desktop));
  CHECK(s850_desktop_audit_guarded(&base,&req,&guard,&desktop));REJECT(valid());
 }
 CHECK(s850_desktop_build_guarded(&base,&req,&guard,dt,0x200000+PG_TABLE_BYTES,&desktop));CHECK(valid());
 struct s844_lease old=leases[3];CHECK(s844_release(&arena,old.owner,old));
 CHECK(s844_acquire(&arena,old.owner,3,&leases[3]));REJECT(valid());
 memset(&roots[3],0,sizeof roots[3]);
 CHECK(s846_build(&base,&c,&arena,leases,3,&guard,ct[3],0x200000+5*PG_TABLE_BYTES,&roots[3]));CHECK(valid());
 memset(roots,0,sizeof roots);memset(ct,0,sizeof ct);
 for(unsigned i=0;i<4;i++)CHECK(s844_release(&arena,leases[i].owner,leases[i]));
 unsigned used;CHECK(s844_count(&arena,&used)&&!used);
 memset(leases,0,sizeof leases);
 CHECK(rt865_roots_audit_sparse(&base,&req,&arena,leases,&guard,&desktop,roots));
 CHECK(s844_acquire(&arena,S846_OWNER_A+3,3,&leases[3]));
 REJECT(rt865_roots_audit_sparse(&base,&req,&arena,leases,&guard,&desktop,roots));
 CHECK(s865_root_build(&base,&c,&arena,leases,3,&guard,ct[3],0x200000+5*PG_TABLE_BYTES,&roots[3]));
 CHECK(rt865_roots_audit_sparse(&base,&req,&arena,leases,&guard,&desktop,roots));
 CHECK(s844_acquire(&arena,S846_OWNER_A,3,&leases[0]));
 CHECK(s865_root_build(&base,&c,&arena,leases,0,&guard,ct[0],0x200000+2*PG_TABLE_BYTES,&roots[0]));
 CHECK(rt865_roots_audit_sparse(&base,&req,&arena,leases,&guard,&desktop,roots));
 uint64_t *x=leaf(&roots[3].plan,S846_CODE),saved=*x;*x=leases[0].pa|5;
 REJECT(rt865_roots_audit_sparse(&base,&req,&arena,leases,&guard,&desktop,roots));*x=saved;
 CHECK(s844_release(&arena,leases[0].owner,leases[0]));memset(&leases[0],0,sizeof leases[0]);
 REJECT(rt865_roots_audit_sparse(&base,&req,&arena,leases,&guard,&desktop,roots));
 memset(&roots[0],0,sizeof roots[0]);memset(ct[0],0,sizeof ct[0]);
 CHECK(rt865_roots_audit_sparse(&base,&req,&arena,leases,&guard,&desktop,roots));
 CHECK(s844_release(&arena,leases[3].owner,leases[3]));memset(&leases[3],0,sizeof leases[3]);
 memset(&roots[3],0,sizeof roots[3]);memset(ct[3],0,sizeof ct[3]);
 CHECK(rt865_roots_audit_sparse(&base,&req,&arena,leases,&guard,&desktop,roots));
 printf("S864 joint four roots HOST ONLY: %u checks, %u refusals PASS\n",count,refused);
}
