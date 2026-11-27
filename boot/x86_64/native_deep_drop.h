#ifndef STYLEOS_X64_NATIVE_DEEP_DROP_H
#define STYLEOS_X64_NATIVE_DEEP_DROP_H
#include <styleos/deep_drop_ipc.h>
#include <styleos/runtime_bank_862.h>
void native_drop_reconcile(void);
enum dd_status native_drop_call(const struct rt862_selection *,unsigned op,uint64_t id,
                                uint64_t address,uint64_t size,uint64_t *result);
#endif
