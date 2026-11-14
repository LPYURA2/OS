/* Pure checks against synthetic inherited tables. The host profile contains no
 * privileged instructions and can also run natively on ARM64/Termux. */
#define STYLEOS_OWNERSHIP_X86_HOST 1
#include "../arch/x86_64/ownership.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* These tests inspect the architecture's bounded proofs, not common claim
 * construction; that API has its own independent host tests. */
enum op_error op_add_claim(struct op_input *in, enum op_kind kind,
                          enum op_proof proof, uint64_t va, uint64_t pa,
                          uint64_t bytes)
{
    if (!in || in->nr_claims == OP_CLAIMS_MAX) return OP_LIMIT;
    in->claims[in->nr_claims++] = (struct op_claim){kind, proof, va, pa, bytes};
    return OP_OK;
}

_Alignas(OP_PAGE) static uint64_t old_pages[16][512];
_Alignas(OP_PAGE) static unsigned char image_bytes[3u * OP_PAGE];
_Alignas(OP_PAGE) static unsigned char gdt_bytes[2u * OP_PAGE];
static struct boot_memory_map baseline;
static struct op_input input;
static size_t next_page;
static unsigned checks;

#define CHECK(condition) do { assert(condition); ++checks; } while (0)

static struct x86_state fixture(void)
{
    memset(old_pages, 0, sizeof old_pages);
    memset(&baseline, 0, sizeof baseline);
    memset(&input, 0, sizeof input);
    next_page = 1;
    baseline.nr_reserved = 1;
    baseline.reserved[0] = (struct boot_range){
        (uintptr_t)old_pages, (uintptr_t)old_pages + sizeof old_pages};
    input.map = &baseline;
    return (struct x86_state){
        .cr0 = BIT(0) | BIT(31), .cr3 = (uintptr_t)old_pages,
        .cr4 = BIT(5), .efer = BIT(8) | BIT(10) | BIT(11),
        .pat = UINT64_C(0x0007040600070406), .bits = 48,
        .nx = 1, .one_gib = 1,
        .pa_mask = (BIT(48) - 1u) & ENTRY_ADDRESS, .input = &input};
}

static uint64_t *leaf(uint64_t va, unsigned stop)
{
    uint64_t *table = old_pages[0];
    for (unsigned level = 0; level < stop; ++level) {
        unsigned index = (unsigned)((va >> (39u - level * 9u)) & 511u);
        if (!(table[index] & 1u)) {
            assert(next_page < sizeof old_pages / sizeof *old_pages);
            table[index] = (uintptr_t)old_pages[next_page++] | 3u;
        }
        table = (uint64_t *)(uintptr_t)(table[index] & ENTRY_ADDRESS);
    }
    return &table[(va >> (39u - stop * 9u)) & 511u];
}

static void mode_checks(void)
{
    struct x86_state good = fixture();
    CHECK(mode_ok(&good));
    const unsigned unsupported[] = {12, 17, 21, 22, 23, 24};
    for (size_t i = 0; i < sizeof unsupported / sizeof *unsupported; ++i) {
        struct x86_state s = good;
        s.cr4 |= BIT(unsupported[i]);
        CHECK(!mode_ok(&s));
    }
    struct x86_state s = good;
    s.rflags = BIT(9); CHECK(!mode_ok(&s));
    s = good; s.rflags = BIT(10); CHECK(!mode_ok(&s));
    s = good; s.bits = 31; CHECK(!mode_ok(&s));
    s = good; s.bits = 53; CHECK(!mode_ok(&s));
    s = good; s.nx = 0; CHECK(!mode_ok(&s));
    s = good; s.efer &= ~BIT(11); CHECK(!mode_ok(&s));
    s = good; s.efer &= ~BIT(10); CHECK(!mode_ok(&s));
    s = good; s.cr4 &= ~BIT(5); CHECK(!mode_ok(&s));
    s = good; s.cr0 |= BIT(29); CHECK(!mode_ok(&s));
    s = good; s.cr0 |= BIT(30); CHECK(!mode_ok(&s));
    s = good; s.cr0 &= ~BIT(31); CHECK(!mode_ok(&s));
    s = good; s.cr0 &= ~BIT(0); CHECK(!mode_ok(&s));
    s = good; s.pat &= ~UINT64_C(255); CHECK(!mode_ok(&s));
}

