#include "styleos/ownership.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* All live/claim addresses are numeric fixtures, including completely unmapped
 * high GDT VAs. A successful host run must not read or write those addresses. */
static struct op_input in, saved;
static struct op_output out;
static struct op_scratch scratch;
static struct boot_memory_map map, saved_map;

static void reserve(uint64_t base, uint64_t bytes)
{
    assert(bm_add(&map, base, bytes, 1) == BM_OK);
}

static void claim(unsigned kind, unsigned proof, uint64_t va, uint64_t pa,
                  uint64_t bytes)
{
    assert(op_add_claim(&in, kind, proof, va, pa, bytes) == OP_OK);
}

static void object(unsigned kind, unsigned proof, struct boot_range r)
{
    claim(kind, proof, r.start, r.start, r.end - r.start);
}

static void identity_walk(uint64_t page)
{
    assert(in.nr_walks < OP_WALKS_MAX);
    /* All table fixtures lie in the 4..6 MiB identity 2-MiB leaf. */
    assert(page >= 0x400000 && page < 0x600000);
    in.walks[in.nr_walks++] = (struct op_walk){
        .kind = OP_WALK_TABLE, .levels = 3, .va = page, .pa = page,
        .tables = {0x400000, 0x401000, 0x402000, 0},
        .entries = {0x401003, 0x402003, 0x400083, 0}
    };
}

static void gdt_walk(uint64_t va, uint64_t pa)
{
    assert(in.nr_walks < OP_WALKS_MAX);
    in.walks[in.nr_walks++] = (struct op_walk){
        .kind = OP_WALK_GDT, .levels = 4, .va = va, .pa = pa,
        .tables = {0x400000, 0x401000, 0x402000, 0x403000},
        .entries = {0x401003, 0x402003, 0x403003, pa | 3u}
    };
}

