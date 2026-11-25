#include <styleos/runtime_entry_858.h>
#include <styleos/desktop_process_runtime.h>
#include <styleos/user_session_runtime.h>
#include <styleos/native_window_channel.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define B(n) (UINT64_C(1)<<(n))
int main(int argc,char **argv){
 struct rt858_expected e={0x1000,0x2000,0x3000,0x4000,0x5390,0x15390,0x57,0x48,0x38};
 struct rt858_live v={0x2000,0x3000,0x4000,0x14000,B(31)|B(16)|1,B(5),B(10)|B(11),2,0x57,4095,0x48,0x38};
 unsigned denied=0;
 assert(rt858_validate(&e,&v));
 #define REJECT(expr) do{assert(!(expr));denied++;}while(0)
 #define EV(field,value) do{struct rt858_expected x=e;x.field=(value);REJECT(rt858_validate(&x,&v));}while(0)
 #define LV(field,value) do{struct rt858_live x=v;x.field=(value);REJECT(rt858_validate(&e,&x));}while(0)
 for(unsigned i=0;i<64;i++){LV(root,v.root^B(i));LV(gdt,v.gdt^B(i));LV(idt,v.idt^B(i));EV(root,e.root^B(i));}
 for(unsigned i=0;i<16;i++){LV(gdt_limit,v.gdt_limit^(1u<<i));LV(idt_limit,v.idt_limit^(1u<<i));LV(tr,v.tr^(1u<<i));LV(cs,v.cs^(1u<<i));}
 EV(old_root,e.root);EV(old_root,0);EV(old_root,1);EV(old_root,B(47));EV(stack_lo,0);EV(stack_lo,0x5391);EV(stack_hi,e.stack_lo);EV(stack_hi,0x15391);EV(stack_hi,B(47));EV(gdt,e.root);EV(idt,e.gdt);EV(cs,0);EV(tr,0x38);EV(gdt_limit,0x58);
 LV(rsp,e.stack_lo-1);LV(rsp,e.stack_hi);LV(rsp,0);LV(cr0,v.cr0^B(31));LV(cr0,v.cr0^B(16));LV(cr0,v.cr0^B(0));LV(cr4,0);LV(cr4,v.cr4|B(7));LV(cr4,v.cr4|B(12));LV(cr4,v.cr4|B(17));LV(efer,v.efer^B(10));LV(efer,v.efer^B(11));LV(rflags,0);LV(rflags,2|B(9));LV(rflags,2|B(14));LV(rflags,2|B(17));REJECT(rt858_validate(0,&v));REJECT(rt858_validate(&e,0));
 struct rt858_live bad=v;bad.root=e.old_root;REJECT(rt858_start(&e,&bad));assert(!dpr_global_state()&&!usr_global()&&!nwc_global());
 if(argc==2&&!strcmp(argv[1],"dirty")){assert(dpr_global_start(e.old_root)==DPR_OK);REJECT(rt858_start(&e,&v));REJECT(rt858_start(&e,&v));assert(!rt858_ready(e.root)&&dpr_global_state()->dispatcher.safe_root==e.old_root&&!usr_global()&&!nwc_global());puts("dirty start refused; existing service unchanged");return 0;}
 assert(rt858_start(&e,&v)&&rt858_ready(e.root));assert(dpr_global_state()->dispatcher.safe_root==e.root);assert(!pr_snapshot(pr_global(),0,0,true)&&!ws_snapshot(ws_global(),0,0)&&!nwc_port_count(nwc_global()));REJECT(rt858_start(&e,&v));REJECT(rt858_ready(e.old_root));assert(rt858_ready(e.root));printf("runtime handoff: 2 positive, %u refusals PASS; real service singletons, host mode only\n",denied);
}