static void descriptor_checks(void)
{
    const uint64_t code = UINT64_C(0x00af9b000000ffff);
    CHECK(code_descriptor_ok(code));
    /* No GDT permission change occurs in this read-only profile, so a valid
     * descriptor whose Accessed bit is clear is accepted unchanged. */
    CHECK(code_descriptor_ok(code & ~BIT(40)));
    CHECK(!code_descriptor_ok(code & ~BIT(53)));
    CHECK(!code_descriptor_ok(code | BIT(54)));
    CHECK(!code_descriptor_ok(code | BIT(45)));
    CHECK(!code_descriptor_ok(code & ~BIT(47)));
    CHECK(!code_descriptor_ok(code & ~BIT(44)));
    struct x86_state s = {.cs = 8, .gdtr = {15, 0x20000},
                          .idtr = {4095, 0x10000}};
    struct boot_range image = {0x10000, 0x20000};
    CHECK(descriptor_bounds_ok(&s, image));
    s.cs = 0; CHECK(!descriptor_bounds_ok(&s, image));
    s.cs = 9; CHECK(!descriptor_bounds_ok(&s, image));
    s.cs = 16; CHECK(!descriptor_bounds_ok(&s, image));
    s.cs = 8; s.gdtr.limit = 14; CHECK(!descriptor_bounds_ok(&s, image));
    s.gdtr.limit = 15; s.gdtr.base = LOW_VA_LIMIT - 8;
    CHECK(!descriptor_bounds_ok(&s, image));
    s.gdtr.base = 0x20000; s.idtr.limit = 4094;
    CHECK(!descriptor_bounds_ok(&s, image));
    s.idtr.limit = 4095; s.idtr.base = image.end - 4095;
    CHECK(!descriptor_bounds_ok(&s, image));
    s.idtr.base = image.start; s.gdtr.base = 0x20001;
    CHECK(!descriptor_bounds_ok(&s, image));
}

static void read_admission_checks(void)
{
    struct old_mapping m;
    struct x86_state s = fixture();
    /* 0x1000 is deliberately outside any host object. Both paths must refuse
     * from metadata before attempting a physical table read. */
    s.cr3 = 0x1000;
    CHECK(old_walk(&s, 0, &m) == WALK_BAD);
    CHECK(s.nr_visited == 0);
    s = fixture(); s.cr3 |= 1u;
    CHECK(old_walk(&s, 0, &m) == WALK_BAD);
    CHECK(s.nr_visited == 0);
    s = fixture(); s.cr3 |= BIT(3);
    CHECK(old_walk(&s, 0, &m) == WALK_BAD);
    CHECK(s.nr_visited == 0);
    s = fixture(); old_pages[0][0] = 0x1003;
    CHECK(old_walk(&s, 0, &m) == WALK_BAD);
    CHECK(s.nr_visited == 1);
    s = fixture(); old_pages[0][0] = 0x1003;
    baseline.nr_reserved = 2;
    baseline.reserved[0] = (struct boot_range){0x1000, 0x2000};
    baseline.reserved[1] = (struct boot_range){
        (uintptr_t)old_pages, (uintptr_t)old_pages + sizeof old_pages};
    baseline.nr_candidates = 1;
    baseline.candidates[0] = (struct boot_range){0x1000, 0x2000};
    CHECK(old_walk(&s, 0, &m) == WALK_BAD);
    CHECK(s.nr_visited == 1);

    s = fixture();
    baseline.reserved[0].end = baseline.reserved[0].start + OP_PAGE - 1u;
    CHECK(old_walk(&s, 0, &m) == WALK_BAD);
    CHECK(s.nr_visited == 0);
    s = fixture();
    baseline.reserved[0].start++;
    CHECK(old_walk(&s, 0, &m) == WALK_BAD);
    CHECK(s.nr_visited == 0);
    s = fixture();
    baseline.nr_candidates = 1;
    baseline.candidates[0] = (struct boot_range){s.cr3, s.cr3 + OP_PAGE};
    CHECK(old_walk(&s, 0, &m) == WALK_BAD);
    CHECK(s.nr_visited == 0);
    s = fixture();
    baseline.nr_reserved = BOOT_MEMORY_INPUT_MAX + 1u;
    CHECK(old_walk(&s, 0, &m) == WALK_BAD);
    s = fixture();
    baseline.nr_candidates = BOOT_MEMORY_RANGE_MAX + 1u;
    CHECK(old_walk(&s, 0, &m) == WALK_BAD);
    s = fixture(); input.map = NULL;
    CHECK(old_walk(&s, 0, &m) == WALK_BAD);
}

