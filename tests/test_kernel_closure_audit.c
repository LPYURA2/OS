#include <styleos/kernel_closure_audit.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static _Alignas(DVM_PAGE_SIZE) struct dvm_storage arena;
static _Alignas(DVM_PAGE_SIZE) unsigned char core[KCA_ROLE_COUNT][DVM_PAGE_SIZE];
static _Alignas(DVM_PAGE_SIZE) unsigned char users[DVM_USER_SLOTS][DVM_USER_PAGES][DVM_PAGE_SIZE];
static struct dfo_owner owner;
static struct kca_manifest inventory;
static unsigned calls,deny_at;
static bool attest(void *ctx,uint64_t pa,uint64_t bytes)
{
    (void)ctx;
    assert(pa && !(pa&(DVM_PAGE_SIZE-1u)) && bytes==DVM_PAGE_SIZE);
    ++calls; return calls!=deny_at;
}
static void setup(unsigned count,int bad_rights)
{
    memset(&arena,0,sizeof arena);memset(&owner,0,sizeof owner);
    memset(&inventory,0,sizeof inventory);calls=0;deny_at=0;
    struct dfo_claims claims={0};
    claims.table_pa=(uint64_t)(uintptr_t)&arena;
    claims.kernel_count=count;
    for(unsigned i=0;i<KCA_ROLE_COUNT;++i){
        const uint64_t va=UINT64_C(0x200000)+(uint64_t)i*0x10000u;
        const uint64_t pa=(uint64_t)(uintptr_t)core[i];
        inventory.role[i]=(struct kca_page){va,pa};
        if(i<count){
            unsigned rights=(i==KCA_TEXT || i==KCA_TRAP_TEXT) ? DVM_EXEC :
                            (i==KCA_IDT ? 0u : DVM_WRITE);
            if(bad_rights && i==KCA_STACK) rights=DVM_EXEC;
            claims.kernel[i]=(struct dvm_region){va,pa,1,rights};
        }
    }
    for(unsigned i=0;i<DVM_USER_SLOTS;++i)
        for(unsigned j=0;j<DVM_USER_PAGES;++j)
            claims.user_pa[i][j]=(uint64_t)(uintptr_t)users[i][j];
    assert(dfo_prepare(&owner,&arena,&claims)==DFO_OK);
}
int main(void)
{
    setup(KCA_ROLE_COUNT,0);
    assert(kca_audit_core(&owner,&inventory,attest,0)==KCA_OK);
    assert(calls==KCA_ROLE_COUNT);
    for(unsigned root=0;root<DVM_ROOTS;++root){
        struct dvm_observed got;
        assert(dvm_lookup(&arena,&owner.roots,root,inventory.role[KCA_TEXT].va,&got));
        assert(!got.user && !got.writable && got.executable);
        assert(dvm_lookup(&arena,&owner.roots,root,inventory.role[KCA_STACK].va,&got));
        assert(!got.user && got.writable && !got.executable);
    }
    setup(KCA_ROLE_COUNT,0);deny_at=KCA_ROLE_COUNT;
    assert(kca_audit_core(&owner,&inventory,attest,0)==KCA_UNPROVEN);
    assert(calls==KCA_ROLE_COUNT);
    setup(KCA_ROLE_COUNT-1,0);
    assert(kca_audit_core(&owner,&inventory,attest,0)==KCA_MISSING);
    assert(calls==0);
    setup(KCA_ROLE_COUNT,1);
    assert(kca_audit_core(&owner,&inventory,attest,0)==KCA_RIGHTS);
    assert(calls==0);
    setup(KCA_ROLE_COUNT,0);inventory.role[KCA_TEXT].pa+=UINT64_C(0x1000000);
    assert(kca_audit_core(&owner,&inventory,attest,0)==KCA_TRANSLATION);
    assert(calls==0);
    setup(KCA_ROLE_COUNT,0);inventory.role[KCA_TRAP_TEXT].pa=inventory.role[KCA_TEXT].pa;
    assert(kca_audit_core(&owner,&inventory,attest,0)==KCA_ALIAS);
    assert(calls==0);
    setup(KCA_ROLE_COUNT,0);inventory.role[KCA_TEXT].pa=owner.plan.user[0][0].pa;
    assert(kca_audit_core(&owner,&inventory,attest,0)==KCA_ALIAS);
    setup(KCA_ROLE_COUNT,0);inventory.role[KCA_TEXT].va=DVM_USER_BEGIN;
    assert(kca_audit_core(&owner,&inventory,attest,0)==KCA_LAYOUT);
    setup(KCA_ROLE_COUNT,0);owner.ready=false;
    assert(kca_audit_core(&owner,&inventory,attest,0)==KCA_NOT_READY);
    assert(kca_audit_core(0,&inventory,attest,0)==KCA_ARGUMENT);
    puts("STYLEOS 778 KERNEL CORE CLOSURE AUDIT HOST PASS: 7 mandatory roles, three roots, rights, alias, absent and attestation refusals");
    return 0;
}
