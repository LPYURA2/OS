#include <styleos/kernel_closure_audit.h>

#define KCA_PAGE UINT64_C(4096)
#define KCA_PHYS_LIMIT (UINT64_C(1) << 52)

static bool valid(uint64_t va,uint64_t pa)
{
    return va && pa && !((va|pa)&(KCA_PAGE-1u)) &&
           va<DVM_USER_BEGIN && va<=DVM_USER_BEGIN-KCA_PAGE &&
           pa<=KCA_PHYS_LIMIT-KCA_PAGE;
}
static bool wants_write(unsigned role)
{ return role==KCA_DATA || role==KCA_STACK || role==KCA_GDT || role==KCA_TSS; }
static bool wants_execute(unsigned role)
{ return role==KCA_TEXT || role==KCA_TRAP_TEXT; }

enum kca_status kca_audit_core(const struct dfo_owner *owner,
    const struct kca_manifest *m,kca_attest attest,void *context)
{
    if(!owner || !m || !attest)return KCA_ARGUMENT;
    if(!owner->ready || !owner->storage || !owner->roots.root[0])return KCA_NOT_READY;
    /* Check all claims before any hardware callback or root lookup. */
    for(unsigned i=0;i<KCA_ROLE_COUNT;++i){
        const struct kca_page a=m->role[i];
        if(!valid(a.va,a.pa))return KCA_LAYOUT;
        for(unsigned j=0;j<i;++j){
            const struct kca_page b=m->role[j];
            if(a.pa==b.pa || a.va==b.va)return KCA_ALIAS;
        }
        for(unsigned s=0;s<DVM_USER_SLOTS;++s)
            for(unsigned p=0;p<DVM_USER_PAGES;++p)
                if(a.pa==owner->plan.user[s][p].pa)return KCA_ALIAS;
        if(a.pa>=owner->plan.physical_table_base &&
           a.pa-owner->plan.physical_table_base<sizeof(struct dvm_storage))
            return KCA_ALIAS;
    }
    /* All roles must resolve to the same PA and rights in SAFE and both user
     * roots. An absent or user-accessible kernel mapping is never tolerated. */
    for(unsigned i=0;i<KCA_ROLE_COUNT;++i){
        const struct kca_page a=m->role[i];
        for(unsigned root=0;root<DVM_ROOTS;++root){
            struct dvm_observed observed;
            if(!dvm_lookup(owner->storage,&owner->roots,root,a.va,&observed))
                return KCA_MISSING;
            if(observed.pa!=a.pa)return KCA_TRANSLATION;
            if(observed.user || observed.writable!=wants_write(i) ||
               observed.executable!=wants_execute(i))return KCA_RIGHTS;
        }
    }
    for(unsigned i=0;i<KCA_ROLE_COUNT;++i)
        if(!attest(context,m->role[i].pa,KCA_PAGE))return KCA_UNPROVEN;
    /* CORE inventory only. No device cache policy, full image range, IRQ
     * stack/IST, UART/GOP or firmware closure has been proved by this call. */
    return KCA_OK;
}
