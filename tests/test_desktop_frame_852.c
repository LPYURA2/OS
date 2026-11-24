/* Host reference: actual shell renderer, no desktop hardware adapter. */
#include <styleos/desktop_v0.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#define W 1280u
#define H 800u
#define N (W*H)
static uint32_t ram[N+2];
void arch_puts(const char *s){fputs(s,stderr);}
_Noreturn void arch_halt(void){abort();}
int main(void)
{
    struct desktop_surface s={(uintptr_t)(ram+1),4u*N,W,H,W,DESKTOP_BGR_BYTES};
    struct desktop_surface bad=s;bad.width=799;
    assert(!desk_first_frame_852(&bad));
    bad=s;bad.bytes--;assert(!desk_first_frame_852(&bad));
    for(size_t i=0;i<N+2;i++)ram[i]=0xdeadbeefu;
    size_t writes=desk_first_frame_852(&s);assert(writes==N);
    assert(ram[0]==0xdeadbeefu&&ram[N+1]==0xdeadbeefu);
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=1;i<=N;i++){
        assert(ram[i]!=0xdeadbeefu);
        for(unsigned b=0;b<4;b++){
            hash^=(ram[i]>>(8*b))&255u;hash*=UINT64_C(1099511628211);
        }
    }
    printf("%" PRIu64 " %" PRIu64 " %u %u\n",(uint64_t)writes,hash,ram[1],ram[N]);
    return 0;
}
