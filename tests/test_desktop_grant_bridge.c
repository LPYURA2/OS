#include <styleos/desktop_grant_bridge.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static _Alignas(DVM_PAGE_SIZE) struct dvm_storage storage;
static _Alignas(DVM_PAGE_SIZE) unsigned char users[DVM_USER_SLOTS][DVM_USER_PAGES][DVM_PAGE_SIZE];
static _Alignas(DVM_PAGE_SIZE) unsigned char kernel_page[DVM_PAGE_SIZE];
static struct dfo_owner owner;
static struct fg_claims physical;
static struct boot_memory_map map;
static struct boot_memory_evidence evidence;
static struct dgb_result output;
static unsigned proofs, deny_at;
static uint64_t min(uint64_t a,uint64_t b){return a<b?a:b;}
static uint64_t max(uint64_t a,uint64_t b){return a>b?a:b;}
static void setup(void)
{
    memset(&storage,0,sizeof storage);memset(users,0,sizeof users);
    memset(kernel_page,0,sizeof kernel_page);memset(&owner,0,sizeof owner);
    memset(&physical,0,sizeof physical);memset(&map,0,sizeof map);
    memset(&evidence,0,sizeof evidence);memset(&output,0,sizeof output);
    proofs=deny_at=0;
    physical.tables=(uint64_t)(uintptr_t)&storage;
    uint64_t lo=min(physical.tables,(uint64_t)(uintptr_t)kernel_page);
    uint64_t hi=max(physical.tables+sizeof storage,
                    (uint64_t)(uintptr_t)kernel_page+DVM_PAGE_SIZE);
    for(unsigned s=0;s<DVM_USER_SLOTS;++s)
        for(unsigned p=0;p<DVM_USER_PAGES;++p){
            physical.user[s][p]=(uint64_t)(uintptr_t)&users[s][p][0];
            lo=min(lo,physical.user[s][p]);
            hi=max(hi,physical.user[s][p]+DVM_PAGE_SIZE);
        }
    evidence.map=&map;evidence.live.image=(struct boot_range){lo,hi};
    map.reserved[0]=evidence.live.image;map.nr_reserved=1;
    map.candidates[0]=(struct boot_range){0x400000,0x500000};map.nr_candidates=1;
}
static bool walk(void *context,uint64_t pa,uint64_t bytes)
{
    (void)context;assert(pa && !(pa & (DVM_PAGE_SIZE-1u)) && bytes);
    ++proofs;return proofs!=deny_at;
}
static enum dgb_status run(void)
{ return dgb_prepare(&owner,&storage,&evidence,&physical,
    (uint64_t)(uintptr_t)kernel_page,walk,0,&output); }
int main(void)
{
    setup();assert(run()==DGB_OK);
    assert(proofs==8 && output.proved_pages==79 && owner.ready);
    assert(output.safe_root==physical.tables &&
           output.user_root[0]!=output.user_root[1]);
    struct dvm_observed obs;
    for(unsigned root=0;root<DVM_ROOTS;++root){
        assert(dvm_lookup(&storage,&owner.roots,root,DGB_SUPERVISOR_VA,&obs));
        assert(!obs.user && obs.writable && !obs.executable);
        for(unsigned p=0;p<DVM_USER_PAGES;++p){
            uint64_t va=DVM_USER_BEGIN+(p==0?0:p==1?0x10000u:0x20000u);
            if(!root)assert(!dvm_lookup(&storage,&owner.roots,root,va,&obs));
            else{
                assert(dvm_lookup(&storage,&owner.roots,root,va,&obs));
                assert(obs.user && obs.pa==physical.user[root-1u][p]);
                assert(obs.executable==(p==0) && obs.writable==(p!=0));
            }
        }
    }
    assert(run()==DGB_READY);
    setup();deny_at=1;assert(run()==DGB_ATTEST && proofs==1 && !owner.ready);
    setup();deny_at=8;assert(run()==DGB_ATTEST && proofs==8 && !owner.ready);
    setup();physical.user[1][0]=physical.user[0][0];
    assert(run()==DGB_POLICY && !proofs);
    setup();physical.user[0][0]++;
    assert(run()==DGB_POLICY && !proofs);
    setup();map.nr_reserved=0;assert(run()==DGB_POLICY && !proofs);
    setup();map.candidates[0]=(struct boot_range){
       (uint64_t)(uintptr_t)kernel_page,
       (uint64_t)(uintptr_t)kernel_page+DVM_PAGE_SIZE};
    assert(run()==DGB_KERNEL && !proofs);
    setup();physical.tables+=DVM_PAGE_SIZE;
    assert(run()==DGB_STORAGE && !proofs);
    setup();assert(dgb_prepare(&owner,&storage,&evidence,&physical,
       physical.tables,walk,0,&output)==DGB_KERNEL && !proofs);
    puts("STYLEOS 777 IMAGE-FRAME BRIDGE HOST PASS: 79 pages, detached SAFE and users, denials");
    return 0;
}
