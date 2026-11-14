/* Pure checks and synthetic inherited tables only; never executes privileged
 * instructions and is safe on ARM64/Termux hosts as well as x86 hosts. */
#define STYLEOS_PAGING_X86_HOST 1
#include "../arch/x86_64/paging.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

_Alignas(PG_PAGE) uint64_t styleos_paging_tables[PG_TABLE_PAGES][512];
_Alignas(PG_PAGE) union pg_control styleos_paging_control;
_Alignas(PG_PAGE) uint64_t styleos_paging_data[PG_DATA_BYTES / sizeof(uint64_t)];
void arch_puts(const char *s) { (void)s; }
void arch_putc(char c) { (void)c; }
void paging_hex(uint64_t v) { (void)v; }
enum pg_error pg_lookup(const struct pg_plan *p, uint64_t a, struct pg_lookup *m)
{ (void)p; (void)a; (void)m; return PG_CONTEXT; }

_Alignas(PG_PAGE) static uint64_t old_pages[16][512];
static size_t next_page;

static struct x86_state fixture(void)
{
    memset(old_pages, 0, sizeof old_pages);
    next_page = 1;
    return (struct x86_state){ .cr0=BIT(0)|BIT(31), .cr3=(uintptr_t)old_pages,
        .cr4=BIT(5), .efer=BIT(8)|BIT(10)|BIT(11), .pat=UINT64_C(0x0007040600070406),
        .bits=48, .nx=1, .one_gib=1,
        .pa_mask=(BIT(48)-1u)&ENTRY_ADDRESS };
}

static uint64_t *leaf(uint64_t va, unsigned stop)
{
    uint64_t *table=old_pages[0];
    for (unsigned level=0; level<stop; ++level) {
        unsigned index=(unsigned)((va>>(39u-level*9u))&511u);
        if (!(table[index]&1u)) {
            assert(next_page<16);
            table[index]=(uintptr_t)old_pages[next_page++]|3u;
        }
        table=(uint64_t *)(uintptr_t)(table[index]&ENTRY_ADDRESS);
    }
    return &table[(va>>(39u-stop*9u))&511u];
}

static void modes(void)
{
    struct x86_state good=fixture();
    assert(mode_ok(&good)); /* Activation adds CR0.WP if absent. */
    const unsigned rejected_cr4[]={12,17,21,22,23,24};
    for (size_t i=0;i<sizeof rejected_cr4/sizeof *rejected_cr4;++i) {
        struct x86_state s=good;s.cr4|=BIT(rejected_cr4[i]);assert(!mode_ok(&s));
    }
    struct x86_state s=good;s.rflags=BIT(9);assert(!mode_ok(&s));
    s=good;s.bits=31;assert(!mode_ok(&s));
    s=good;s.bits=53;assert(!mode_ok(&s));
    s=good;s.nx=0;assert(!mode_ok(&s));
    s=good;s.efer&=~BIT(11);assert(!mode_ok(&s));
    s=good;s.efer&=~BIT(10);assert(!mode_ok(&s));
    s=good;s.cr4=0;assert(!mode_ok(&s));
    s=good;s.cr0|=BIT(30);assert(!mode_ok(&s));
    s=good;s.pat&=~UINT64_C(255);assert(!mode_ok(&s));
    s=good;s.cr0&=~BIT(31);assert(!mode_ok(&s));
}

