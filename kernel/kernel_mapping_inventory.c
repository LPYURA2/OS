#include <styleos/kernel_mapping_inventory.h>
#define KMI_PAGE_BYTES UINT64_C(4096)
#define KMI_LOW_HALF (UINT64_C(1)<<47)
enum kmi_error kmi_account(struct kmi_inventory *a,enum kmi_kind kind,
                           const struct kmi_observation *m,bool identity)
{
    /* No failure path is permitted to retain an accidental activation flag. */
    if (a) a->root_activation_permitted=false;
    if (!a || !m || (unsigned)kind>=KMI_KIND_COUNT) return KMI_ARGUMENT;
    if (!m->va || (m->va&(KMI_PAGE_BYTES-1u)) || m->va>=KMI_LOW_HALF ||
        m->pa>=((UINT64_C(1)<<52)-KMI_PAGE_BYTES) ||
        (m->pa&(KMI_PAGE_BYTES-1u))) return KMI_PAGE;
    if (identity && m->va!=m->pa) return KMI_TRANSLATION;
    if (m->user) return KMI_PRIVILEGE;
    /* Only inherited WB RAM mappings are in scope. No PAT/PCD/MMIO
     * inference, and no attempt to read a device address. */
    if (m->cache_type!=6u) return KMI_CACHE;
    if (a->checked_pages>=KMI_MAX_ACCOUNTED ||
        a->pages[kind]>=KMI_MAX_ACCOUNTED) return KMI_LIMIT;
    ++a->checked_pages;
    ++a->pages[kind];
    if (m->writable && m->executable) ++a->rwx_pages;
    a->root_activation_permitted=false;
    return KMI_OK;
}
