#include <styleos/ist_guard_plan_785.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static struct igp785_request good(void)
{
    struct igp785_request r={0};
    r.reserved_image=(struct boot_range){0x100000u,0x130000u};
    r.old_cr3_pa=0x70000u;
    r.virtual_ist=IGP785_IST_ALIAS;
    r.nr_protected=1;
    r.protected_ranges[0]=(struct boot_range){0x120000u,0x122000u};
    for(size_t i=0;i<IGP785_FRAMES;++i) r.pages[i]=0x100000u+(uint64_t)i*IGP785_PAGE;
    return r;
}
static int refused(struct igp785_request r)
{
    struct igp785_layout l;
    memset(&l,0xa5,sizeof(l));
    if(igp785_prepare_layout(&r,&l)==IGP785_OK) return 0;
    struct igp785_layout zero={0};
    return memcmp(&l,&zero,sizeof(l))==0;
}
int main(void)
{
    struct igp785_request r=good();struct igp785_layout l={0};
    CHECK(igp785_prepare_layout(&r,&l)==IGP785_OK);
    CHECK(l.structural_only && !l.activation_allowed &&
          !l.hardware_guard_proven && !l.hardware_ist_proven);
    CHECK(l.virtual_ist_top==IGP785_IST_ALIAS+IGP785_PAGE-16u);
    CHECK(l.virtual_guard_below+IGP785_PAGE==l.virtual_ist);
    CHECK(l.virtual_guard_above==l.virtual_ist+IGP785_PAGE);
    CHECK(l.new_root_pa==r.pages[IGP785_NEW_PML4]);
    CHECK(igp785_prepare_layout(NULL,&l)==IGP785_INVALID);
    CHECK(!l.activation_allowed);
    CHECK(igp785_prepare_layout(&r,NULL)==IGP785_INVALID);
    r=good();r.old_cr3_pa=0;CHECK(refused(r));
    r=good();r.old_cr3_pa+=1;CHECK(refused(r));
    r=good();r.nr_protected=0;CHECK(refused(r));
    r=good();r.nr_candidates=IGP785_MAX_EXCLUSIONS+1u;CHECK(refused(r));
    r=good();r.nr_protected=IGP785_MAX_EXCLUSIONS+1u;CHECK(refused(r));
    r=good();r.virtual_ist=0;CHECK(refused(r));
    r=good();r.virtual_ist=IGP785_IST_ALIAS+4096u;CHECK(refused(r));
    r=good();r.reserved_image.end=r.reserved_image.start;CHECK(refused(r));
    r=good();r.reserved_image.start+=1;CHECK(refused(r));
    r=good();r.pages[1]=r.pages[0];CHECK(refused(r));
    r=good();r.pages[3]=r.old_cr3_pa;CHECK(refused(r));
    r=good();r.pages[2]+=1;CHECK(refused(r));
    r=good();r.pages[5]=0x130000u;CHECK(refused(r));
    r=good();r.pages[5]=0;CHECK(refused(r));
    r=good();r.pages[5]=r.protected_ranges[0].start;CHECK(refused(r));
    r=good();r.nr_candidates=1;r.candidate_ram[0]=(struct boot_range){0x101000u,0x103000u};CHECK(refused(r));
    r=good();r.reserved_image.end=(UINT64_C(1)<<47)+4096u;CHECK(refused(r));
    r=good();r.nr_candidates=1;r.candidate_ram[0]=(struct boot_range){0x105800u,0x106800u};CHECK(refused(r));
    /* New root is a strict clone except an empty PML4 slot. */
    static uint64_t old[IGP785_ENTRIES];
    static struct igp785_tables tables;
    r=good();old[0]=UINT64_C(0x900003);old[7]=UINT64_C(0x990003);
    struct igp785_layout layout;
    CHECK(igp785_prepare_tables(&r,old,&tables,&layout)==IGP785_OK);
    CHECK(igp785_audit_tables(&r,old,&tables,&layout));
    CHECK(old[128]==0 && old[0]==UINT64_C(0x900003));
    CHECK(tables.pml4[0]==old[0] && tables.pml4[7]==old[7]);
    CHECK(tables.pml4[128]==(r.pages[1]|3u));
    CHECK(tables.pdpt[0]==(r.pages[2]|3u));
    CHECK(tables.pd[0]==(r.pages[3]|3u));
    CHECK(tables.pt[3]==(r.pages[8]|IGP785_STACK_PTE));
    CHECK(tables.pt[2]==0 && tables.pt[4]==0);
    CHECK(!layout.activation_allowed && !layout.hardware_guard_proven &&
          !layout.hardware_ist_proven);
    tables.pt[2]=2;CHECK(!igp785_audit_tables(&r,old,&tables,&layout));
    tables.pt[2]=0;tables.pt[3]|=4; /* US flag must never occur. */
    CHECK(!igp785_audit_tables(&r,old,&tables,&layout));
    tables.pt[3]&=~UINT64_C(4);tables.pt[3]&=~(UINT64_C(1)<<63);
    CHECK(!igp785_audit_tables(&r,old,&tables,&layout));
    tables.pt[3]|=UINT64_C(1)<<63;
    CHECK(igp785_audit_tables(&r,old,&tables,&layout));
    layout.activation_allowed=true;
    CHECK(!igp785_audit_tables(&r,old,&tables,&layout));
    layout.activation_allowed=false;
    old[128]=0x123;CHECK(igp785_prepare_tables(&r,old,&tables,&layout)==IGP785_OCCUPIED);
    CHECK(!layout.structural_only && tables.pt[3]==0);
    old[128]=0;
    r=good();r.pages[8]=r.pages[7];
    CHECK(igp785_prepare_tables(&r,old,&tables,&layout)==IGP785_COLLISION);
    CHECK(tables.pt[3]==0 && !layout.structural_only);
    printf("STYLEOS 785 GUARD TABLE PLAN HOST PASS guards=absent alias=NX supervisor-only activation=0 hardware=not-run\n");
    return 0;
}
