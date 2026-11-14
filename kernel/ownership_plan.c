#include "styleos/arch.h"
#include "styleos/boot_pool.h"
#include "styleos/ownership.h"

_Alignas(OP_PAGE) union op_workspace styleos_ownership_workspace;
_Static_assert(sizeof styleos_ownership_workspace == OP_WORKSPACE_BYTES, "workspace size");
_Static_assert(sizeof(struct op_state) <= OP_WORKSPACE_BYTES, "workspace capacity");

static _Noreturn void fail(const char *why)
{
    arch_puts("STYLEOS OWNERSHIP ERROR: "); arch_puts(why); arch_putc('\n');
    arch_halt();
}
static void check(enum op_error e)
{
    if (e != OP_OK) fail(op_error_name(e));
}
static void hex(uint64_t value)
{
    arch_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        arch_putc("0123456789abcdef"[(value >> shift) & 15u]);
}
static void field(const char *key, uint64_t value)
{
    arch_puts(key); hex(value);
}
static int overlap(struct boot_range a, struct boot_range b)
{
    return a.start < b.end && b.start < a.end;
}
static struct boot_range admit(const struct boot_memory_evidence *e)
{
    struct boot_range pool = boot_pool_storage_range(), meta = boot_pool_metadata_range();
    if (boot_pool_check_layout(pool.start, pool.end-pool.start, meta.start,
                              meta.end-meta.start, e) != BP_OK) fail("context");
    struct boot_range own;
    if (bm_extent((uintptr_t)&styleos_ownership_workspace,
                  sizeof styleos_ownership_workspace, &own)) fail("overflow");
    if ((own.start | own.end) & (OP_PAGE-1u)) fail("alignment");
    if (own.start < e->live.image.start || own.end > e->live.image.end) fail("range");
    const struct boot_range excluded[] = {e->live.stack,e->live.handoff,e->live.source,
                                          e->workspace,pool,meta};
    for (size_t n=0;n<sizeof excluded/sizeof excluded[0];++n)
        if (overlap(own,excluded[n])) fail("context");
    return own;
}
static void claim(struct op_input *in, enum op_kind kind, enum op_proof proof,
                  struct boot_range r)
{
    check(op_add_claim(in,kind,proof,r.start,r.start,r.end-r.start));
}
static void snapshot(const char *label, const struct op_snapshot *s)
{
    arch_puts(label);
    field(" mode=",s->mode); field(" root=",s->root);
    field(" aux0=",s->aux0); field(" aux1=",s->aux1);
    field(" pat=",s->pat); field(" rflags=",s->rflags);
    field(" bits=",s->pa_bits); field(" features=",s->features);
    field(" gdt_va=",s->gdt_va); field(" gdt_bytes=",s->gdt_bytes);
    field(" idt_va=",s->idt_va); field(" idt_bytes=",s->idt_bytes);
    field(" cs=",s->cs); arch_putc('\n');
}
static void print_range(const char *kind, struct boot_range r, const char *reason)
{
    arch_puts("STYLEOS OWNERSHIP "); arch_puts(kind); arch_putc(' ');
    hex(r.start); arch_putc(' '); hex(r.end);
    arch_puts(reason); arch_putc('\n');
}

