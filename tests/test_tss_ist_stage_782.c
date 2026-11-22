#include <styleos/tss_ist_stage_782.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct boot_memory_map map;
static struct boot_memory_evidence e;
static struct fg_claims prior;
static struct tis782_claims claim;
static unsigned calls;
static bool walk(void *arg,uint64_t a,uint64_t n)
{ ++calls;return n==TIS782_PAGE && a && (!arg || a!=*(uint64_t*)arg); }
/* Host fixture's only goal is to check policy; the real inherited CR3 walk
 * comes from arch_paging_attest_image_writable in the QEMU guest. */
static void reset(void)
{
    memset(&map,0,sizeof map); memset(&e,0,sizeof e);
    memset(&prior,0,sizeof prior);calls=0;
    e.map=&map; e.live.image=(struct boot_range){0x100000,0x300000};
    e.live.stack=(struct boot_range){0x1a0000,0x1b0000};
    e.live.source=(struct boot_range){0x1e0000,0x1f0000};
    e.live.handoff=(struct boot_range){0x1c0000,0x1c1000};
    e.workspace=(struct boot_range){0x1d0000,0x1d1000};
    map.reserved[0]=e.live.image;map.nr_reserved=1;
    map.candidates[0]=(struct boot_range){0x400000,0x500000};map.nr_candidates=1;
    prior.tables=0x110000;
    for(unsigned a=0;a<DVM_USER_SLOTS;++a)
        for(unsigned b=0;b<DVM_USER_PAGES;++b)
            prior.user[a][b]=0x160000+(a*DVM_USER_PAGES+b)*4096;
    claim=(struct tis782_claims){0x170000,0x171000,0x172000,0x173000};
}
int main(void)
{
    struct tis782_plan p={0};uint8_t tss[104];
    reset();assert(tis782_prepare(&e,&prior,&claim,walk,NULL,&p)==TIS782_OK);
    assert(calls==4 && p.staged && !p.ltr_permitted &&
           p.rsp0_top==0x172ff0 && p.ist1_top==0x173ff0);
    memset(tss,0xaa,sizeof tss);
    assert(tis782_encode(&p,tss,sizeof tss));
    assert(tss[4]==0xf0 && tss[5]==0x2f && tss[6]==0x17);
    assert(tss[36]==0xf0 && tss[37]==0x3f && tss[38]==0x17);
    assert(tss[102]==104 && tss[103]==0 && tss[0]==0);
    reset();claim.ist1=claim.rsp0;
    assert(tis782_prepare(&e,&prior,&claim,walk,NULL,&p)==TIS782_CONFLICT && !calls && !p.staged);
    reset();claim.tss++;
    assert(tis782_prepare(&e,&prior,&claim,walk,NULL,&p)==TIS782_LAYOUT && !calls);
    reset();claim.gdt=prior.tables;
    assert(tis782_prepare(&e,&prior,&claim,walk,NULL,&p)==TIS782_CONFLICT && !calls);
    reset();claim.rsp0=prior.user[0][0];
    assert(tis782_prepare(&e,&prior,&claim,walk,NULL,&p)==TIS782_CONFLICT && !calls);
    reset();claim.ist1=e.live.stack.start;
    assert(tis782_prepare(&e,&prior,&claim,walk,NULL,&p)==TIS782_CONFLICT && !calls);
    reset();map.nr_reserved=0;
    assert(tis782_prepare(&e,&prior,&claim,walk,NULL,&p)==TIS782_ARGUMENT && !calls);
    reset();uint64_t refuse=claim.ist1;
    assert(tis782_prepare(&e,&prior,&claim,walk,&refuse,&p)==TIS782_UNATTESTED && calls==4 && !p.staged);
    memset(tss,0xaa,sizeof tss);
    assert(!tis782_encode(&p,tss,sizeof tss));
    for(unsigned i=0;i<104u;++i)assert(tss[i]==0xaa);
    reset();assert(tis782_prepare(&e,&prior,&claim,NULL,NULL,&p)==TIS782_ARGUMENT);
    reset();assert(tis782_prepare(&e,&prior,&claim,walk,NULL,&p)==TIS782_OK);
    assert(!tis782_encode(&p,tss,103u));
    p.ltr_permitted=true;assert(!tis782_encode(&p,tss,sizeof tss));
    puts("STYLEOS 782 TSS IST OWNER POLICY HOST PASS");
    return 0;
}
