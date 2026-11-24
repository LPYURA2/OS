#include <styleos/desktop_gop_850.h>
#include <styleos/desktop_root_850.h>
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
#define REFUSE(...) do{r=reset();__VA_ARGS__;CHECK(!s850_gop_prepare(&r,&o)&&!o.ready&&!o.mapping.bytes);}while(0)

static _Alignas(4096) uint64_t original[PG_TABLE_PAGES][512],child_tables[PG_TABLE_PAGES][512],snapshot[PG_TABLE_PAGES][512];
static struct s842_root source;
static struct s850_desktop_root child;
#define LAPIC UINT64_C(0xfee00000)
#define IOAPIC UINT64_C(0xfec00000)
#define DEST UINT64_C(0x220000)
static struct s850_gop_request root_reset(void){
 struct s850_gop_request r=reset();
 struct pg_plan p={.arch=PG_X86,.pa_bits=48,.tables=original,.tables_pa=0x200000,.capacity=PG_TABLE_PAGES,.count=2,.probe=0x400000001000,.maps={{0x200000,0x200000,2*PG_TABLE_BYTES+3*4096,PG_WRITE},{0x400000,0x400000,4096,PG_EXEC}}};
 assert(s842_build(&p,&m,&c,LAPIC,IOAPIC,&source));memcpy(snapshot,original,sizeof original);return r;
}
static uint64_t *leaf(uint64_t va){
 size_t n=0;for(unsigned l=4;l>1;l--){uint64_t entry=child_tables[n][(va>>(12+9*(l-1)))&511];assert(entry&1);n=((entry&UINT64_C(0x000ffffffffff000))-DEST)/4096;assert(n<child.built.used);}
 return &child_tables[n][(va>>12)&511];
}
static void root_tests(void){
 struct s850_gop_request r=root_reset();
 unsigned before=count;
 #define BUILD() s850_desktop_build(&source,&r,child_tables,DEST,&child)
 #define AUDIT() s850_desktop_audit(&source,&r,&child)
 #define RESTORE() do{r=root_reset();assert(BUILD());}while(0)
 #define BAD(...) do{RESTORE();__VA_ARGS__;CHECK(!AUDIT());}while(0)
 CHECK(BUILD()&&child.ready&&child.built.root==DEST&&AUDIT());
 CHECK(!memcmp(original,snapshot,sizeof original)&&s842_audit(&source,&c));
 CHECK(child.plan.count==source.plan.count+1&&child.gop.pages==1000);
 CHECK(*leaf(BASE)==(BASE|UINT64_C(0x800000000000001b)));
 CHECK(*leaf(BASE+4096*999)==((BASE+4096*999)|UINT64_C(0x800000000000001b)));
 CHECK(*leaf(LAPIC)==(LAPIC|UINT64_C(0x800000000000001b))&&*leaf(IOAPIC)==(IOAPIC|UINT64_C(0x800000000000001b)));
 CHECK(*leaf(0x400000)==0x400001&&*leaf(DEST)==(DEST|UINT64_C(0x8000000000000003)));
 *leaf(BASE)|=0x60;child_tables[0][0]|=0x20;CHECK(AUDIT());
 BAD(*leaf(BASE)&=~UINT64_C(0x8000000000000000));
 BAD(*leaf(BASE)|=4);BAD(*leaf(BASE)&=~UINT64_C(16));BAD(*leaf(BASE)^=4096);BAD(*leaf(BASE)=0);
 BAD(*leaf(BASE+4096*999)=0);BAD(*leaf(LAPIC)^=8);BAD(*leaf(0x400000)|=2);
 BAD(child_tables[0][1]=child_tables[0][0]);BAD(child_tables[0][0]|=4);BAD(child_tables[0][0]=0x200003);
 BAD(child_tables[child.built.used][511]=1);BAD(child.built.used--);BAD(child.built.root=0x200000);
 BAD(child.plan.maps[0].flags=0);BAD(child.plan.maps[child.plan.count-1].bytes-=4096);BAD(child.plan.count--);
 BAD(child.gop.cache_bits=0);BAD(child.gop.pat_index=0);BAD(child.gop.pages++);BAD(child.gop.bar_bytes*=2);BAD(child.ready=false);
 BAD(r.mask_lo=0xffe00008);BAD(t[2][1]=0);BAD(original[0][1]=original[0][0]);
 r=root_reset();CHECK(!s850_desktop_build(&source,&r,original,DEST,&child)&&!child.ready&&!memcmp(original,snapshot,sizeof original));
 CHECK(!s850_desktop_build(&source,&r,child_tables,0x200000,&child)&&!child.ready);
 CHECK(!s850_desktop_build(&source,&r,child_tables,0x210000,&child)&&!child.ready);
 CHECK(!s850_desktop_build(&source,&r,child_tables,0x230000,&child)&&!child.ready);
 CHECK(!s850_desktop_build(&source,&r,child_tables,0x400000,&child)&&!child.ready);
 CHECK(!s850_desktop_build(&source,&r,child_tables,DEST+1,&child)&&!child.ready);
 CHECK(!s850_desktop_build(&source,&r,child_tables,LAPIC,&child)&&!child.ready);
 CHECK(!s850_desktop_build(&source,&r,child_tables,BASE,&child)&&!child.ready);
 CHECK(!s850_desktop_build(&source,&r,NULL,DEST,&child)&&!child.ready);
 CHECK(!s850_desktop_build(NULL,&r,child_tables,DEST,&child)&&!child.ready);
 CHECK(!s850_desktop_build(&source,NULL,child_tables,DEST,&child)&&!child.ready);
 CHECK(!s850_desktop_build(&source,&r,child_tables,DEST,NULL));
 CHECK(!s850_desktop_build(&source,&r,child_tables,DEST,(struct s850_desktop_root *)child_tables));
 CHECK(!s850_desktop_build(&source,&r,child_tables,DEST,(struct s850_desktop_root *)original)&&!memcmp(original,snapshot,sizeof original));
 CHECK(!s850_desktop_build(&source,&r,child_tables,DEST,(struct s850_desktop_root *)&source)&&s842_audit(&source,&c));
 printf("S850 desktop detached root: %u PASS (host only)\n",count-before);
 #undef BUILD
 #undef AUDIT
 #undef RESTORE
 #undef BAD
}