static void descriptor_checks(void)
{
    const uint64_t good=UINT64_C(0x00af9b000000ffff);
    assert(code_descriptor_ok(good));
    assert(code_descriptor_ok(good&~BIT(40))); /* Needs authorized hardware A write. */
    assert(!code_descriptor_ok(good&~BIT(53)));
    assert(!code_descriptor_ok(good|BIT(54)));
    assert(!code_descriptor_ok(good|BIT(45)));
    assert(!code_descriptor_ok(good&~BIT(47)));
    assert(!code_descriptor_ok(good&~BIT(44)));
    struct x86_state s={.cs=8,.gdtr={15,0x20000},.idtr={4095,0x10000}};
    struct boot_range image={0x10000,0x20000};
    assert(descriptor_bounds_ok(&s,image));
    s.cs=0;assert(!descriptor_bounds_ok(&s,image));
    s.cs=9;assert(!descriptor_bounds_ok(&s,image));
    s.cs=16;assert(!descriptor_bounds_ok(&s,image));
    s.cs=8;s.gdtr.limit=14;assert(!descriptor_bounds_ok(&s,image));
    s.gdtr.limit=15;s.gdtr.base=LOW_VA_LIMIT-8;assert(!descriptor_bounds_ok(&s,image));
    s.gdtr.base=0x20000;s.idtr.limit=4094;assert(!descriptor_bounds_ok(&s,image));
    s.idtr.limit=4095;s.idtr.base=image.end-4095;assert(!descriptor_bounds_ok(&s,image));
    s.idtr.base=image.start;s.gdtr.base=0x20001;assert(!descriptor_bounds_ok(&s,image));
    struct boot_memory_map map={0};
    struct boot_memory_evidence e={.map=&map};
    assert(!hardware_gdt_write_allowed(&e,0x80000));
    map.nr_reserved=1;map.reserved[0]=(struct boot_range){0x80000,0x81000};
    assert(hardware_gdt_write_allowed(&e,0x80000));
    map.reserved[0].end--;assert(!hardware_gdt_write_allowed(&e,0x80000));
    map.reserved[0].end++;
    map.nr_candidates=1;map.candidates[0]=(struct boot_range){0x80fff,0x82000};
    assert(!hardware_gdt_write_allowed(&e,0x80000));
    map.candidates[0]=(struct boot_range){0x81000,0x82000};
    assert(hardware_gdt_write_allowed(&e,0x80000));
    map.nr_reserved=BOOT_MEMORY_INPUT_MAX+1u;
    assert(!hardware_gdt_write_allowed(&e,0x80000));
}

static void old_walk_checks(void)
{
    struct x86_state s=fixture();struct old_mapping m;
    const uint64_t va=UINT64_C(0x12345000),pa=UINT64_C(0x76543000);
    *leaf(va,3)=pa|3u;
    assert(old_walk(&s,va+4079,&m)==WALK_OK&&m.pa==pa+4079&&m.writable&&m.executable&&m.type==6);
    *leaf(va,3)=pa|1u|BIT(63);
    assert(old_walk(&s,va,&m)==WALK_OK&&!m.writable&&!m.executable);
    assert(old_walk(&s,PG_ALIAS,&m)==WALK_MISSING);
    assert(old_walk(&s,LOW_VA_LIMIT,&m)==WALK_BAD);
    *leaf(va,3)=pa|3u|BIT(51);assert(old_walk(&s,va,&m)==WALK_BAD);
    *leaf(va,3)=pa|3u|BIT(60);assert(old_walk(&s,va,&m)==WALK_BAD);
    s=fixture();*leaf(va,2)=UINT64_C(0x20000000)|0x83u;
    assert(old_walk(&s,va+17,&m)==WALK_OK&&m.pa==UINT64_C(0x20000000)+(va&0x1fffffu)+17);
    *leaf(va,2)|=BIT(13);assert(old_walk(&s,va,&m)==WALK_BAD);
    s=fixture();*leaf(va,1)=UINT64_C(0x80000000)|0x83u;
    assert(old_walk(&s,va,&m)==WALK_OK&&m.pa==UINT64_C(0x80000000)+va);
    s.one_gib=0;assert(old_walk(&s,va,&m)==WALK_BAD);
    s=fixture();*leaf(va,0)=0x83;assert(old_walk(&s,va,&m)==WALK_BAD);
    s=fixture();
    for (size_t i=0;i<16;++i) {
        uint64_t address=(uintptr_t)old_pages[i];*leaf(address,3)=address|3u;
    }
    assert(old_walk(&s,(uintptr_t)old_pages,&m)==WALK_OK);
    assert(identity_tables(&s));
    struct boot_range image={(uintptr_t)old_pages,(uintptr_t)old_pages+sizeof old_pages};
    assert(image_identity(&s,image));
    assert(writable_identity(&s,image.start,sizeof old_pages,image));
    *leaf(image.end-PG_PAGE,3)&=~UINT64_C(2);
    assert(!writable_identity(&s,image.start,sizeof old_pages,image));
    assert(image_identity(&s,image)); /* Identity alone is not a write proof. */
    *leaf(image.end-PG_PAGE,3)|=2u;
    *leaf((uintptr_t)old_pages,3)=((uintptr_t)old_pages+PG_PAGE)|3u;
    assert(!identity_tables(&s));
    s=fixture();s.nr_visited=OLD_TABLE_LIMIT;
    for(size_t i=0;i<OLD_TABLE_LIMIT;++i)s.visited[i]=(i+1u)*PG_PAGE;
    assert(!remember_table(&s,(uintptr_t)old_pages));
}

int main(void)
{
    modes();descriptor_checks();old_walk_checks();
    puts("x86 paging mode/descriptor/old-walk checks PASS");
    return 0;
}
