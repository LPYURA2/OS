#include "styleos/boot_memory.h"

enum { FDT_BEGIN = 1, FDT_END_NODE = 2, FDT_PROP = 3, FDT_NOP = 4, FDT_END = 9 };
enum node_kind { ROOT, MEMORY_NAME, RESERVED_ROOT, RESERVED_CHILD, OTHER };
enum { MAX_DEPTH = 32 };
struct node {
    enum node_kind kind;
    const uint8_t *reg;
    uint32_t reg_len;
    unsigned ac, sc;
    int ac_seen, sc_seen, ranges_seen, children, memory, type_seen;
    int status_seen, disabled, dynamic;
};

static uint32_t be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8 | (uint32_t)p[3];
}

static uint64_t cells(const uint8_t *p, unsigned n)
{
    return n == 1 ? be32(p) : ((uint64_t)be32(p) << 32) | be32(p + 4);
}

static int eq(const uint8_t *p, const char *s)
{
    while (*s && *p == (uint8_t)*s) { ++p; ++s; }
    return !*p && !*s;
}

static int memory_name(const uint8_t *p)
{
    const char *s = "memory";
    while (*s && *p == (uint8_t)*s) { ++p; ++s; }
    return !*s && (!*p || *p == '@');
}

static int nul_before(const uint8_t *p, size_t size, size_t *length)
{
    for (size_t i = 0; i < size; ++i)
        if (!p[i]) { *length = i; return 1; }
    return 0;
}

static int supported_cells(const struct node *n)
{
    return n->ac >= 1 && n->ac <= 2 && n->sc >= 1 && n->sc <= 2;
}

static enum boot_memory_error property(struct node *n, const uint8_t *name,
                                       const uint8_t *value, uint32_t len)
{
    size_t text_len;
    if (eq(name, "reg")) {
        if (n->reg) return BM_FDT_FORMAT;
        n->reg = value;
        n->reg_len = len;
    } else if (eq(name, "#address-cells") || eq(name, "#size-cells")) {
        if (len != 4) return BM_FDT_FORMAT;
        int address = eq(name, "#address-cells");
        int *seen = address ? &n->ac_seen : &n->sc_seen;
        unsigned *count = address ? &n->ac : &n->sc;
        if (*seen) return BM_FDT_FORMAT;
        *seen = 1;
        *count = be32(value);
    } else if (eq(name, "device_type")) {
        if (n->type_seen || !nul_before(value, len, &text_len) ||
            text_len + 1 != len) return BM_FDT_FORMAT;
        n->type_seen = 1;
        n->memory = eq(value, "memory");
    } else if (eq(name, "status")) {
        if (n->status_seen || !nul_before(value, len, &text_len) ||
            text_len + 1 != len) return BM_FDT_FORMAT;
        n->status_seen = 1;
        n->disabled = !(eq(value, "okay") || eq(value, "ok"));
    } else if (n->kind == RESERVED_ROOT && eq(name, "ranges")) {
        if (n->ranges_seen || len) return BM_FDT_UNSUPPORTED;
        n->ranges_seen = 1;
    } else if (n->kind == RESERVED_CHILD && eq(name, "size")) {
        n->dynamic = 1;
    }
    return BM_OK;
}

static enum boot_memory_error add_reg(struct boot_memory_map *m,
                                     const struct node *n,
                                     const struct node *parent, int reserved)
{
    if (!supported_cells(parent)) return BM_FDT_UNSUPPORTED;
    uint32_t stride = 4u * (parent->ac + parent->sc);
    if (!n->reg || !n->reg_len || n->reg_len % stride) return BM_FDT_FORMAT;
    for (uint32_t i = 0; i < n->reg_len; i += stride) {
        const uint8_t *p = n->reg + i;
        enum boot_memory_error e = bm_add(m, cells(p, parent->ac),
                                          cells(p + 4 * parent->ac, parent->sc),
                                          reserved);
        if (e) return e;
    }
    return BM_OK;
}