static void old_walk_checks(void)
{
    const uint64_t va = UINT64_C(0x12345000), pa = UINT64_C(0x76543000);
    struct x86_state s = fixture();
    struct old_mapping m;
    *leaf(va, 3) = pa | 3u;
    CHECK(old_walk(&s, va + 4079, &m) == WALK_OK && m.pa == pa + 4079 &&
          m.writable && m.executable && m.type == 6);
    CHECK(s.nr_visited == 4);
    *leaf(va, 3) = pa | 1u | BIT(63);
    CHECK(old_walk(&s, va, &m) == WALK_OK && !m.writable && !m.executable);
    *leaf(va, 3) = pa | 3u;
    *leaf(va, 0) = (*leaf(va, 0) & ~BIT(1)) | BIT(63);
    CHECK(old_walk(&s, va, &m) == WALK_OK && !m.writable && !m.executable);
    CHECK(old_walk(&s, LOW_VA_LIMIT - OP_PAGE, &m) == WALK_MISSING);
    CHECK(old_walk(&s, LOW_VA_LIMIT, &m) == WALK_BAD);
    *leaf(va, 3) = pa | 3u | BIT(51);
    CHECK(old_walk(&s, va, &m) == WALK_BAD);
    *leaf(va, 3) = pa | 3u | BIT(60);
    CHECK(old_walk(&s, va, &m) == WALK_BAD);
    s = fixture(); *leaf(va, 2) = UINT64_C(0x20000000) | 0x83u;
    CHECK(old_walk(&s, va + 17, &m) == WALK_OK &&
          m.pa == UINT64_C(0x20000000) + (va & 0x1fffffu) + 17);
    *leaf(va, 2) |= BIT(13);
    CHECK(old_walk(&s, va, &m) == WALK_BAD);
    s = fixture(); *leaf(va, 1) = UINT64_C(0x80000000) | 0x83u;
    CHECK(old_walk(&s, va, &m) == WALK_OK &&
          m.pa == UINT64_C(0x80000000) + va);
    s.one_gib = 0; CHECK(old_walk(&s, va, &m) == WALK_BAD);
    s = fixture(); *leaf(va, 0) = 0x83;
    CHECK(old_walk(&s, va, &m) == WALK_BAD);
    s = fixture(); *leaf(va, 0) = (uintptr_t)old_pages[1] | 0x1bu;
    CHECK(old_walk(&s, va, &m) == WALK_BAD);
}

static void closure_checks(void)
{
    struct x86_state s = fixture();
    struct old_mapping m;
    for (size_t i = 0; i < sizeof old_pages / sizeof *old_pages; ++i) {
        uint64_t address = (uintptr_t)old_pages[i];
        *leaf(address, 3) = address | 3u;
    }
    CHECK(old_walk(&s, (uintptr_t)old_pages, &m) == WALK_OK);
    CHECK(identity_tables(&s));
    struct boot_range image = {(uintptr_t)old_pages,
                              (uintptr_t)old_pages + sizeof old_pages};
    CHECK(image_identity(&s, image));
    *leaf(image.end - OP_PAGE, 3) = 0;
    CHECK(!image_identity(&s, image));
    *leaf(image.end - OP_PAGE, 3) = (image.end - OP_PAGE) | 3u;
    *leaf((uintptr_t)old_pages, 3) = ((uintptr_t)old_pages + OP_PAGE) | 3u;
    CHECK(!identity_tables(&s));
    s = fixture();
    s.nr_visited = OP_TABLES_MAX;
    for (size_t i = 0; i < OP_TABLES_MAX; ++i)
        s.visited[i] = (i + 1u) * OP_PAGE;
    CHECK(!remember_table(&s, (uintptr_t)old_pages));
    CHECK(!remember_table(&s, 0));
    CHECK(!remember_table(&s, (uintptr_t)old_pages + 1u));
    CHECK(!remember_table(&s, LOW_VA_LIMIT));
}

