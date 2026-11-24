#include <styleos/iommu_guest_837.h>
#include <styleos/arch.h>
#ifdef STYLEOS_TEST_GUEST_OBSERVATION_840
int s840_observe_guest(const struct boot_info *,const struct op_input *,const struct op_output *,struct boot_range);
#endif
static struct s836_work work837;
static struct op_output result837;
static struct op_snapshot after837;
static int published;
static void hex(uint64_t v){arch_puts("0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(v>>i)&15u]);}
static void field(const char *s,uint64_t v){arch_puts(s);hex(v);}
static void range(const char *s,struct boot_range r){arch_puts("STYLEOS S837 ");arch_puts(s);field(" start=",r.start);field(" end=",r.end);arch_putc('\n');}
int s837_guest_compose(const struct boot_info *boot,const struct s835_report *q){
 const struct op_state *p=&styleos_ownership_workspace.state;
 const struct op_input *in=&p->input;
 if(published||!boot||!q||in->source!=OP_UEFI||in->arch!=STYLEOS_ARCH_X86_64||
    in->live.handoff.start!=(uintptr_t)boot||in->live.source.start!=boot->memory_map||
    in->live.source.end-in->live.source.start!=boot->memory_map_size||
    in->source_stride!=boot->descriptor_size||in->source_version!=boot->descriptor_version||
    !op_arch_unchanged(in,&after837))return 0;
 if(!s836_extend(&p->output,q,&result837,&work837)||!op_arch_unchanged(in,&after837))return 0;
 published=1;
 for(size_t i=0;i<result837.nr_protected;i++)range("PROTECTED",result837.protected_ranges[i]);
 for(size_t i=0;i<result837.nr_review;i++)range("REVIEW",result837.review[i]);
 arch_puts("STYLEOS S837 TOTAL");field(" protected=",result837.protected_pages);field(" review=",result837.review_pages);field(" granted=",result837.granted);arch_putc('\n');
 arch_puts("STYLEOS S837 POLICY detached=yes allocator-integrated=no\n");
#ifdef STYLEOS_TEST_GUEST_OBSERVATION_840
 if(!s840_observe_guest(boot,in,&result837,q->cover))return 0;
#endif
 return 1;
}
