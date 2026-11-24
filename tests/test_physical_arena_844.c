#include <styleos/physical_arena_844.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) unsigned char memory[S844_BYTES];
static struct s844_arena arena;
static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
static void same_refusal(uint32_t owner,struct s844_lease v){
 struct s844_arena before=arena;unsigned char snapshot[S844_BYTES];
 memcpy(snapshot,memory,sizeof memory);CHECK(!s844_release(&arena,owner,v));
 CHECK(!memcmp(&before,&arena,sizeof arena));CHECK(!memcmp(snapshot,memory,sizeof memory));
}
static void descriptor(unsigned char *p,unsigned type,uint64_t base,uint64_t count,uint64_t flags){
 memset(p,0,48);p[0]=(unsigned char)type;
 const uint64_t v[]={base,0,count,flags};
 for(unsigned i=0;i<4;i++)for(unsigned j=0;j<8;j++)p[8+i*8+j]=(unsigned char)(v[i]>>(j*8));
}
int main(void){
 uint64_t base=(uintptr_t)memory;struct s844_lease a,b,c,out={0};unsigned n;
 memset(memory,0xa5,sizeof memory);CHECK(!s844_init(&arena,base+1));
 CHECK(s844_init(&arena,base));CHECK(!s844_init(&arena,base));
 CHECK(s844_acquire(&arena,1,4,&a));CHECK(a.pa==base&&a.generation==1&&a.pages==4);
 for(size_t i=0;i<4*4096u;i++)CHECK(memory[i]==0);
 CHECK(memory[4*4096u]==0xa5);
 CHECK(s844_acquire(&arena,2,12,&b));CHECK(b.pa==base+4*4096u&&b.generation==2);
 CHECK(s844_count(&arena,&n)&&n==16);CHECK(!s844_acquire(&arena,3,1,&out));
 CHECK(!s844_acquire(&arena,0,1,&out));CHECK(!s844_acquire(&arena,3,17,&out));
 memset(memory,0x7b,sizeof memory);
 same_refusal(2,a);struct s844_lease bad=a;bad.pages=3;same_refusal(1,bad);
 bad=a;bad.pa+=4096;same_refusal(1,bad);
 CHECK(s844_release(&arena,1,a));CHECK(memory[0]==0&&memory[4*4096u]==0x7b);
 same_refusal(1,a);
 CHECK(s844_acquire(&arena,1,4,&c));CHECK(c.pa==a.pa&&c.generation==3);same_refusal(1,a);
 CHECK(s844_release(&arena,1,c));CHECK(s844_release(&arena,2,b));
 CHECK(s844_count(&arena,&n)&&n==0);
 CHECK(!s844_acquire(&arena,1,1,(void*)memory));CHECK(!s844_acquire(&arena,1,1,&arena.slots[0]));
 arena.next=UINT64_MAX;CHECK(!s844_acquire(&arena,1,1,&out));
 unsigned char raw[96];struct boot_range r={0x200000,0x210000},d={0};uint64_t attr=0;
 descriptor(raw,2,0x200000,16,8);descriptor(raw+48,7,0x300000,1,8);
 struct boot_memory_map map={.reserved={{0x200000,0x210000}},.nr_reserved=1};
 struct boot_info boot={.magic=STYLEOS_BOOT_MAGIC,.version=1,.size=sizeof boot,.arch=STYLEOS_ARCH_X86_64,
 .platform=STYLEOS_PLATFORM_UEFI,.flags=STYLEOS_BOOT_EXITED_SERVICES,.memory_map=(uintptr_t)raw,
 .memory_map_size=96,.descriptor_size=48,.descriptor_version=1};
 struct boot_memory_evidence ev={.live={{0x100000,0x110000},{0x102000,0x104000},{0x104000,0x104100},{(uintptr_t)raw,(uintptr_t)raw+sizeof raw}},
 .workspace={0x105000,0x106000},.map=&map};
 CHECK(s844_efi_proof(&ev,&boot,r,&d,&attr)&&d.start==r.start&&d.end==r.end&&attr==8);
 raw[0]=7;CHECK(!s844_efi_proof(&ev,&boot,r,&d,&attr));raw[0]=2;
 raw[32]=0;CHECK(!s844_efi_proof(&ev,&boot,r,&d,&attr));raw[32]=8;
 map.nr_candidates=1;map.candidates[0]=r;CHECK(!s844_efi_proof(&ev,&boot,r,&d,&attr));map.nr_candidates=0;
 map.nr_reserved=0;CHECK(!s844_efi_proof(&ev,&boot,r,&d,&attr));map.nr_reserved=1;
 descriptor(raw+48,2,0x201000,1,8);CHECK(!s844_efi_proof(&ev,&boot,r,&d,&attr));
 descriptor(raw+48,7,0x300000,1,8);ev.workspace=r;CHECK(!s844_efi_proof(&ev,&boot,r,&d,&attr));
 printf("S844 physical arena host assertions: %u PASS\n",checks);return 0;
}