static void fixture(unsigned arch)
{
    memset(&in, 0, sizeof in);
    memset(&map, 0, sizeof map);
    in.arch = arch;
    in.map = &map;
    in.live.image = (struct boot_range){0x100003, 0x160123};
    in.live.stack = (struct boot_range){0x130000, 0x140000};
    in.live.handoff = (struct boot_range){0x130100, 0x130160};
    in.live.source = (struct boot_range){0x200008, 0x200068};
    in.map_workspace = (struct boot_range){0x140000, 0x140000 + OP_DIAG_BYTES};
    in.own_workspace = (struct boot_range){0x110000, 0x120000};
    in.pool = (struct boot_range){0x120000, 0x128000};
    in.pool_metadata = (struct boot_range){0x128000, 0x129000};
    assert(bm_add(&map, 0x10000, 0xff0000, 0) == BM_OK);
    reserve(in.live.image.start, in.live.image.end - in.live.image.start);
    reserve(in.live.source.start, in.live.source.end - in.live.source.start);
    reserve(0x9000000, OP_PAGE);
    /* Represents an excluded/unknown firmware type outside known RAM. */
    reserve(0x9002000, OP_PAGE);
    in.snapshot.pa_bits = 40;
    object(OP_IMAGE, OP_IMAGE_IDENTITY, in.live.image);
    object(OP_STACK, OP_IMAGE_IDENTITY, in.live.stack);
    object(OP_HANDOFF, OP_IMAGE_IDENTITY, in.live.handoff);
    object(OP_SOURCE_BUFFER, OP_BOOT_SOURCE, in.live.source);
    object(OP_MAP_WORKSPACE, OP_IMAGE_IDENTITY, in.map_workspace);
    object(OP_OWN_WORKSPACE, OP_IMAGE_IDENTITY, in.own_workspace);
    object(OP_BOOT_POOL, OP_IMAGE_IDENTITY, in.pool);
    object(OP_POOL_METADATA, OP_IMAGE_IDENTITY, in.pool_metadata);
    if (arch == STYLEOS_ARCH_AARCH64) {
        in.source = OP_FDT;
        in.snapshot.phase = OP_MMU_OFF;
        in.snapshot.mode = 0x30d00800;
        in.snapshot.aux0 = 0x146000;
        in.snapshot.aux1 = 0x3c0;
        claim(OP_VECTORS, OP_IMAGE_IDENTITY, 0x146000, 0x146000, 2048);
        claim(OP_MMIO, OP_FIXED_DEVICE, 0x9000000, 0x9000000, OP_PAGE);
    } else {
        in.source = OP_UEFI;
        in.source_stride = 48;
        in.source_version = 1;
        in.unresolved = OP_TABLE_SET_INCOMPLETE | OP_OTHER_FIRMWARE_STATE;
        in.snapshot.phase = OP_INHERITED;
        in.snapshot.mode = 0x80000001;
        in.snapshot.root = 0x400000;
        in.snapshot.aux0 = 0x20;
        in.snapshot.aux1 = 0xd00;
        in.snapshot.pat = 6;
        in.snapshot.features = 1;
        in.snapshot.gdt_va = UINT64_C(0xffff800000009ff8);
        in.snapshot.gdt_bytes = 24;
        in.snapshot.idt_va = 0x144011;
        in.snapshot.idt_bytes = OP_PAGE;
        in.snapshot.cs = 8;
        reserve(0x300000, 2 * OP_PAGE);
        reserve(0x400000, 4 * OP_PAGE);
        claim(OP_GDT, OP_OLD_WALK, UINT64_C(0xffff800000009000), 0x300000, OP_PAGE);
        claim(OP_GDT, OP_OLD_WALK, UINT64_C(0xffff80000000a000), 0x301000, OP_PAGE);
        claim(OP_IDT, OP_OLD_WALK, 0x144011, 0x144011, OP_PAGE);
        claim(OP_ROOT, OP_REGISTER, 0, 0x400000, OP_PAGE);
        claim(OP_OLD_TABLE, OP_OLD_WALK, 0x400000, 0x400000, OP_PAGE);
        claim(OP_OLD_TABLE, OP_OLD_WALK, 0x401000, 0x401000, OP_PAGE);
        claim(OP_OLD_TABLE, OP_OLD_WALK, 0x402000, 0x402000, OP_PAGE);
        claim(OP_OLD_TABLE, OP_OLD_WALK, 0x403000, 0x403000, OP_PAGE);
        gdt_walk(UINT64_C(0xffff800000009000), 0x300000);
        gdt_walk(UINT64_C(0xffff80000000a000), 0x301000);
        for (uint64_t page = 0x400000; page < 0x404000; page += OP_PAGE)
            identity_walk(page);
    }
    assert(bm_normalize(&map) == BM_OK);
}

static void expect_range(struct boot_range r, uint64_t start, uint64_t end)
{
    assert(r.start == start && r.end == end);
}

static void refused(enum op_error expected)
{
    memset(&out, 0xa5, sizeof out);
    enum op_error actual = op_build(&in, &out, &scratch);
    if (actual != expected)
        fprintf(stderr, "expected %s, got %s\n", op_error_name(expected), op_error_name(actual));
    assert(actual == expected);
    assert(!out.valid && !out.nr_protected && !out.nr_review &&
           !out.protected_pages && !out.review_pages && !out.granted);
}

static uint64_t pages(const struct boot_range *rs, size_t n)
{
    uint64_t total = 0;
    for (size_t i = 0; i < n; ++i) {
        assert(rs[i].start < rs[i].end);
        assert(!((rs[i].start | rs[i].end) & (OP_PAGE - 1u)));
        if (i) assert(rs[i - 1].end < rs[i].start);
        total += (rs[i].end - rs[i].start) / OP_PAGE;
    }
    return total;
}

