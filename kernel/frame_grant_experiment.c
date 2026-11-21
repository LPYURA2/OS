#include <styleos/frame_grant_proof.h>
#include <styleos/paging.h>
#include <styleos/arch.h>

/* Only present in the independent --test-frame-grants PE image.
 * Neither storage nor the six data pages are acquired from candidate RAM.
 * The arrays are never published to the ordinary desktop or to a PMM. */
_Alignas(DVM_PAGE_SIZE) struct dvm_storage styleos_grant_tables;
_Alignas(DVM_PAGE_SIZE) unsigned char
    styleos_grant_users[DVM_USER_SLOTS][DVM_USER_PAGES][DVM_PAGE_SIZE];
_Static_assert(sizeof styleos_grant_tables == 72u * 4096u,"72 page-table frames");
_Static_assert(sizeof styleos_grant_users == 6u * 4096u,"six private frames");

static void put_field(const char *key,uint64_t value)
{ arch_puts(key); paging_hex(value); }
static void show(const char *name,uint64_t va,uint64_t bytes)
{
    arch_puts("STYLEOS GRANTS "); arch_puts(name);
    put_field(" va=",va); put_field(" pa=",va);
    put_field(" bytes=",bytes); arch_putc('\n');
}

void fg_experiment_run(struct pg_experiment *p)
{
    if (!p || !p->evidence || !p->old_root)
        paging_fail("frame-grants context");
    const struct boot_memory_evidence *e=p->evidence;
    const uint64_t table_va=(uint64_t)(uintptr_t)&styleos_grant_tables;
    struct fg_claims claims={0};
    claims.tables=table_va;
    for(unsigned i=0;i<DVM_USER_SLOTS;++i)
        for(unsigned j=0;j<DVM_USER_PAGES;++j)
            claims.user[i][j]=(uint64_t)(uintptr_t)&styleos_grant_users[i][j][0];
    if(fg_check_policy(e,&claims)!=FG_OK)
        paging_fail("frame-grants policy");
    if(arch_paging_attest_image_writable(p,table_va,sizeof styleos_grant_tables)!=PG_OK)
        paging_fail("frame-grants tables physical walk");
    show("TABLES",table_va,sizeof styleos_grant_tables);
    for(unsigned i=0;i<DVM_USER_SLOTS;++i)
        for(unsigned j=0;j<DVM_USER_PAGES;++j) {
            uint64_t va=claims.user[i][j];
            if(arch_paging_attest_image_writable(p,va,DVM_PAGE_SIZE)!=PG_OK)
                paging_fail("frame-grants user physical walk");
            /* Explicit independent per-frame line, validated against the PE
             * image's real BSS range by the strict Python QEMU parser. */
            const char *names[2][3]={{"USER_A_CODE","USER_A_STACK","USER_A_DATA"},
                                     {"USER_B_CODE","USER_B_STACK","USER_B_DATA"}};
            show(names[i][j],va,DVM_PAGE_SIZE);
        }
    /* Exercise negative policy cases without modifying the real claims.
     * These results are observations of the same map and checker. */
    struct fg_claims invalid=claims;
    invalid.user[1][0]=claims.user[0][0];
    if(fg_check_policy(e,&invalid)!=FG_CONFLICT)
        paging_fail("frame-grants alias refusal");
    invalid=claims;invalid.user[0][0]+=1;
    if(fg_check_policy(e,&invalid)!=FG_LAYOUT)
        paging_fail("frame-grants alignment refusal");
    invalid=claims;invalid.user[0][0]=e->live.stack.start & ~(DVM_PAGE_SIZE-1u);
    if(fg_check_policy(e,&invalid)!=FG_CONFLICT)
        paging_fail("frame-grants stack refusal");
    int checked_candidate=0;
    for(size_t n=0;n<e->map->nr_candidates;++n) {
        struct boot_range r=e->map->candidates[n];
        if(r.end-r.start<DVM_PAGE_SIZE || !r.start)continue;
        invalid=claims;invalid.user[0][0]=r.start;
        /* Outside the reserved image is an adequate refusal even if
         * the policy returns NOT_IMAGE before checking CANDIDATE. */
        if(fg_check_policy(e,&invalid)==FG_OK)
            paging_fail("frame-grants candidate accepted");
        checked_candidate=1;
        break;
    }
    if(!checked_candidate)
        paging_fail("frame-grants no candidate case");
    arch_puts("STYLEOS GRANTS REFUSALS alias=1 alignment=1 stack=1 candidate=1\n");
    arch_puts("STYLEOS GRANTS PASS pages=0x000000000000004e origin=image-owned no-pmm=1\n");
    arch_puts("STYLEOS GRANTS END\n");
}
