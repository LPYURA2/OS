#include <styleos/ist_guard_plan_785.h>
#define LOWER_CANONICAL_LIMIT (UINT64_C(1)<<47)

static int inside(struct boot_range outer, uint64_t page)
{
    return outer.start < outer.end && !(outer.start & (IGP785_PAGE-1u)) &&
        !(outer.end & (IGP785_PAGE-1u)) &&
        page >= outer.start && page < outer.end &&
        outer.end - page >= IGP785_PAGE;
}
static int intersects(struct boot_range region, uint64_t page)
{
    return region.start < region.end && page < region.end &&
        region.start < page + IGP785_PAGE;
}

enum igp785_error igp785_prepare_layout(const struct igp785_request *r,
    struct igp785_layout *out)
{
    if (out) *out=(struct igp785_layout){0};
    if (!r || !out || !r->old_cr3_pa ||
        (r->old_cr3_pa & (IGP785_PAGE-1u)) ||
        r->nr_candidates>IGP785_MAX_EXCLUSIONS ||
        !r->nr_protected || r->nr_protected>IGP785_MAX_EXCLUSIONS ||
        r->reserved_image.start>=r->reserved_image.end ||
        r->reserved_image.end>LOWER_CANONICAL_LIMIT ||
        r->virtual_ist!=IGP785_IST_ALIAS)
        return IGP785_INVALID;
    /* Fixed high alias keeps the stack and its two unmapped guard VAs in a
     * single newly owned 4-KiB PT. Only a hardware old-root walk can prove
     * its PML4 slot is free; this PURE function does NOT prove that. */
    const uint64_t guard_lo=r->virtual_ist-IGP785_PAGE;
    const uint64_t guard_hi=r->virtual_ist+IGP785_PAGE;
    if (guard_lo>=LOWER_CANONICAL_LIMIT || guard_hi>=LOWER_CANONICAL_LIMIT ||
        (guard_lo>>39u)!=(r->virtual_ist>>39u) ||
        (guard_hi>>39u)!=(r->virtual_ist>>39u)) return IGP785_ALIAS;
    for(size_t i=0;i<IGP785_FRAMES;++i) {
        uint64_t page=r->pages[i];
        if (!page || (page&(IGP785_PAGE-1u)) ||
            page==r->old_cr3_pa) return IGP785_INVALID;
        if (!inside(r->reserved_image,page)) return IGP785_OUTSIDE_IMAGE;
        for(size_t j=0;j<i;++j)
            if(page==r->pages[j]) return IGP785_COLLISION;
        for(size_t j=0;j<r->nr_candidates;++j)
            if(intersects(r->candidate_ram[j],page)) return IGP785_EXCLUDED;
        for(size_t j=0;j<r->nr_protected;++j)
            if(intersects(r->protected_ranges[j],page)) return IGP785_EXCLUDED;
    }
    *out=(struct igp785_layout){
        guard_lo,r->virtual_ist,guard_hi,
        r->virtual_ist+IGP785_PAGE-16u,r->pages[IGP785_NEW_PML4],
        true,false,false,false
    };
    return IGP785_OK;
}

/* A software-only frame recipe. No dynamic allocation, old-root modification,
 * CR3 switch, or descriptor/IDT activation. Root entry collision is fatal. */
#define PRESENT_RW UINT64_C(3)
#define X86_ALIAS_PML4 128u
#define X86_ALIAS_PDPT 0u
#define X86_ALIAS_PD 0u
#define X86_ALIAS_PT 3u

enum igp785_error igp785_prepare_tables(const struct igp785_request *req,
    const uint64_t old_pml4[IGP785_ENTRIES],
    struct igp785_tables *tables, struct igp785_layout *layout)
{
    /* Reject overlap so zeroing an output cannot mutate the caller's root.
     * These are host-side pointer intervals; not a physical address proof. */
    if (!tables || !layout || !req || !old_pml4) return IGP785_INVALID;
    uintptr_t t=(uintptr_t)tables, o=(uintptr_t)old_pml4;
    if (t > UINTPTR_MAX-sizeof(*tables) ||
        o > UINTPTR_MAX-IGP785_ENTRIES*sizeof(uint64_t) ||
        (t < o+IGP785_ENTRIES*sizeof(uint64_t) && o < t+sizeof(*tables)))
        return IGP785_INVALID;
    *tables=(struct igp785_tables){0};
    *layout=(struct igp785_layout){0};
    enum igp785_error err=igp785_prepare_layout(req,layout);
    if (err!=IGP785_OK) return err;
    if (old_pml4[X86_ALIAS_PML4]!=0) {
        *layout=(struct igp785_layout){0};
        return IGP785_OCCUPIED;
    }
    for (unsigned i=0;i<IGP785_ENTRIES;++i)
        tables->pml4[i]=old_pml4[i];
    tables->pml4[X86_ALIAS_PML4]=req->pages[IGP785_NEW_PDPT]|PRESENT_RW;
    tables->pdpt[X86_ALIAS_PDPT]=req->pages[IGP785_NEW_PD]|PRESENT_RW;
    tables->pd[X86_ALIAS_PD]=req->pages[IGP785_NEW_PT]|PRESENT_RW;
    tables->pt[X86_ALIAS_PT]=req->pages[IGP785_IST1]|IGP785_STACK_PTE;
    /* The adjacent PT leaves remain *exactly zero*, not merely P=0. */
    return IGP785_OK;
}

bool igp785_audit_tables(const struct igp785_request *req,
    const uint64_t old_pml4[IGP785_ENTRIES],
    const struct igp785_tables *tables, const struct igp785_layout *layout)
{
    struct igp785_layout expected={0};
    if (!req || !old_pml4 || !tables || !layout ||
        igp785_prepare_layout(req,&expected)!=IGP785_OK ||
        old_pml4[X86_ALIAS_PML4] ||
        layout->new_root_pa!=expected.new_root_pa ||
        layout->virtual_guard_below!=expected.virtual_guard_below ||
        layout->virtual_ist!=expected.virtual_ist ||
        layout->virtual_guard_above!=expected.virtual_guard_above ||
        layout->virtual_ist_top!=expected.virtual_ist_top ||
        !layout->structural_only || layout->hardware_guard_proven ||
        layout->hardware_ist_proven || layout->activation_allowed) return false;
    for (unsigned i=0;i<IGP785_ENTRIES;++i) {
        if (tables->pml4[i] != (i==X86_ALIAS_PML4 ?
              (req->pages[IGP785_NEW_PDPT]|PRESENT_RW) : old_pml4[i]) ||
            tables->pdpt[i] != (i==X86_ALIAS_PDPT ?
              (req->pages[IGP785_NEW_PD]|PRESENT_RW) : 0) ||
            tables->pd[i] != (i==X86_ALIAS_PD ?
              (req->pages[IGP785_NEW_PT]|PRESENT_RW) : 0) ||
            tables->pt[i] != (i==X86_ALIAS_PT ?
              (req->pages[IGP785_IST1]|IGP785_STACK_PTE) : 0))
            return false;
    }
    return true;
}
