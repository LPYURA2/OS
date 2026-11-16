#include "styleos/user_processes.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static struct up_control control;
static struct up_spec spec(unsigned id)
{
    uint64_t stack=id?UP_STACK_B_VA:UP_STACK_A_VA;
    return (struct up_spec){UINT64_C(0x2000)+(uint64_t)id*UP_PAGE,UP_CODE_VA,
        UP_CODE_VA,id?UP_DATA_B_VA:UP_DATA_A_VA,{stack,stack+UP_PAGE}};
}
static void init(void){up_init(&control,0x1000);}
static void create_both(void)
{
    init();for(unsigned n=0;n<2;++n){struct up_spec s=spec(n);unsigned id=99;
        assert(up_create(&control,&s,&id)==UP_OK);assert(id==n);}
}
#define REFUSAL(call,expected) do { \
    struct up_control before=control; \
    assert((call)==(expected)); \
    assert(memcmp(&before,&control,sizeof control)==0); \
} while(0)
static void lifecycle(void)
{
    create_both();unsigned id=99;
    REFUSAL(up_release(&control,0,0x1000),UP_STATE);
    assert(up_go(&control,&id)==UP_OK && id==0);
    REFUSAL(up_go(&control,&id),UP_STATE);
    REFUSAL(up_accept(&control,UP_EXIT,0x1000),UP_OPERATION);
    REFUSAL(up_accept(&control,UP_FAULT,0x1000),UP_OPERATION);
    REFUSAL(up_accept(&control,UP_YIELD,0x2000),UP_STATE);
    assert(up_accept(&control,UP_YIELD,0x1000)==UP_OK);
    REFUSAL(up_accept(&control,UP_YIELD,0x1000),UP_STATE);
    assert(control.slots[0].state==UP_READY && control.slots[0].yields==1);
    assert(up_go(&control,&id)==UP_OK && id==1);
    REFUSAL(up_accept(&control,UP_EXIT,0x1000),UP_OPERATION);
    assert(up_accept(&control,UP_YIELD,0x1000)==UP_OK);
    assert(up_go(&control,&id)==UP_OK && id==0);
    REFUSAL(up_accept(&control,UP_YIELD,0x1000),UP_OPERATION);
    assert(up_accept(&control,UP_FAULT,0x1000)==UP_OK);
    assert(control.slots[0].state==UP_FAULTED);
    REFUSAL(up_release(&control,0,0x1000),UP_STATE);
    assert(up_go(&control,&id)==UP_OK && id==1);
    REFUSAL(up_accept(&control,UP_FAULT,0x1000),UP_OPERATION);
    assert(up_accept(&control,UP_EXIT,0x1000)==UP_OK);
    id=99;REFUSAL(up_go(&control,&id),UP_EMPTY);assert(id==99);
    assert(control.slots[0].state==UP_FAULTED && control.slots[1].state==UP_EXITED);
    REFUSAL(up_release(&control,0,0x3000),UP_STATE);
    REFUSAL(up_release(&control,2,0x1000),UP_ARGUMENT);
    assert(up_release(&control,0,0x1000)==UP_OK);
    assert(control.slots[0].state==UP_FREE);
    assert(!memcmp(&control.slots[0],&(struct up_slot){0},sizeof(struct up_slot)));
    REFUSAL(up_release(&control,0,0x1000),UP_STATE);
    assert(up_release(&control,1,0x1000)==UP_OK);
    assert(control.released==2 && control.dispatches==4 && control.phase==4);
    id=123;REFUSAL(up_go(&control,&id),UP_EMPTY);assert(id==123);
    struct up_spec s=spec(0);REFUSAL(up_create(&control,&s,&id),UP_STATE);
}
static void arguments(void)
{
    init();struct up_spec s=spec(0);unsigned id=99;
    REFUSAL(up_create(&control,NULL,&id),UP_ARGUMENT);
    REFUSAL(up_create(&control,&s,NULL),UP_ARGUMENT);
    REFUSAL(up_create(&control,&s,(unsigned *)(void *)&control),UP_ARGUMENT);
    REFUSAL(up_create(&control,(const struct up_spec *)(const void *)&control,&id),UP_ARGUMENT);
    REFUSAL(up_go(&control,(unsigned *)(void *)&control),UP_ARGUMENT);
    REFUSAL(up_go(&control,NULL),UP_ARGUMENT);
    REFUSAL(up_accept(&control,UP_YIELD,0x1000),UP_STATE);
    struct up_control snapshot=control;up_init(&control,0);assert(!memcmp(&control,&snapshot,sizeof control));
    up_init(&control,0x1001);assert(!memcmp(&control,&snapshot,sizeof control));
    assert(up_go(NULL,&id)==UP_ARGUMENT);up_init(NULL,0x1000);
    s.root=0;REFUSAL(up_create(&control,&s,&id),UP_ALIGNMENT);
    s=spec(0);s.root=0x1000;REFUSAL(up_create(&control,&s,&id),UP_ALIGNMENT);
    s=spec(0);s.root|=1;REFUSAL(up_create(&control,&s,&id),UP_ALIGNMENT);
    s=spec(0);s.stack_va.end--;REFUSAL(up_create(&control,&s,&id),UP_ALIGNMENT);
    s=spec(0);s.stack_va.end+=UP_PAGE;REFUSAL(up_create(&control,&s,&id),UP_RANGE);
    s=spec(0);s.entry=s.code_va+UP_PAGE;REFUSAL(up_create(&control,&s,&id),UP_RANGE);
    s=spec(0);s.code_va=UINT64_MAX-(UP_PAGE-1u);REFUSAL(up_create(&control,&s,&id),UP_OVERFLOW);
    s=spec(0);s.data_va=s.code_va;REFUSAL(up_create(&control,&s,&id),UP_OVERLAP);
    s=spec(0);s.data_va=s.stack_va.start;REFUSAL(up_create(&control,&s,&id),UP_OVERLAP);
    assert(id==99);
}
static void capacity_and_corruption(void)
{
    create_both();unsigned id=99;struct up_spec s=spec(0);
    REFUSAL(up_create(&control,&s,&id),UP_OVERLAP);
    s.root=0x4000;s.stack_va.start=0x11020000;s.stack_va.end=s.stack_va.start+UP_PAGE;s.data_va=0x12020000;
    REFUSAL(up_create(&control,&s,&id),UP_FULL);assert(id==99);
    control.current=2;REFUSAL(up_go(&control,&id),UP_STATE);
    create_both();control.slots[1].spec.root=control.slots[0].spec.root;REFUSAL(up_go(&control,&id),UP_STATE);
    create_both();control.slots[1].state=UP_RUNNING;REFUSAL(up_go(&control,&id),UP_STATE);
    create_both();control.slots[0].context.pc=UP_CODE_VA+UP_PAGE;REFUSAL(up_go(&control,&id),UP_CONTEXT);
    create_both();control.slots[0].context.sp=UP_STACK_A_VA-1;REFUSAL(up_go(&control,&id),UP_CONTEXT);
    create_both();control.dispatches=UINT_MAX;REFUSAL(up_go(&control,&id),UP_STATE);
    create_both();control.phase=5;REFUSAL(up_go(&control,&id),UP_STATE);
    create_both();control.next=2;REFUSAL(up_go(&control,&id),UP_STATE);
    init();control.slots[0].context.gpr[0]=1;REFUSAL(up_go(&control,&id),UP_STATE);
}
int main(void)
{
    lifecycle();arguments();capacity_and_corruption();
    puts("user-processes host PASS: lifecycle, four-event order, safe-root release, refusals unchanged");
    return 0;
}