static void positive(void)
{
    for (unsigned arch = STYLEOS_ARCH_X86_64; arch <= STYLEOS_ARCH_AARCH64; ++arch) {
        fixture(arch);
        saved = in; saved_map = map;
        assert(op_build(&in, &out, &scratch) == OP_OK);
        assert(out.valid && !out.granted);
        assert(!memcmp(&saved, &in, sizeof in));
        assert(!memcmp(&saved_map, &map, sizeof map));
        assert(out.protected_pages == pages(out.protected_ranges, out.nr_protected));
        assert(out.review_pages == pages(out.review, out.nr_review));
        assert(out.protected_pages == (arch == STYLEOS_ARCH_AARCH64 ? 100 : 106));
        assert(out.review_pages + out.protected_pages == 0xff0 + 2);
        expect_range(out.protected_ranges[0], 0x100000, 0x161000);
        expect_range(out.review[0], 0x10000, 0x100000);
        expect_range(out.protected_ranges[out.nr_protected - 1], 0x9002000, 0x9003000);
        for (size_t i = 0; i < out.nr_review; ++i) {
            assert(bm_ram_contains(&map, out.review[i]));
            for (size_t j = 0; j < out.nr_protected; ++j)
                assert(out.review[i].end <= out.protected_ranges[j].start ||
                       out.protected_ranges[j].end <= out.review[i].start);
        }
    }
}

static void nested_and_adjacent(void)
{
    fixture(STYLEOS_ARCH_X86_64);
    /* Explicit dependencies can protect candidate pages, but never grant them.
     * Overlap and adjacency are a union, including GDT aliases and table pages. */
    reserve(0x500001, OP_PAGE - 1);
    reserve(0x501000, OP_PAGE);
    assert(bm_normalize(&map) == BM_OK);
    claim(OP_OLD_TABLE, OP_OLD_WALK, 0x500000, 0x500000, OP_PAGE);
    claim(OP_OLD_TABLE, OP_OLD_WALK, 0x501000, 0x501000, OP_PAGE);
    identity_walk(0x500000);
    identity_walk(0x501000);
    assert(op_build(&in, &out, &scratch) == OP_OK);
    assert(out.protected_pages == 108 && !out.granted);
    int found = 0;
    for (size_t i = 0; i < out.nr_protected; ++i)
        if (out.protected_ranges[i].start == 0x500000) {
            expect_range(out.protected_ranges[i], 0x500000, 0x502000);
            found = 1;
        }
    assert(found);

    fixture(STYLEOS_ARCH_AARCH64);
    /* Partial image and source pages remain protected in their entirety. */
    map.nr_reserved = 0;
    reserve(0x100001, 0x1001);
    reserve(0x100800, 0x60000);
    reserve(0x160100, 0x23);
    reserve(0x200008, 0x60);
    reserve(0x9000000, OP_PAGE);
    reserve(0x9002000, OP_PAGE);
    assert(bm_normalize(&map) == BM_OK);
    assert(op_build(&in, &out, &scratch) == OP_OK);
    expect_range(out.protected_ranges[0], 0x100000, 0x161000);
    assert(out.protected_pages == 100 && !out.granted);
}

static void malformed_map(void)
{
    fixture(STYLEOS_ARCH_AARCH64); map.nr_ram = SIZE_MAX; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.nr_reserved = BOOT_MEMORY_INPUT_MAX + 1u; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.nr_candidates = BOOT_MEMORY_RANGE_MAX + 1u; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.nr_candidates = 0; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.ram[0].end = map.ram[0].start; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.reserved[0].end = map.reserved[0].start; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.reserved[0].start++; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.candidates[0].start++; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.candidates[0].end -= OP_PAGE; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.candidates[0].end = 0x110000; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.candidates[0] = (struct boot_range){0x8000, 0x9000}; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.ram[1] = map.ram[0]; map.nr_ram = 2; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.reserved[1] = map.reserved[0]; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.candidates[1] = map.candidates[0]; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); map.ram[0].end = (UINT64_C(1) << 40) + 1; refused(OP_MAP);
    fixture(STYLEOS_ARCH_AARCH64); in.map = 0; refused(OP_MAP);
    /* A wholly omitted candidate partition is rejected even when each listed
     * candidate is independently aligned, in RAM and clear of reserves. */
    fixture(STYLEOS_ARCH_AARCH64); --map.nr_candidates; refused(OP_MAP);
}