static void guarded_tests(void){
 struct s850_gop_request r=root_reset();struct s846_guard g={{0x240000,0x241000,0x242000}};unsigned before=count;
 #define GBUILD() s850_desktop_build_guarded(&source,&r,&g,child_tables,DEST,&child)
 #define GAUDIT() s850_desktop_audit_guarded(&source,&r,&g,&child)
 #define GRESET() do{r=root_reset();g=(struct s846_guard){{0x240000,0x241000,0x242000}};}while(0)
 #define GBAD(...) do{GRESET();assert(GBUILD());__VA_ARGS__;CHECK(!GAUDIT());}while(0)
 #define GREFUSE(...) do{GRESET();__VA_ARGS__;CHECK(!GBUILD()&&!child.ready&&!memcmp(original,snapshot,sizeof original));}while(0)
 CHECK(GBUILD()&&GAUDIT()&&!s850_desktop_audit(&source,&r,&child));
 CHECK(child.plan.count==source.plan.count+4&&!memcmp(original,snapshot,sizeof original));
 for(unsigned i=0;i<3;i++){
  uint64_t v=S846_IST+3*i*4096;
  CHECK(*leaf(v)==(g.pa[i]|UINT64_C(0x8000000000000003)));
  CHECK(*leaf(v-4096)==0&&*leaf(v+4096)==0);
  CHECK(*leaf(g.pa[i])==*leaf(v));
 }
 CHECK(*leaf(BASE)==(BASE|UINT64_C(0x800000000000001b)));
 GBAD(*leaf(S846_IST-4096)=*leaf(S846_IST));GBAD(*leaf(S846_IST+4096)=*leaf(S846_IST));
 GBAD(*leaf(S846_IST+3*4096)|=4);GBAD(*leaf(S846_IST+6*4096)&=~UINT64_C(0x8000000000000000));
 GBAD(*leaf(S846_IST)^=8);GBAD(*leaf(S846_IST)^=4096);GBAD(*leaf(S846_IST)=0);
 GBAD(g.pa[0]=g.pa[1]);GBAD(g.pa[2]=0x243000);GBAD(child.plan.maps[child.plan.count-1].flags=PG_EXEC);
 GBAD(child.plan.maps[child.plan.count-1].va+=4096);GBAD(child_tables[0][128]|=4);
 GREFUSE(g.pa[0]=g.pa[1]);GREFUSE(g.pa[0]++);GREFUSE(g.pa[0]=0);
 GREFUSE(g.pa[0]=0x200000);GREFUSE(g.pa[0]=DEST);GREFUSE(g.pa[0]=BASE);
 GREFUSE(g.pa[0]=0x400000);GREFUSE(g.pa[0]=0x243000);GREFUSE(g.pa[0]=LAPIC);
 GREFUSE(e.live.image.end=0x242000);GREFUSE(e.workspace=(struct boot_range){0x240000,0x241000});
 GREFUSE(m.nr_candidates=1;m.candidates[0]=(struct boot_range){0x241000,0x242000});
 GRESET();CHECK(!s850_desktop_build_guarded(&source,&r,(const struct s846_guard *)child_tables,child_tables,DEST,&child));
 GRESET();CHECK(!s850_desktop_build_guarded(&source,&r,(const struct s846_guard *)&child,child_tables,DEST,&child));
 printf("S850 guarded IST composition: %u PASS (host only)\n",count-before);
 #undef GBUILD
 #undef GAUDIT
 #undef GRESET
 #undef GBAD
 #undef GREFUSE
}

