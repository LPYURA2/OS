#include "native_deep_drop.h"
#include <styleos/arch.h>
#include <styleos/native_runtime_865.h>

static struct dd_ipc ipc;
/* Profile is single-vCPU. Lifetime is in TSC ticks, independent of whether
 * either process runs. No promise of a calibrated wall-clock duration. */
#define DD_LIFETIME_TSC (UINT64_C(1)<<37)
static uint64_t ticks(void)
{
    uint32_t hi,lo;
    __asm__ volatile("lfence; rdtsc; lfence" : "=a"(lo),"=d"(hi) :: "memory");
    return ((uint64_t)hi<<32)|lo;
}
void native_drop_gesture(uint32_t from,uint32_t to)
{
    if (!ipc.broker.channel && dd_init(&ipc.broker,(struct nwc_service *)nwc_global())!=DD_OK)
        return;
    if (dd_ipc_gesture(&ipc,from,to,ticks(),DD_LIFETIME_TSC)==DD_OK)
        arch_puts("STYLEOS DD72 GESTURE\n");
}
void native_drop_reconcile(void)
{
    if ((ipc.gesture.id || ipc.broker.active.phase!=DD_IDLE) && dd_ipc_reconcile(&ipc,ticks()))
        arch_puts("STYLEOS DD72 REVOKED\n");
}
enum dd_status native_drop_call(const struct rt862_selection *t,unsigned op,uint64_t id,
                                uint64_t address,uint64_t size,uint64_t *result)
{
    *result=0;
    uint8_t *alias=0;
    if (op==DD_CALL_OFFER || op==DD_CALL_READ) {
        if (!size || size>DD_TEXT_MAX) return DD_INVALID;
        alias=(uint8_t *)s865_native_data_span(t->slot,t->pid,t->handle,t->child_root,address,size);
        if (!alias) return DD_INVALID;
    } else if (address || size) return DD_INVALID;
    struct dd_endpoint caller={.pid=t->pid,.generation=t->handle,.root=t->child_root};
    return dd_ipc_call(&ipc,caller,op,id,alias,size,ticks(),result);
}
