#ifndef STYLEOS_BUSMASTER_857_H
#define STYLEOS_BUSMASTER_857_H
#include <stdbool.h>
#include <stdint.h>
/* Only the observed terminal PIIX3 fixture, not a general DMA grant. */
struct bm857_snapshot {
    uint64_t root;
    uint32_t id, class_revision, command_status, bar4;
    uint8_t bm[4];
};
static inline bool bm857_idle(const struct bm857_snapshot *s, uint64_t root)
{
    return s && root && !(root & 4095u) && s->root == root &&
           s->id == UINT32_C(0x70108086) && s->class_revision == UINT32_C(0x01018000) &&
           s->bar4 == UINT32_C(0x60e1) && !s->bm[0] && !s->bm[1] && !s->bm[2] && !s->bm[3];
}
static inline bool bm857_plan(const struct bm857_snapshot *s, uint64_t root, uint16_t *word)
{
    if (!word || !bm857_idle(s,root) || s->command_status != UINT32_C(0x02800007)) return false;
    *word = 3u; /* clear bit 2 only; preserve I/O and memory bits; no Status write */
    return true;
}
static inline bool bm857_confirm(const struct bm857_snapshot *s, uint64_t root)
{
    return bm857_idle(s,root) && s->command_status == UINT32_C(0x02800003);
}
#endif
