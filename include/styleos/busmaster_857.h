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
/* Only two independently observed Q35/PIIX3 port windows, selected by
 * the trusted boot fixture. Never derive an unrestricted port from PCI. */
static inline bool bm857_known_bar(uint32_t bar)
{ return bar == UINT32_C(0x60e1) || bar == UINT32_C(0x6061); }
static inline bool bm857_idle_at(const struct bm857_snapshot *s, uint64_t root, uint32_t bar)
{
    return s && root && !(root & 4095u) && s->root == root &&
           s->id == UINT32_C(0x70108086) && s->class_revision == UINT32_C(0x01018000) &&
           bm857_known_bar(bar) && s->bar4 == bar && !s->bm[0] && !s->bm[1] && !s->bm[2] && !s->bm[3];
}
static inline bool bm857_plan_at(const struct bm857_snapshot *s, uint64_t root, uint32_t bar, uint16_t *word)
{
    if (!word || !bm857_idle_at(s,root,bar) || s->command_status != UINT32_C(0x02800007)) return false;
    *word = 3u; /* clear bit 2 only; preserve I/O and memory bits; no Status write */
    return true;
}
static inline bool bm857_confirm_at(const struct bm857_snapshot *s, uint64_t root, uint32_t bar)
{
    return bm857_idle_at(s,root,bar) && s->command_status == UINT32_C(0x02800003);
}
/* Preserve all pre-864 terminal contracts unchanged. */
static inline bool bm857_idle(const struct bm857_snapshot *s,uint64_t root)
{ return bm857_idle_at(s,root,UINT32_C(0x60e1)); }
static inline bool bm857_plan(const struct bm857_snapshot *s,uint64_t root,uint16_t *word)
{ return bm857_plan_at(s,root,UINT32_C(0x60e1),word); }
static inline bool bm857_confirm(const struct bm857_snapshot *s,uint64_t root)
{ return bm857_confirm_at(s,root,UINT32_C(0x60e1)); }
#endif