void ownership_plan_run(const struct boot_info *info, const struct boot_memory_evidence *e)
{
    const struct boot_range own = admit(e);
    /* First persistent write follows complete image/overlap admission. No
     * address listed in the plan is used as an allocation or probe target. */
    struct op_state *state = &styleos_ownership_workspace.state;
    *state = (struct op_state){0};
    struct op_input *in = &state->input;
    in->arch=info->arch; in->source=info->arch==STYLEOS_ARCH_X86_64 ? OP_UEFI : OP_FDT;
    in->map=e->map; in->live=e->live; in->map_workspace=e->workspace;
    in->own_workspace=own; in->pool=boot_pool_storage_range();
    in->pool_metadata=boot_pool_metadata_range();
    if (in->source==OP_UEFI) {
        if (info->memory_map!=in->live.source.start ||
            info->memory_map_size!=in->live.source.end-in->live.source.start) fail("source");
        in->source_stride=info->descriptor_size;
        in->source_version=info->descriptor_version;
    } else if (info->dtb!=in->live.source.start) fail("source");
    claim(in,OP_IMAGE,OP_IMAGE_IDENTITY,in->live.image);
    claim(in,OP_STACK,OP_IMAGE_IDENTITY,in->live.stack);
    claim(in,OP_HANDOFF,OP_IMAGE_IDENTITY,in->live.handoff);
    claim(in,OP_SOURCE_BUFFER,OP_BOOT_SOURCE,in->live.source);
    claim(in,OP_MAP_WORKSPACE,OP_IMAGE_IDENTITY,in->map_workspace);
    claim(in,OP_OWN_WORKSPACE,OP_IMAGE_IDENTITY,in->own_workspace);
    claim(in,OP_BOOT_POOL,OP_IMAGE_IDENTITY,in->pool);
    claim(in,OP_POOL_METADATA,OP_IMAGE_IDENTITY,in->pool_metadata);
    check(op_arch_collect(in,info));
#ifdef STYLEOS_TEST_OWNERSHIP_INVALID
    /* Controlled metadata-only refusal. The real handoff/map remain intact. */
    in->claims[0].proof=OP_UNPROVEN;
#endif
    check(op_build(in,&state->output,&state->scratch));
#ifdef STYLEOS_TEST_OWNERSHIP_INVALID
    fail("negative input accepted");
#endif
    if (!state->output.valid || state->output.granted ||
        !op_arch_unchanged(in,&state->after)) fail("context");

    /* Nothing from a partial plan is serialized. The after-build snapshot was
     * captured from hardware before publishing this complete read-only plan. */
    arch_puts("STYLEOS OWNERSHIP PLAN arch=");
    arch_puts(in->arch==STYLEOS_ARCH_X86_64 ? "x86_64 phase=inherited source=uefi\n" :
                                          "aarch64 phase=mmu-off source=fdt\n");
    arch_puts("STYLEOS OWNERSHIP WORKSPACE"); field(" pa=",own.start);
    field(" bytes=",own.end-own.start); arch_putc('\n');
    arch_puts("STYLEOS OWNERSHIP SOURCE"); field(" pa=",in->live.source.start);
    field(" bytes=",in->live.source.end-in->live.source.start);
    field(" stride=",in->source_stride); field(" version=",in->source_version); arch_putc('\n');
    snapshot("STYLEOS OWNERSHIP STATE",&in->snapshot);
    for (size_t n=0;n<in->nr_claims;++n) {
        const struct op_claim *c=&in->claims[n];
        arch_puts("STYLEOS OWNERSHIP CLAIM kind="); arch_puts(op_kind_name(c->kind));
        arch_puts(" proof="); arch_puts(op_proof_name(c->proof));
        field(" va=",c->va); field(" pa=",c->pa); field(" bytes=",c->bytes); arch_putc('\n');
    }
    for (size_t n=0;n<in->nr_walks;++n) {
        const struct op_walk *w=&in->walks[n];
        arch_puts("STYLEOS OWNERSHIP WALK kind=");
        arch_puts(w->kind==OP_WALK_GDT ? "GDT" : "TABLE");
        field(" va=",w->va); field(" pa=",w->pa); field(" levels=",w->levels); arch_putc('\n');
        for (size_t level=0;level<w->levels;++level) {
            arch_puts("STYLEOS OWNERSHIP STEP"); field(" level=",level);
            field(" table=",w->tables[level]); field(" entry=",w->entries[level]); arch_putc('\n');
        }
    }
    arch_puts("STYLEOS OWNERSHIP INACTIVE paging=not-built pool=not-admitted\n");
    arch_puts("STYLEOS OWNERSHIP UNRESOLVED"); field(" flags=",in->unresolved); arch_putc('\n');
    for (size_t n=0;n<state->output.nr_protected;++n)
        print_range("PROTECTED",state->output.protected_ranges[n],"");
    for (size_t n=0;n<state->output.nr_review;++n)
        print_range("REVIEW",state->output.review[n]," reason=no-transfer");
    arch_puts("STYLEOS OWNERSHIP UNCLASSIFIED outside-covered-union\n");
    arch_puts("STYLEOS OWNERSHIP TOTAL"); field(" protected=",state->output.protected_pages);
    field(" review=",state->output.review_pages); field(" granted=",state->output.granted); arch_putc('\n');
    snapshot("STYLEOS OWNERSHIP STATE AFTER",&state->after);
    arch_puts("STYLEOS OWNERSHIP PASS\n");
}
