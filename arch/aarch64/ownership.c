#include "styleos/ownership.h"

extern const unsigned char aarch64_vectors[];

static enum op_error read_snapshot(const struct op_input *in, struct op_snapshot *out)
{
    uint64_t el, spsel, sp, mmfr0, sctlr, vbar, daif;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
    __asm__ volatile("mrs %0, SPSel" : "=r"(spsel));
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    __asm__ volatile("mrs %0, vbar_el1" : "=r"(vbar));
    __asm__ volatile("mrs %0, DAIF" : "=r"(daif));
    static const unsigned widths[]={32,36,40,42,44,48};
    unsigned index=(unsigned)(mmfr0&15u);
    if (index>=sizeof widths/sizeof widths[0] || el!=4 || spsel!=1 ||
        (sp&15u) || sp<in->live.stack.start || sp>in->live.stack.end-16u ||
        (daif&UINT64_C(0x3c0))!=UINT64_C(0x3c0) ||
        (sctlr&((UINT64_C(1)<<25)|(UINT64_C(1)<<24)|(UINT64_C(1)<<12)|5u)))
        return OP_PLATFORM;
    if (in->live.image.start!=UINT64_C(0x40080000) ||
        in->live.image.end>(UINT64_C(1)<<widths[index]) ||
        vbar!=(uint64_t)(uintptr_t)aarch64_vectors || (vbar&2047u) ||
        vbar<in->live.image.start || vbar>in->live.image.end-2048u)
        return OP_CONTEXT;
    *out=(struct op_snapshot){.mode=sctlr,.root=0,.aux0=vbar,.aux1=daif,
                             .pa_bits=widths[index],.phase=OP_MMU_OFF};
    return OP_OK;
}

enum op_error op_arch_collect(struct op_input *in, const struct boot_info *info)
{
    if (!in || !info || in->arch!=STYLEOS_ARCH_AARCH64 || in->source!=OP_FDT ||
        info->arch!=STYLEOS_ARCH_AARCH64 || info->platform!=STYLEOS_PLATFORM_QEMU_VIRT ||
        info->flags!=STYLEOS_BOOT_HAS_DTB || info->dtb!=in->live.source.start ||
        info->uart_base!=UINT64_C(0x9000000) || info->boot_el!=1)
        return OP_PLATFORM;
    enum op_error e=read_snapshot(in,&in->snapshot);
    if (e) return e;
    in->unresolved=0;
    /* MMU-off fixed Image profile, not a generic pointer-to-PA translation.
     * DTB bounds/reservations came from the existing successful parser. No
     * candidate RAM or MMIO is read here. PL011 is only classified by range. */
    e=op_add_claim(in,OP_VECTORS,OP_IMAGE_IDENTITY,
                   in->snapshot.aux0,in->snapshot.aux0,2048u);
    if (e) return e;
    return op_add_claim(in,OP_MMIO,OP_FIXED_DEVICE,
                        UINT64_C(0x9000000),UINT64_C(0x9000000),OP_PAGE);
}

int op_arch_unchanged(const struct op_input *in, struct op_snapshot *after)
{
    if (!in || !after || read_snapshot(in,after)!=OP_OK) return 0;
    return after->mode==in->snapshot.mode && after->aux0==in->snapshot.aux0 &&
           after->aux1==in->snapshot.aux1 && after->pa_bits==in->snapshot.pa_bits;
}