int main(void){struct s850_gop_request r=reset();struct s850_gop_plan o;
 CHECK(s850_gop_prepare(&r,&o)&&o.ready&&o.pages==1000&&o.mapping.va==BASE&&o.mapping.pa==BASE&&o.mapping.bytes==4096000&&o.mapping.flags==PG_WRITE&&o.cache_bits==24&&o.pat_index==3&&o.bar_bytes==16777216);
 r=reset();b.memory_map_size=40;e.live.source.end=e.live.source.start+40;CHECK(s850_gop_prepare(&r,&o));
 REFUSE(s.base+=4);REFUSE(s.bytes--);REFUSE(s.width=319);REFUSE(s.stride=1279);REFUSE(s.height=799;s.bytes=4u*s.stride*s.height);REFUSE(s.format=2);
 REFUSE(r.mask_lo=0xffe00008);REFUSE(r.mask_hi=1);REFUSE(bar.memory=false);REFUSE(bar.base+=4096);REFUSE(bar.raw_lo^=8);REFUSE(bar.raw_lo^=0x1000000);REFUSE(bar.prefetch=false);REFUSE(s.bytes=0x2000000);
 REFUSE(m.nr_ram=2;m.ram[1]=(struct boot_range){BASE+0x300000,BASE+0x301000});
 REFUSE(m.nr_candidates=1;m.candidates[0]=(struct boot_range){BASE,BASE+4096});
 REFUSE(e.workspace=(struct boot_range){BASE+0x300000,BASE+0x301000});
 REFUSE(m.nr_reserved=1;m.reserved[0]=(struct boot_range){9,8});
 REFUSE(b.flags=0);REFUSE(b.descriptor_size=39);REFUSE(b.memory_map_size=41);REFUSE(e.live.source.end--);REFUSE(e.live.handoff.start++);
 REFUSE(descriptor(1,7,BASE,0x1000,8));REFUSE(descriptor(1,11,BASE,0x1000,UINT64_C(1)<<63));REFUSE(descriptor(1,11,BASE,0,1));REFUSE(descriptor(1,11,0x200000,1,1));
 REFUSE(c.def=6);REFUSE(c.count=0);REFUSE(c.pairs[0].base|=6);REFUSE(c.pat=0x0606060606060606);REFUSE(c.pat=0x0207040600070406);REFUSE(c.bits=35);REFUSE(c.pairs[0].mask^=0x200000000);REFUSE(c.pairs[0].base|=0x1000000);REFUSE(c.pairs[0].mask|=1);
 REFUSE(t[2][1]=0);REFUSE(t[2][1]=(BASE+0x400000)|0x83);REFUSE(t[2][1]|=8);REFUSE(t[0][0]=0x9003);REFUSE(r.inherited_cr3=0);REFUSE(r.read_entry=NULL);
 CHECK(!s850_gop_prepare(NULL,&o)&&!o.ready);CHECK(!s850_gop_prepare(&r,NULL));
 printf("S850 GOP detached mapping proposal: %u PASS (host only)\n",count);root_tests();guarded_tests();printf("S850 combined: %u PASS\n",count);return 0;}
