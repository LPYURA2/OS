#include <styleos/process_entry_847.h>
#include <styleos/user_copy_772.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) uint8_t gdt[4096],old[4096];
static unsigned n;
#define C(x) do{assert(x);n++;}while(0)
static void put(unsigned p,uint64_t v){for(unsigned i=0;i<8;i++)gdt[p+i]=(uint8_t)(v>>(8*i));}
static uint64_t get(unsigned p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)gdt[p+i]<<(8*i);return v;}
static void fixture(void){memset(gdt,0,sizeof gdt);put(0x38,UINT64_C(0x00af9b000000ffff));put(0x48,UINT64_C(0x00008b4000000067));put(0x50,0);}
static void refusal(uint16_t limit,uint16_t tr,uint16_t cs){struct s847_selectors v={1,2,3};memcpy(old,gdt,sizeof gdt);C(!s847_gdt_append(gdt,sizeof gdt,limit,tr,cs,&v));C(!memcmp(old,gdt,sizeof gdt));C(v.cs==1&&v.ss==2&&v.limit==3);}
static void reject(struct s847_raw f,uint64_t va,uint64_t cr2,uint64_t root){struct up_context c={.pc=8},saved=c;struct up_event e={.vector=99},before=e;C(!s847_decode(&f,va,cr2,root,0x5b,0x63,&c,&e));C(!memcmp(&c,&saved,sizeof c));C(!memcmp(&e,&before,sizeof e));}
int main(void){fixture();memcpy(old,gdt,sizeof gdt);struct s847_selectors s;C(s847_gdt_append(gdt,sizeof gdt,0x57,0x48,0x38,&s));C(s.cs==0x5b&&s.ss==0x63&&s.limit==0x67);C(!memcmp(old,gdt,0x58));C(get(0x58)==UINT64_C(0x00affb000000ffff));C(get(0x60)==UINT64_C(0x00cff3000000ffff));C(!memcmp(old+0x68,gdt+0x68,4096-0x68));refusal(0x57,0x48,0x38);fixture();refusal(0x56,0x48,0x38);refusal(0x57,0x40,0x38);refusal(0x57,0x48,0x3b);refusal(0xfff,0xff0,0x38);put(0x48,UINT64_C(0x0000894000000067));refusal(0x57,0x48,0x38);fixture();put(0x38,UINT64_C(0x00cf9b000000ffff));refusal(0x57,0x48,0x38);fixture();put(0x50,0x10000);refusal(0x57,0x48,0x38);
 const uint64_t va=S846_IST+4096-16-176,pfva=va+3*4096;struct s847_raw f={.vector=128,.rip=S846_CODE+32,.cs=0x5b,.rflags=2,.rsp=S846_STACK+4096,.ss=0x63};for(unsigned i=0;i<15;i++)f.gpr[i]=0x84700+i;
 struct up_context c;struct up_event e;C(s847_decode(&f,va,UINT64_MAX,0x3000,0x5b,0x63,&c,&e));C(c.pc==f.rip&&c.sp==f.rsp&&c.cs==f.cs&&c.ss==f.ss&&c.status==f.rflags);for(unsigned i=0;i<31;i++)C(c.gpr[i]==(i<15?f.gpr[i]:0));C(e.kind==UP_EVENT_NONE&&e.vector==128&&!e.fault_va&&e.active_root==0x3000&&e.kernel_sp==va);
 struct s847_raw bad=f;bad.cs=0x38;reject(bad,va,0,0x3000);bad=f;bad.ss=0x10;reject(bad,va,0,0x3000);bad=f;bad.error=1;reject(bad,va,0,0x3000);bad=f;bad.vector=13;reject(bad,va,0,0x3000);bad=f;bad.rflags|=0x200;reject(bad,va,0,0x3000);bad=f;bad.rip=S846_DATA;reject(bad,va,0,0x3000);bad=f;bad.rsp=S846_STACK-1;reject(bad,va,0,0x3000);reject(f,va+8,0,0x3000);reject(f,pfva,0,0x3000);reject(f,va,0,0x3001);
 f.vector=14;f.error=5;C(s847_decode(&f,pfva,0x4000,0x3000,0x5b,0x63,&c,&e));C(e.kind==UP_FAULT&&e.vector==14&&e.syndrome==5&&e.fault_va==0x4000);reject(f,va,0x4000,0x3000);reject(f,pfva,UINT64_MAX,0x3000);bad=f;bad.error=1;reject(bad,pfva,0x4000,0x3000);bad=f;bad.error=13;reject(bad,pfva,0x4000,0x3000);
 struct s847_control ctl={0};unsigned id;struct s847_decision d;C(s847_init(&ctl,0x1000,0x3000,0x4000,0x5b,0x63));C(s847_dispatch(&ctl,0x1000,&id,&c)&&id==0);C(s847_decode(&f,pfva,0x4000,0x3000,0x5b,0x63,&c,&e));C(s847_trap(&ctl,&c,&e,&d)&&d.action==S847_FAULT);C(s847_dispatch(&ctl,0x1000,&id,&c)&&id==1);
 struct s847_raw call=f;call.vector=128;call.error=0;call.gpr[0]=S847_CALL_RANGE;call.gpr[1]=S846_DATA;call.gpr[2]=8;
 struct s847_raw previous=call;struct s847_control saved=ctl;struct s847_decision prev=d;
 C(!s847_apply_raw(&ctl,&call,va,0,0x3000,&d));C(!memcmp(&call,&previous,sizeof call));C(!memcmp(&ctl,&saved,sizeof ctl));C(!memcmp(&d,&prev,sizeof d));
 C(s847_apply_raw(&ctl,&call,va,0,0x4000,&d)&&d.action==S847_RETURN&&d.result==UC772_OK);previous.gpr[0]=UC772_OK;C(!memcmp(&call,&previous,sizeof call));
 call.gpr[0]=S847_CALL_RANGE;call.gpr[1]=S846_CODE;previous=call;C(s847_apply_raw(&ctl,&call,va,0,0x4000,&d)&&d.action==S847_RETURN&&d.result==UC772_RANGE);previous.gpr[0]=UC772_RANGE;C(!memcmp(&call,&previous,sizeof call));
 call.gpr[0]=S847_CALL_EXIT;previous=call;C(s847_apply_raw(&ctl,&call,va,0,0x4000,&d)&&d.action==S847_EXIT);C(!memcmp(&call,&previous,sizeof call));C(!s847_dispatch(&ctl,0x1000,&id,&c));
 printf("S847 GDT and raw-frame checks: %u PASS\n",n);return 0;}
