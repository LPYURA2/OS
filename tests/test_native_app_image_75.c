#include <styleos/native_app_image_75.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) uint8_t memory[S844_BYTES];
static uint8_t elf[5*NTE_PAGE],hash[32],before[S844_BYTES];
static struct s844_arena arena;
static struct s844_lease lease;
static struct nai75_layout plan;
static unsigned checks;
#define CHECK(x) do {assert(x);++checks;}while(0)
static void wr(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)p[i]=(uint8_t)(v>>(i*8));}
static void ph(unsigned n,unsigned flags,uint64_t off,uint64_t va,uint64_t file,uint64_t mem){
 uint8_t *p=elf+64+n*56;wr(p,1,4);wr(p+4,flags,4);wr(p+8,off,8);
 wr(p+16,va,8);wr(p+32,file,8);wr(p+40,mem,8);wr(p+48,NTE_PAGE,8);
}
static void fixture(void){
 memset(elf,0,sizeof elf);
 memcpy(elf,"\177ELF\2\1\1",7);
 wr(elf+16,2,2);wr(elf+18,62,2);wr(elf+20,1,4);
 wr(elf+24,NTE_USER_CODE_VA+4097,8);wr(elf+32,64,8);
 wr(elf+52,64,2);wr(elf+54,56,2);wr(elf+56,2,2);
 ph(0,5,NTE_PAGE,NTE_USER_CODE_VA,5000,6000);
 ph(1,6,3*NTE_PAGE,NAI75_DATA_VA,4100,9000);
 for(size_t i=0;i<5000;++i)elf[NTE_PAGE+i]=(uint8_t)(i*13+1);
 for(size_t i=0;i<4100;++i)elf[3*NTE_PAGE+i]=(uint8_t)(i*7+9);
 nte_sha256(elf,sizeof elf,hash);
}
static void refuse(enum nte_status status,struct s844_lease l,unsigned stack){
 struct nai75_layout sentinel={.entry=99,.code_pages=77};
 plan=sentinel;memcpy(before,memory,sizeof memory);
 CHECK(nai75_stage(&arena,l,elf,sizeof elf,hash,stack,&plan)==status);
 CHECK(!memcmp(before,memory,sizeof memory));
 CHECK(!memcmp(&sentinel,&plan,sizeof plan));
}
int main(void){
 CHECK(s844_init(&arena,(uintptr_t)memory));
 fixture();
 CHECK(nai75_inspect(elf,sizeof elf,hash,2,&plan)==NTE_OK);
 CHECK(plan.entry==NTE_USER_CODE_VA+4097&&plan.code_pages==2&&plan.data_pages==3&&plan.stack_pages==2&&plan.total_pages==7);
 CHECK(s844_acquire(&arena,7,7,&lease));
 struct s844_lease peer;CHECK(s844_acquire(&arena,8,3,&peer));
 memset(memory,0xa5,sizeof memory);
 CHECK(nai75_stage(&arena,lease,elf,sizeof elf,hash,2,&plan)==NTE_OK);
 uint8_t *out=(uint8_t *)(uintptr_t)lease.pa;
 CHECK(!memcmp(out,elf+NTE_PAGE,5000));
 CHECK(!memcmp(out+2*NTE_PAGE,elf+3*NTE_PAGE,4100));
 for(size_t i=5000;i<2*NTE_PAGE;++i)CHECK(out[i]==0);
 for(size_t i=2*NTE_PAGE+4100;i<7*NTE_PAGE;++i)CHECK(out[i]==0);
 for(size_t i=0;i<3*NTE_PAGE;++i)CHECK(((uint8_t *)(uintptr_t)peer.pa)[i]==0xa5);
 fixture();elf[0]=0;refuse(NTE_FORMAT,lease,2);
 fixture();wr(elf+56,3,2);refuse(NTE_FORMAT,lease,2);
 fixture();wr(elf+32,UINT64_MAX,8);refuse(NTE_BOUNDS,lease,2);
 fixture();wr(elf+64+4,7,4);refuse(NTE_RIGHTS,lease,2);
 fixture();wr(elf+120+4,7,4);refuse(NTE_RIGHTS,lease,2);
 fixture();wr(elf+120+16,NTE_USER_CODE_VA,8);refuse(NTE_RIGHTS,lease,2);
 fixture();wr(elf+24,NTE_USER_CODE_VA+5000,8);refuse(NTE_RIGHTS,lease,2);
 fixture();wr(elf+64+8,0,8);refuse(NTE_BOUNDS,lease,2);
 fixture();wr(elf+120+8,2*NTE_PAGE,8);refuse(NTE_BOUNDS,lease,2);
 fixture();wr(elf+120+8,UINT64_MAX,8);refuse(NTE_BOUNDS,lease,2);
 fixture();wr(elf+120+40,UINT64_MAX,8);refuse(NTE_RIGHTS,lease,2);
 fixture();wr(elf+64+32,6001,8);refuse(NTE_RIGHTS,lease,2);
 fixture();elf[NTE_PAGE+1]^=1;refuse(NTE_HASH,lease,2);
 fixture();refuse(NTE_ARGUMENT,lease,0);refuse(NTE_ARGUMENT,lease,5);
 refuse(NTE_BOUNDS,peer,2);refuse(NTE_BOUNDS,lease,1);
 struct s844_lease stale=lease;stale.generation++;refuse(NTE_ARGUMENT,stale,2);
 memcpy(before,memory,sizeof memory);
 CHECK(nai75_stage(&arena,lease,elf,sizeof elf,hash,2,(struct nai75_layout *)(void *)out)==NTE_ARGUMENT);
 CHECK(!memcmp(before,memory,sizeof memory));
 /* BSS-only data segment is valid but still requires its own zeroed pages. */
 fixture();wr(elf+120+32,0,8);nte_sha256(elf,sizeof elf,hash);
 CHECK(nai75_stage(&arena,lease,elf,sizeof elf,hash,2,&plan)==NTE_OK);
 CHECK(plan.data_file==0);
 for(size_t i=2*NTE_PAGE;i<7*NTE_PAGE;++i)CHECK(out[i]==0);
 CHECK(s844_release(&arena,lease.owner,lease));
 for(size_t i=0;i<7*NTE_PAGE;++i)CHECK(out[i]==0);
 refuse(NTE_ARGUMENT,lease,2);
 CHECK(s844_release(&arena,peer.owner,peer));
 unsigned used=99;CHECK(s844_count(&arena,&used)&&used==0);
 puts("Native app image 75: detached multi-page RX/RW/stack staging, refusals, peer preservation and scrub");
 printf("%u checks PASS\n",checks);
}