static enum boot_memory_error structure(struct boot_memory_map *m,
                                       const uint8_t *p, size_t size,
                                       const uint8_t *strings, size_t str_size)
{
    struct node stack[MAX_DEPTH];
    unsigned depth = 0;
    int root_seen = 0, reservations_seen = 0;
    size_t at = 0;
    while (size - at >= 4) {
        uint32_t token = be32(p + at);
        at += 4;
        if (token == FDT_NOP) continue;
        if (token == FDT_BEGIN) {
            size_t len;
            if (!nul_before(p + at, size - at, &len)) return BM_FDT_FORMAT;
            size_t padded = (len + 4u) & ~(size_t)3;
            if (padded > size - at) return BM_FDT_FORMAT;
            if (depth == MAX_DEPTH) return BM_LIMIT;
            enum node_kind kind = OTHER;
            if (!depth) {
                if (root_seen || len) return BM_FDT_FORMAT;
                kind = ROOT;
                root_seen = 1;
            } else {
                if (!len) return BM_FDT_FORMAT;
                struct node *parent = &stack[depth - 1];
                parent->children = 1;
                if (depth == 1 && eq(p + at, "reserved-memory")) {
                    if (reservations_seen) return BM_FDT_FORMAT;
                    reservations_seen = 1;
                    kind = RESERVED_ROOT;
                } else if (depth == 1 && memory_name(p + at)) {
                    kind = MEMORY_NAME;
                } else if (parent->kind == RESERVED_ROOT) {
                    if (!parent->ranges_seen || !supported_cells(parent) ||
                        !parent->ac_seen || !parent->sc_seen ||
                        parent->ac != stack[0].ac || parent->sc != stack[0].sc)
                        return BM_FDT_UNSUPPORTED;
                    kind = RESERVED_CHILD;
                } else if (parent->kind == RESERVED_CHILD) {
                    return BM_FDT_UNSUPPORTED;
                }
            }
            stack[depth++] = (struct node){.kind = kind, .ac = 2, .sc = 1};
            at += padded;
        } else if (token == FDT_PROP) {
            if (!depth || stack[depth - 1].children || size - at < 8)
                return BM_FDT_FORMAT;
            uint32_t len = be32(p + at), nameoff = be32(p + at + 4);
            at += 8;
            size_t name_len;
            if (len > size - at || nameoff >= str_size ||
                !nul_before(strings + nameoff, str_size - nameoff, &name_len) ||
                !name_len) return BM_FDT_FORMAT;
            size_t padded = ((size_t)len + 3) & ~(size_t)3;
            if (padded > size - at) return BM_FDT_FORMAT;
            enum boot_memory_error e = property(&stack[depth - 1], strings + nameoff,
                                                p + at, len);
            if (e) return e;
            at += padded;
        } else if (token == FDT_END_NODE) {
            if (!depth) return BM_FDT_FORMAT;
            const struct node *n = &stack[--depth];
            if (n->kind == RESERVED_ROOT && (!n->ranges_seen || !n->ac_seen ||
                !n->sc_seen || !supported_cells(n) || n->ac != stack[0].ac ||
                n->sc != stack[0].sc)) return BM_FDT_UNSUPPORTED;
            if (n->kind == RESERVED_CHILD) {
                if (!n->reg) return n->dynamic ? BM_FDT_UNSUPPORTED : BM_FDT_FORMAT;
                enum boot_memory_error e = add_reg(m, n, &stack[depth - 1], 1);
                if (e) return e;
            } else if (n->kind == MEMORY_NAME || n->memory) {
                if (depth != 1 || !n->memory || n->children)
                    return BM_FDT_UNSUPPORTED;
                if (!n->disabled) {
                    enum boot_memory_error e = add_reg(m, n, &stack[0], 0);
                    if (e) return e;
                }
            }
        } else if (token == FDT_END) {
            return root_seen && !depth && at == size ? BM_OK : BM_FDT_FORMAT;
        } else {
            return BM_FDT_FORMAT;
        }
    }
    return BM_FDT_FORMAT;
}

enum boot_memory_error bm_fdt_parse(struct boot_memory_map *m, const void *data,
                                    size_t available, uint32_t *total_size)
{
    const uint8_t *b = data;
    if (!b || available < 40 || be32(b) != UINT32_C(0xd00dfeed))
        return BM_FDT_FORMAT;
    uint32_t total = be32(b + 4), st = be32(b + 8), str = be32(b + 12);
    uint32_t res = be32(b + 16), ver = be32(b + 20), compat = be32(b + 24);
    uint32_t str_size = be32(b + 32), st_size = be32(b + 36);
    if (total < 40 || total > available || total > BOOT_DTB_MAX ||
        ver < 17 || compat > 17 || st < 40 || str < 40 || res < 40 ||
        st >= total || str >= total || res >= total || (st & 3) || (res & 7) ||
        st_size < 4 || (st_size & 3) || st_size > total - st ||
        !str_size || str_size > total - str ||
        (st < str + str_size && str < st + st_size) ||
        (res >= st && res < st + st_size) ||
        (res >= str && res < str + str_size))
        return BM_FDT_FORMAT;

    uint32_t end = total;
    if (st > res && st < end) end = st;
    if (str > res && str < end) end = str;
    int terminated = 0;
    /* A DTSpec reservation block must not contradict itself. Cross-source
     * overlaps (e.g. reserve map plus /reserved-memory) are normal. */
    size_t first = m->nr_reserved;
    for (uint32_t at = res; end - at >= 16; at += 16) {
        uint64_t address = cells(b + at, 2), size = cells(b + at + 8, 2);
        if (!address && !size) { terminated = 1; break; }
        enum boot_memory_error e = bm_add(m, address, size, 1);
        if (e) return e;
        struct boot_range r = m->reserved[m->nr_reserved - 1];
        for (size_t j = first; j + 1 < m->nr_reserved; ++j)
            if (r.start < m->reserved[j].end && m->reserved[j].start < r.end)
                return BM_CONFLICT;
    }
    if (!terminated) return BM_FDT_FORMAT;
    enum boot_memory_error e = structure(m, b + st, st_size, b + str, str_size);
    if (e) return e;
    *total_size = total;
    return BM_OK;
}