static void mandatory_and_proofs(void)
{
    for (unsigned arch = STYLEOS_ARCH_X86_64; arch <= STYLEOS_ARCH_AARCH64; ++arch) {
        fixture(arch);
        size_t count = in.nr_claims;
        for (size_t i = 0; i < count; ++i) {
            fixture(arch);
            /* Removing one of multiple OLD_TABLEs need not lose the declared
             * partial table set; every other single fixture claim is required. */
            if (in.claims[i].kind == OP_OLD_TABLE) continue;
            in.claims[i] = in.claims[--in.nr_claims];
            refused(OP_REQUIRED);
        }
        for (size_t i = 0; i < count; ++i) {
            fixture(arch);
            in.claims[i].proof = OP_UNPROVEN;
            refused(OP_REQUIRED);
        }
        fixture(arch); in.claims[0].proof = 99; refused(OP_PROOF);
        fixture(arch); in.claims[0].kind = 99; refused(OP_ARGUMENT);
        fixture(arch); in.claims[0].pa++; refused(OP_CONTEXT);
        fixture(arch); in.claims[0].bytes--; refused(OP_CONTEXT);
        fixture(arch); in.claims[0].va++; refused(OP_CONTEXT);
        fixture(arch); in.claims[3].proof = OP_IMAGE_IDENTITY; refused(OP_REQUIRED);
        fixture(arch); in.claims[in.nr_claims++] = in.claims[0]; refused(OP_CONTEXT);
    }
    fixture(STYLEOS_ARCH_X86_64);
    in.claims[8].va = in.live.image.start & ~(OP_PAGE - 1u);
    in.snapshot.gdt_va = in.claims[8].va + 8;
    in.snapshot.gdt_bytes = 16;
    in.claims[9] = in.claims[--in.nr_claims];
    /* Its physical proof contradicts the image's identity for these VA bytes. */
    refused(OP_CONTEXT);

    fixture(STYLEOS_ARCH_X86_64); in.claims[8].pa = 0x70000000; refused(OP_RANGE);
    fixture(STYLEOS_ARCH_X86_64);
    /* A known dependency is valid in baseline-protected non-RAM. */
    in.claims[8].pa = 0x9002000;
    in.walks[0].pa = 0x9002000;
    in.walks[0].entries[3] = 0x9002003;
    assert(op_build(&in, &out, &scratch) == OP_OK && !out.granted);
    fixture(STYLEOS_ARCH_X86_64);
    in.claims[in.nr_claims - 1].kind = OP_MMIO; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_AARCH64);
    in.claims[in.nr_claims - 1].kind = OP_GDT; refused(OP_PLATFORM);
}

