#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MS __attribute__((ms_abi))
struct context { uint64_t gpr[12], sp, pc; };
extern void MS tasks_arch_switch_host(struct context *, const struct context *) __asm__("tasks_arch_switch");
extern void MS tasks_arch_dispatch_host(struct context *, const struct context *) __asm__("tasks_arch_dispatch");
extern uint64_t MS tasks_arch_probe_host(struct context *, const struct context *, uint64_t) __asm__("tasks_arch_probe");
extern _Noreturn void MS tasks_arch_finish_host(struct context *, const struct context *) __asm__("tasks_arch_finish");
extern unsigned char styleos_tasks_dispatch_resume[], styleos_tasks_probe_resume[], styleos_tasks_exit_resume[], styleos_tasks_trap_return[];
static _Alignas(4096) unsigned char stacks[2][16384];
static struct context task[2], dispatcher;
static unsigned current, state[2], phase[2], events;
static unsigned const expected[6]={0,1,0,1,0,1};
_Noreturn void MS tasks_completed_resumed(void) { abort(); }
_Noreturn static void MS bootstrap(void) {
    unsigned id=current;
    volatile uint64_t cookie=UINT64_C(0x987654321abcdef0)+id;
    for(unsigned n=0;n<3;n++) {
        uintptr_t p=(uintptr_t)&cookie;
        assert(p>=(uintptr_t)stacks[id]+16 && p+sizeof cookie<=(uintptr_t)stacks[id]+16384-16);
        assert(current==id && events<6 && expected[events++]==id);
        assert(cookie==UINT64_C(0x987654321abcdef0)+id);
        phase[id]++;
        if(n<2) {
            state[id]=1;
            uint64_t mask=tasks_arch_probe_host(&task[id],&dispatcher,0xa101+id*0x1100+n);
            assert(mask==0xff && state[id]==2 && current==id);
        } else {
            state[id]=3;
            tasks_arch_finish_host(&task[id],&dispatcher);
        }
    }
    abort();
}
int main(void) {
    for(unsigned id=0;id<2;id++) {
        uintptr_t top=(uintptr_t)stacks[id]+sizeof stacks[id]-16;
        memset(stacks[id],0xa5,16); memset(stacks[id]+16384-16,0x5a,16);
        task[id].sp=top-40;
        task[id].pc=(uintptr_t)bootstrap;
        *(uint64_t *)(uintptr_t)task[id].sp=(uintptr_t)styleos_tasks_trap_return;
        assert(task[id].sp%16==8); state[id]=1;
    }
    volatile uint64_t dispatch_cookie=UINT64_C(0xd15ca11c12345678);
    for(unsigned n=0;n<6;n++) {
        unsigned id=n%2; current=id; assert(state[id]==1); state[id]=2;
        tasks_arch_dispatch_host(&dispatcher,&task[id]);
        assert(dispatcher.pc==(uintptr_t)styleos_tasks_dispatch_resume && dispatcher.sp%16==0);
        assert(dispatch_cookie==UINT64_C(0xd15ca11c12345678));
        assert(task[id].sp>=(uintptr_t)stacks[id]+16 && task[id].sp<=(uintptr_t)stacks[id]+16384-16);
        if(n<4) {
            assert(state[id]==1 && task[id].pc==(uintptr_t)styleos_tasks_probe_resume);
            for(unsigned r=0;r<8;r++) assert(task[id].gpr[r]==UINT64_C(0x1122000000000000)+(UINT64_C(1)<<32)*r+0xa101+id*0x1100+n/2);
        } else assert(state[id]==3 && task[id].pc==(uintptr_t)styleos_tasks_exit_resume);
    }
    assert(events==6 && phase[0]==3 && phase[1]==3);
    for(unsigned id=0;id<2;id++) for(unsigned i=0;i<16;i++) {
        assert(stacks[id][i]==0xa5 && stacks[id][16384-16+i]==0x5a);
    }
    puts("Native x86 Microsoft ABI: six real stack/continuation switches, probe ff, saved raw register patterns, dispatcher/task C cookies, finish and canaries PASS");
}
