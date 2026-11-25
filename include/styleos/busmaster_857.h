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
/* PIIX3 BMIDE command bit 3 is direction, not Start (bit 0).
 * Status bit 2 is an interrupt latch, not Active (bit 0) or Error (bit 1).
 * Only these harmless observed bits may remain set. Never clear a W1C
 * latch or write an engine register; PCI Bus Master must be disabled. */
static inline bool bm864_optical_byte(unsigned index,uint8_t value)
{
    return index<4 && !(value & (uint8_t)~((index&1u)?4u:8u));
}
static inline bool bm864_optical_idle(const struct bm857_snapshot *s,uint64_t root)
{
    if(!s||!root||(root&4095u)||s->root!=root||s->id!=UINT32_C(0x70108086)||
       s->class_revision!=UINT32_C(0x01018000)||s->bar4!=0x6061u)return false;
    for(unsigned i=0;i<4;i++)if(!bm864_optical_byte(i,s->bm[i]))return false;
    return true;
}
/* Optical firmware may leave I/O enabled without memory decoding, or
 * may already have disabled bus mastering. No other PCI command bits are
 * allowed; never enable any decoder or master. Require idle engines and
 * exactly the same status bits before and after. */
static inline bool bm864_optical_plan(const struct bm857_snapshot *s,uint64_t root,uint16_t *word)
{
    if(!word||!bm864_optical_idle(s,root)||
       (s->command_status&~UINT32_C(6))!=UINT32_C(0x02800001))return false;
    *word=(uint16_t)(s->command_status&3u);return true;
}
static inline bool bm864_optical_confirm(const struct bm857_snapshot *s,uint64_t root,uint16_t expected)
{
    return (expected==1||expected==3)&&bm864_optical_idle(s,root)&&
           s->command_status==(UINT32_C(0x02800000)|expected);
}
/* Preserve all pre-864 terminal contracts unchanged. */
static inline bool bm857_idle(const struct bm857_snapshot *s,uint64_t root)
{ return bm857_idle_at(s,root,UINT32_C(0x60e1)); }
static inline bool bm857_plan(const struct bm857_snapshot *s,uint64_t root,uint16_t *word)
{ return bm857_plan_at(s,root,UINT32_C(0x60e1),word); }
static inline bool bm857_confirm(const struct bm857_snapshot *s,uint64_t root)
{ return bm857_confirm_at(s,root,UINT32_C(0x60e1)); }
#endif