static void invalid_context(void)
{
    fixture(STYLEOS_ARCH_AARCH64); in.live.image.end = in.live.image.start; refused(OP_RANGE);
    fixture(STYLEOS_ARCH_AARCH64); in.live.source.end = in.live.source.start; refused(OP_SOURCE);
    fixture(STYLEOS_ARCH_AARCH64); in.live.stack.end = in.live.image.end + 1; refused(OP_CONTEXT);
    fixture(STYLEOS_ARCH_AARCH64); in.map_workspace.end--; refused(OP_CONTEXT);
    fixture(STYLEOS_ARCH_AARCH64); in.own_workspace.end--; refused(OP_CONTEXT);
    fixture(STYLEOS_ARCH_AARCH64); in.pool.end--; refused(OP_CONTEXT);
    fixture(STYLEOS_ARCH_AARCH64); in.pool_metadata.end--; refused(OP_CONTEXT);
    fixture(STYLEOS_ARCH_AARCH64); in.live.handoff.end--; refused(OP_CONTEXT);
    fixture(STYLEOS_ARCH_AARCH64); in.own_workspace.start++; in.own_workspace.end++; refused(OP_ALIGNMENT);
    fixture(STYLEOS_ARCH_AARCH64); in.pool.start++; in.pool.end++; refused(OP_ALIGNMENT);
    fixture(STYLEOS_ARCH_AARCH64); in.pool_metadata.start++; in.pool_metadata.end++; refused(OP_ALIGNMENT);
    fixture(STYLEOS_ARCH_AARCH64); in.pool = in.own_workspace; refused(OP_CONTEXT);
    fixture(STYLEOS_ARCH_AARCH64);
    in.own_workspace = in.live.stack;
    refused(OP_CONTEXT);
    fixture(STYLEOS_ARCH_AARCH64);
    in.map_workspace = (struct boot_range){in.pool.start, in.pool.start + OP_DIAG_BYTES};
    refused(OP_CONTEXT);
    fixture(STYLEOS_ARCH_AARCH64);
    in.map_workspace = (struct boot_range){in.live.source.start, in.live.source.start + OP_DIAG_BYTES};
    refused(OP_CONTEXT);
    fixture(STYLEOS_ARCH_AARCH64);
    map.reserved[0].end = 0x160000;
    assert(bm_normalize(&map) == BM_OK);
    refused(OP_REQUIRED);
    fixture(STYLEOS_ARCH_AARCH64);
    map.reserved[1] = map.reserved[--map.nr_reserved];
    assert(bm_normalize(&map) == BM_OK);
    refused(OP_REQUIRED);
    fixture(STYLEOS_ARCH_AARCH64);
    map.reserved[2] = map.reserved[--map.nr_reserved];
    assert(bm_normalize(&map) == BM_OK);
    refused(OP_RANGE); /* PL011 is neither RAM nor protected anymore. */
}

static void unsupported_platform(void)
{
    for (unsigned arch = STYLEOS_ARCH_X86_64; arch <= STYLEOS_ARCH_AARCH64; ++arch) {
        fixture(arch); in.source = 99; refused(OP_SOURCE);
        fixture(arch); in.source = arch == STYLEOS_ARCH_X86_64 ? OP_FDT : OP_UEFI; refused(OP_SOURCE);
        fixture(arch); in.snapshot.phase = 99; refused(OP_PLATFORM);
        fixture(arch); in.snapshot.pa_bits = 31; refused(OP_PLATFORM);
        fixture(arch); in.snapshot.pa_bits = arch == STYLEOS_ARCH_X86_64 ? 53 : 49; refused(OP_PLATFORM);
        fixture(arch); in.unresolved ^= OP_TABLE_SET_INCOMPLETE; refused(OP_PLATFORM);
        fixture(arch); in.source_version = 2; refused(OP_SOURCE);
    }
    fixture(STYLEOS_ARCH_AARCH64); in.arch = 99; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_AARCH64); in.snapshot.mode |= 1; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_AARCH64); in.snapshot.root = OP_PAGE; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_AARCH64); in.snapshot.aux0++; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_AARCH64); in.snapshot.aux1 = 0; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_AARCH64); in.snapshot.pat = 6; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_X86_64); in.source_stride = 40; refused(OP_SOURCE);
    fixture(STYLEOS_ARCH_X86_64); in.source_stride = 47; refused(OP_SOURCE);
    fixture(STYLEOS_ARCH_X86_64); in.snapshot.mode = 0; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_X86_64); in.snapshot.aux0 |= 1u << 12; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_X86_64); in.snapshot.aux0 |= 1u << 17; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_X86_64); in.snapshot.aux1 = 0x500; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_X86_64); in.snapshot.pat = 0; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_X86_64); in.snapshot.rflags = 1u << 9; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_X86_64); in.snapshot.root++; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_X86_64); in.snapshot.gdt_va++; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_X86_64); in.snapshot.cs = 3; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_X86_64); in.snapshot.idt_bytes--; refused(OP_PLATFORM);
}

