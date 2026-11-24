#include <styleos/desktop_v0.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 1280u
#define H 800u
#define N (W*H)
static uint32_t ram[N+2], before[N];
void arch_puts(const char *s){fputs(s,stderr);}
_Noreturn void arch_halt(void){abort();}
int main(void){
    struct desktop_surface s={(uintptr_t)(ram+1),4u*N,W,H,W,DESKTOP_BGR_BYTES};
    ram[0]=ram[N+1]=0xdeadbeefu;
    assert(desk_first_frame_852(&s)==N);
    memcpy(before,ram+1,sizeof before);
    struct desk_event e={.mouse=true,.dx=20};
    assert(!desk_cursor_853(NULL));
    e.buttons=1;assert(!desk_cursor_853(&e));e.buttons=0;
    e.dx=21;assert(!desk_cursor_853(&e));e.dx=20;
    assert(!memcmp(before,ram+1,sizeof before));
    size_t writes=desk_cursor_853(&e);
    assert(writes&&writes<=340&&ram[0]==0xdeadbeefu&&ram[N+1]==0xdeadbeefu);
    uint64_t h=UINT64_C(14695981039346656037);size_t changed=0;
    for(size_t i=0;i<N;i++){
        uint32_t p=ram[i+1];
        if(p!=before[i]){
            size_t x=i%W,y=i/W;
            assert(y>=600&&y<617&&((x>=640&&x<650)||(x>=660&&x<670)));
            changed++;
        }
        for(unsigned j=0;j<4;j++){h^=(p>>(8*j))&255u;h*=UINT64_C(1099511628211);}
    }
    assert(changed&&changed<=writes&&!desk_cursor_853(&e));
    printf("%zu %" PRIu64 "\n",writes,h);
    return 0;
}
