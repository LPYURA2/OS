#include <styleos/root_minimization_789.h>
#include <stdint.h>
/* This function cannot install CR3 or transfer memory ownership. Failures
 * must not leave a partially pruned root, including on overlapping inputs. */
bool r789_audit_restricted_root(const uint64_t *staged,
    const uint64_t *inherited)
{
    if (!staged || !inherited || staged == inherited ||
        (inherited[0] & UINT64_C(1)) == 0 || inherited[R789_ALIAS_SLOT] ||
        (staged[0] != inherited[0]) ||
        (staged[R789_ALIAS_SLOT] & UINT64_C(0x3)) != UINT64_C(0x3) ||
        (staged[R789_ALIAS_SLOT] & UINT64_C(0x000ffffffffff000)) == 0)
        return false;
    for (size_t i=1;i<R789_PML4_ENTRIES;i++) {
        if (i!=R789_ALIAS_SLOT && staged[i]!=0) return false;
    }
    return true;
}
bool r789_prune_inherited_root(uint64_t *staged,
    const uint64_t *inherited,const uint64_t *required,size_t nr_required)
{
    if (!staged || !inherited || !required || !nr_required || nr_required>32u ||
        (uintptr_t)staged > UINTPTR_MAX-4096u ||
        (uintptr_t)inherited > UINTPTR_MAX-4096u)
        return false;
    uintptr_t a=(uintptr_t)staged,b=(uintptr_t)inherited;
    if (a < b+4096u && b < a+4096u) return false;
    if (inherited[R789_ALIAS_SLOT] || (inherited[0]&1u)==0u ||
        staged[0]!=inherited[0] ||
        (staged[R789_ALIAS_SLOT]&3u)!=3u ||
        !(staged[R789_ALIAS_SLOT]&UINT64_C(0x000ffffffffff000)))
        return false;
    /* Only a known inherited recipe may be narrowed, never an unknown root. */
    for (size_t i=1;i<R789_PML4_ENTRIES;i++) {
        if (i!=R789_ALIAS_SLOT && staged[i]!=inherited[i]) return false;
    }
    for(size_t i=0;i<nr_required;i++) {
        if (!required[i] || required[i]>=R789_LOW_SLOT_LIMIT) return false;
    }
    for (size_t i=1;i<R789_PML4_ENTRIES;i++) {
        if(i!=R789_ALIAS_SLOT) staged[i]=0;
    }
    return r789_audit_restricted_root(staged,inherited);
}