static void check_record(struct x86_state *s, uint64_t va, uint64_t pa,
                         unsigned levels)
{
    struct old_mapping mapping;
    struct op_walk record;
    memset(&record, 0xa5, sizeof record);
    CHECK(old_walk_record(s, va, &mapping, &record) == WALK_OK);
    CHECK(record.kind == 0 && record.levels == levels);
    CHECK(record.va == va && record.pa == pa && mapping.pa == pa);
    const uint64_t *table = old_pages[0];
    for (unsigned i = 0; i < levels; ++i) {
        const unsigned index = (unsigned)((va >> (39u - i * 9u)) & 511u);
        CHECK(record.tables[i] == (uintptr_t)table);
        CHECK(record.entries[i] == table[index]);
        if (i + 1u < levels)
            table = (const uint64_t *)(uintptr_t)(table[index] & ENTRY_ADDRESS);
    }
    for (unsigned i = levels; i < 4; ++i)
        CHECK(record.tables[i] == 0 && record.entries[i] == 0);
}

static void walk_record_checks(void)
{
    const uint64_t va = UINT64_C(0x12345000), pa = UINT64_C(0x76543000);
    struct x86_state s = fixture();
    *leaf(va, 3) = pa | 3u;
    check_record(&s, va + 53u, pa + 53u, 4);
    s = fixture(); *leaf(va, 2) = UINT64_C(0x20000000) | 0x83u;
    check_record(&s, va + 19u,
                 UINT64_C(0x20000000) + (va & 0x1fffffu) + 19u, 3);
    s = fixture(); *leaf(va, 1) = UINT64_C(0x80000000) | 0x83u;
    check_record(&s, va + 7u, UINT64_C(0x80000000) + va + 7u, 2);
    s = fixture();
    old_pages[0][0] = 0x1003;
    struct op_walk record;
    struct old_mapping mapping;
    CHECK(old_walk_record(&s, 0, &mapping, &record) == WALK_BAD);
    CHECK(record.levels == 1 && record.tables[0] == (uintptr_t)old_pages);
    CHECK(record.entries[0] == 0x1003 && record.tables[1] == 0);

    s = fixture();
    for (size_t i = 0; i < sizeof old_pages / sizeof *old_pages; ++i) {
        uint64_t address = (uintptr_t)old_pages[i];
        *leaf(address, 3) = address | 3u;
    }
    CHECK(remember_table(&s, (uintptr_t)old_pages));
    CHECK(s.nr_visited == 1);
    CHECK(record_identity_tables(&s, &input));
    CHECK(s.nr_visited > 1 && input.nr_walks == s.nr_visited);
    for (size_t i = 0; i < s.nr_visited; ++i) {
        CHECK(input.walks[i].kind == OP_WALK_TABLE);
        CHECK(input.walks[i].va == s.visited[i] &&
              input.walks[i].pa == s.visited[i]);
        CHECK(input.walks[i].levels == 4);
    }
    memset(&input.walks[OP_WALKS_MAX - 1u], 0x5a, sizeof *input.walks);
    struct op_walk last = input.walks[OP_WALKS_MAX - 1u];
    input.nr_walks = OP_WALKS_MAX;
    CHECK(!record_identity_tables(&s, &input));
    CHECK(input.nr_walks == OP_WALKS_MAX);
    CHECK(!memcmp(&last, &input.walks[OP_WALKS_MAX - 1u], sizeof last));
    input.nr_walks = OP_WALKS_MAX - 1u;
    CHECK(!record_identity_tables(&s, &input));
    CHECK(input.nr_walks == OP_WALKS_MAX);
    CHECK(input.walks[OP_WALKS_MAX - 1u].kind == OP_WALK_TABLE);
    input.nr_walks = 0;
    baseline.nr_candidates = 1;
    baseline.candidates[0] = (struct boot_range){s.cr3, s.cr3 + OP_PAGE};
    CHECK(!record_identity_tables(&s, &input));
    CHECK(input.nr_walks == 0);
}

static void store64(unsigned char *p, uint64_t value)
{
    for (unsigned i = 0; i < 8; ++i) p[i] = (unsigned char)(value >> (i * 8u));
}