static void descriptor_evidence(void)
{
    fixture(STYLEOS_ARCH_X86_64); --in.nr_walks; refused(OP_REQUIRED);
    fixture(STYLEOS_ARCH_X86_64); in.nr_walks = OP_WALKS_MAX + 1u; refused(OP_LIMIT);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].kind = 99; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].levels = 1; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].levels = 5; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[in.nr_walks++] = in.walks[0]; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); identity_walk(0x500000); refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].tables[0] = 0x401000; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].tables[1] = 0x500000; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].tables[1]++; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].entries[0] &= ~UINT64_C(1); refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].entries[0] |= UINT64_C(1) << 60; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].entries[0] |= UINT64_C(1) << 45; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].entries[0] |= 128; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].entries[0] |= 8; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].entries[3] += OP_PAGE; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[0].entries[3] |= 8; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[2].entries[2] |= 0x2000; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[2].entries[3] = 1; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.walks[2].tables[3] = 0x403000; refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64); in.snapshot.features = 0; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_X86_64); in.snapshot.features |= 4; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_AARCH64); in.nr_walks = 1; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_AARCH64); in.snapshot.features = 1; refused(OP_PLATFORM);
    fixture(STYLEOS_ARCH_X86_64);
    /* CPU Accessed/Dirty bookkeeping may change between bounded captures. */
    in.walks[2].entries[0] |= 0x60;
    assert(op_build(&in, &out, &scratch) == OP_OK && !out.granted);
    fixture(STYLEOS_ARCH_X86_64);
    /* A descriptor may not disagree between two otherwise valid VA paths. */
    in.walks[2].entries[0] |= 2u << 8;
    refused(OP_PROOF);
    fixture(STYLEOS_ARCH_X86_64);
    /* Every old table PA must already be reserved before an architecture
     * walker could read it, even if it is ordinary candidate RAM. */
    map.reserved[3].end = 0x403000;
    assert(bm_normalize(&map) == BM_OK);
    refused(OP_REQUIRED);

    fixture(STYLEOS_ARCH_X86_64);
    /* Valid 4-KiB table-identity leaves, sharing the GDT's leaf table. */
    for (size_t i = 2; i < in.nr_walks; ++i) {
        struct op_walk *w = &in.walks[i];
        w->levels = 4;
        w->tables[3] = 0x403000;
        w->entries[2] = 0x403003;
        w->entries[3] = w->pa | 3u;
    }
    assert(op_build(&in, &out, &scratch) == OP_OK && !out.granted);

    fixture(STYLEOS_ARCH_X86_64);
    /* Valid 1-GiB table-identity leaves require the actual CPU capability. */
    in.snapshot.features |= 2;
    in.snapshot.gdt_va += UINT64_C(1) << 30;
    for (size_t i = 0; i < 2; ++i) {
        in.claims[8 + i].va += UINT64_C(1) << 30;
        in.walks[i].va += UINT64_C(1) << 30;
    }
    for (size_t i = 2; i < in.nr_walks; ++i) {
        struct op_walk *w = &in.walks[i];
        w->levels = 2;
        w->tables[2] = w->tables[3] = 0;
        w->entries[1] = 0x83;
        w->entries[2] = w->entries[3] = 0;
    }
    assert(op_build(&in, &out, &scratch) == OP_OK && !out.granted);
    in.snapshot.features &= ~2u;
    refused(OP_PROOF);
}