static void descriptor_content_checks(void)
{
    struct x86_state s = fixture();
    memset(image_bytes, 0, sizeof image_bytes);
    memset(gdt_bytes, 0, sizeof gdt_bytes);
    struct boot_range image = {(uintptr_t)image_bytes,
                              (uintptr_t)image_bytes + sizeof image_bytes};
    baseline.reserved[baseline.nr_reserved++] = image;
    baseline.reserved[baseline.nr_reserved++] = (struct boot_range){
        (uintptr_t)gdt_bytes, (uintptr_t)gdt_bytes + sizeof gdt_bytes};
    for (size_t i = 0; i < sizeof old_pages / sizeof *old_pages; ++i) {
        uint64_t address = (uintptr_t)old_pages[i];
        *leaf(address, 3) = address | 3u;
    }
    for (uint64_t address = image.start; address < image.end; address += OP_PAGE)
        *leaf(address, 3) = address | 3u;
    for (size_t i = 0; i < sizeof gdt_bytes / OP_PAGE; ++i) {
        uint64_t address = (uintptr_t)gdt_bytes + i * OP_PAGE;
        *leaf(address, 3) = address | 3u;
    }
    /* Both the GDT and an unaligned 4096-byte IDT span two physical pages. */
    s.cs = 8;
    s.gdtr.base = (uintptr_t)gdt_bytes + OP_PAGE - 8u;
    s.gdtr.limit = 15;
    s.idtr.base = image.start + 37u;
    s.idtr.limit = 4095;
    store64(gdt_bytes + OP_PAGE, UINT64_C(0x00af9a000000ffff));
    const uint64_t target = image.start + 2u * OP_PAGE + 128u;
    for (unsigned i = 0; i < 256; ++i) {
        const uint64_t low = (target & 65535u) | (uint64_t)s.cs << 16 |
            UINT64_C(0x8e) << 40 | ((target >> 16) & 65535u) << 48;
        store64(image_bytes + 37u + i * 16u, low);
        store64(image_bytes + 37u + i * 16u + 8u, target >> 32);
    }
    CHECK(image_identity(&s, image));
    struct old_mapping mapping;
    for (size_t i = 0; i < sizeof gdt_bytes / OP_PAGE; ++i) {
        const uint64_t address = (uintptr_t)gdt_bytes + i * OP_PAGE;
        CHECK(old_walk(&s, address, &mapping) == WALK_OK && mapping.pa == address);
    }
    CHECK(identity_tables(&s));
    CHECK(descriptor_bounds_ok(&s, image));
    CHECK(descriptors_ok(&s, image));
    *leaf(target, 3) |= BIT(63);
    CHECK(!descriptors_ok(&s, image));
    *leaf(target, 3) &= ~BIT(63);
    CHECK(descriptors_ok(&s, image));
    image_bytes[37u + 2u] ^= 8u; /* Gate's selector no longer selects CS. */
    CHECK(!descriptors_ok(&s, image));
    image_bytes[37u + 2u] ^= 8u;
    image_bytes[37u + 4u] = 1u; /* No unproved IST stack is accepted. */
    CHECK(!descriptors_ok(&s, image));
    image_bytes[37u + 4u] = 0;
    image_bytes[37u + 5u] = 0x8fu; /* Narrow interrupt-gate profile. */
    CHECK(!descriptors_ok(&s, image));
    image_bytes[37u + 5u] = 0x8eu;
    image_bytes[37u + 12u] = 1u; /* Reserved gate bits must be zero. */
    CHECK(!descriptors_ok(&s, image));
    image_bytes[37u + 12u] = 0;
    image_bytes[37u + 255u * 16u + 12u] = 1u;
    CHECK(!descriptors_ok(&s, image)); /* Check the last gate across page two. */
    image_bytes[37u + 255u * 16u + 12u] = 0;
    store64(gdt_bytes + OP_PAGE, UINT64_C(0x00af9a000000ffff) & ~BIT(53));
    CHECK(!descriptors_ok(&s, image));
}

int main(void)
{
    mode_checks();
    descriptor_checks();
    read_admission_checks();
    old_walk_checks();
    closure_checks();
    walk_record_checks();
    descriptor_content_checks();
    printf("x86 ownership mode/descriptor/read-admission/old-walk checks PASS (%u)\n",
           checks);
    return 0;
}