static void arithmetic_and_limits(void)
{
    fixture(STYLEOS_ARCH_AARCH64);
    size_t before = in.nr_claims;
    assert(op_add_claim(0, OP_IMAGE, OP_IMAGE_IDENTITY, 1, 1, 1) == OP_ARGUMENT);
    assert(op_add_claim(&in, 99, OP_IMAGE_IDENTITY, 1, 1, 1) == OP_ARGUMENT);
    assert(op_add_claim(&in, OP_IMAGE, 99, 1, 1, 1) == OP_PROOF);
    assert(op_add_claim(&in, OP_IMAGE, OP_IMAGE_IDENTITY, 1, 1, 0) == OP_RANGE);
    assert(op_add_claim(&in, OP_IMAGE, OP_IMAGE_IDENTITY, 0, UINT64_MAX, 1) == OP_OVERFLOW);
    assert(op_add_claim(&in, OP_IMAGE, OP_IMAGE_IDENTITY, UINT64_MAX, 0, 1) == OP_OVERFLOW);
    assert(in.nr_claims == before);
    in.nr_claims = OP_CLAIMS_MAX;
    assert(op_add_claim(&in, OP_IMAGE, OP_IMAGE_IDENTITY, 1, 1, 1) == OP_LIMIT);
    in.nr_claims = SIZE_MAX;
    assert(op_add_claim(&in, OP_IMAGE, OP_IMAGE_IDENTITY, 1, 1, 1) == OP_LIMIT);
    refused(OP_LIMIT);
    fixture(STYLEOS_ARCH_AARCH64); in.claims[0].pa = UINT64_MAX; refused(OP_OVERFLOW);
    fixture(STYLEOS_ARCH_AARCH64); in.claims[0].va = UINT64_MAX; refused(OP_OVERFLOW);
    fixture(STYLEOS_ARCH_AARCH64);
    in.claims[0].pa = UINT64_MAX - 2; in.claims[0].va = 1; in.claims[0].bytes = 1;
    refused(OP_OVERFLOW); /* Extent fits, outward page rounding does not. */
    fixture(STYLEOS_ARCH_X86_64);
    in.claims[8].pa = (UINT64_C(1) << in.snapshot.pa_bits) - OP_PAGE / 2;
    refused(OP_RANGE); /* Complete PA extent, not merely its start. */
    fixture(STYLEOS_ARCH_X86_64);
    reserve(0x500000, (OP_TABLES_MAX - 3) * OP_PAGE);
    assert(bm_normalize(&map) == BM_OK);
    for (size_t n = 4; n < OP_TABLES_MAX; ++n) {
        uint64_t page = 0x500000 + (n - 4) * OP_PAGE;
        claim(OP_OLD_TABLE, OP_OLD_WALK, page, page, OP_PAGE);
        identity_walk(page);
    }
    assert(in.nr_walks == OP_WALKS_MAX);
    assert(op_build(&in, &out, &scratch) == OP_OK && !out.granted);
    claim(OP_OLD_TABLE, OP_OLD_WALK, 0x53c000, 0x53c000, OP_PAGE);
    refused(OP_LIMIT);
    assert(op_build(0, &out, &scratch) == OP_ARGUMENT && !out.valid && !out.granted);
    assert(op_build(&in, 0, &scratch) == OP_ARGUMENT);
    assert(op_build(&in, &out, 0) == OP_ARGUMENT && !out.valid && !out.nr_review);
}

int main(void)
{
    positive();
    nested_and_adjacent();
    malformed_map();
    mandatory_and_proofs();
    invalid_context();
    unsupported_platform();
    descriptor_evidence();
    arithmetic_and_limits();
    assert(!strcmp(op_error_name(OP_REQUIRED), "required"));
    assert(!strcmp(op_error_name((enum op_error)99), "unknown"));
    assert(!strcmp(op_kind_name(OP_SOURCE_BUFFER), "SOURCE_BUFFER"));
    assert(!strcmp(op_kind_name(99), "unknown"));
    assert(!strcmp(op_proof_name(OP_OLD_WALK), "OLD_WALK"));
    assert(!strcmp(op_proof_name(99), "unknown"));
    puts("Ownership plan: complete evidence, interval union, refusals, REVIEW and zero GRANTED PASS");
    return 0;
}
