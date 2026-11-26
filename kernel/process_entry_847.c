#include <styleos/process_entry_847.h>
#define P UINT64_C(4096)
#define LOW (UINT64_C(1)<<47)
static bool region(const void *p,size_t n,size_t a){return p&&!((uintptr_t)p%a)&&n<=UINTPTR_MAX-(uintptr_t)p;}
static bool separate(const void *a,size_t n,const void *b,size_t m){return (uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a;}
static uint64_t get64(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(v>>(8*i));}
bool s847_gdt_append(uint8_t *g,size_t len,uint16_t limit,uint16_t tr,uint16_t cs,struct s847_selectors *out){
 unsigned end=(unsigned)limit+1;
 if(len!=P||!region(g,len,P)||!region(out,sizeof *out,_Alignof(struct s847_selectors))||!separate(g,len,out,sizeof *out)||!end||(end&7)||end>P-16||!tr||(tr&7)||!cs||(cs&7)||(unsigned)tr+16!=end||(unsigned)cs+8>tr)return false;
 uint64_t code=get64(g+cs),tss=get64(g+tr),high=get64(g+tr+8);
 uint64_t tssbase=((tss>>16)&0xffffff)|((tss>>56)<<24)|(high<<32);
 if(((code>>40)&0xfe)!=0x9a||!(code&(UINT64_C(1)<<53))||(code&(UINT64_C(1)<<54))||((tss>>40)&255)!=0x8b||((tss>>52)&15)||((tss>>48)&15)||(tss&0xffff)!=103||(high>>16)||!tssbase||tssbase>=LOW-P||(tssbase&(P-1))||get64(g+end)||get64(g+end+8))return false;
 /* Accessed bit preset: descriptor reads need not modify GDT. Flat long-mode RX code / writable data. */
 put64(g+end,UINT64_C(0x00affa000000ffff)|UINT64_C(0x0000010000000000));
 put64(g+end+8,UINT64_C(0x00cff2000000ffff)|UINT64_C(0x0000010000000000));
 *out=(struct s847_selectors){(uint16_t)(end|3),(uint16_t)((end+8)|3),(uint16_t)(end+15)};return true;
}
bool s847_decode(const struct s847_raw *f,uint64_t va,uint64_t cr2,uint64_t root,uint16_t cs,uint16_t ss,struct up_context *out,struct up_event *ev){
 if(!region(f,sizeof *f,_Alignof(struct s847_raw))||!region(out,sizeof *out,_Alignof(struct up_context))||!region(ev,sizeof *ev,_Alignof(struct up_event))||!separate(f,sizeof *f,out,sizeof *out)||!separate(f,sizeof *f,ev,sizeof *ev)||!separate(out,sizeof *out,ev,sizeof *ev)||!root||root>=LOW||(root&(P-1))||(cs&7)!=3||(ss&7)!=3||cs<11||ss<11||cs==ss||f->cs!=cs||f->ss!=ss||!(f->rflags&2)||(f->rflags&~UINT64_C(0x8d7))||f->rip<S846_CODE||f->rip>=S846_CODE+P||f->rsp<S846_STACK||f->rsp>S846_STACK+P)return false;
 uint64_t ist;
 if(f->vector==128){if(f->error)return false;ist=S846_IST;}
 else if(f->vector==14){if(!(f->error&4)||(f->error&~UINT64_C(7))||cr2>=LOW)return false;ist=S846_IST+3*P;}
#if (defined(STYLEOS_TEST_FOUR_GUEST_864) || defined(STYLEOS_OWNED_SHELL_865))
 /* Only zero-error-code GP and UD in the currently admitted user ABI.
  * CR2 is meaningful only for PF; the trusted adapter supplies zero
  * for the other vectors. Both exceptions use the guarded fault IST. */
 else if(f->vector==6||f->vector==13){if(f->error||cr2)return false;ist=S846_IST+3*P;}
#endif
 else return false;
 if(va!=ist+P-16-sizeof *f)return false;
 struct up_context c={.sp=f->rsp,.pc=f->rip,.status=f->rflags,.cs=f->cs,.ss=f->ss};
 for(unsigned i=0;i<15;i++)c.gpr[i]=f->gpr[i];
 struct up_event e={.kind=f->vector!=128?UP_FAULT:UP_EVENT_NONE,.vector=f->vector,.syndrome=f->error,.fault_va=f->vector==14?cr2:0,.active_root=root,.kernel_sp=va};
 *out=c;*ev=e;return true;
}

#if defined(STYLEOS_INTEGRATED_RUNTIME_864) || defined(STYLEOS_TEST_RUNTIME_ENTRY_CLASSIFIER_865)
enum rt865_frame_kind rt865_classify_frame(const struct s847_raw *f,uint64_t va,
 uint64_t cr2,uint64_t child,uint64_t safe,uint16_t cs,uint16_t ss,bool armed)
{
 if(!region(f,sizeof *f,_Alignof(struct s847_raw))||
    !child||child>=LOW||(child&(P-1))||!safe||safe>=LOW||(safe&(P-1))||child==safe||
    (cs&7)!=3||(ss&7)!=3||cs<11||ss<11||cs==ss||
    f->cs!=cs||f->ss!=ss||(f->rflags&0x202)!=0x202||(f->rflags&~UINT64_C(0xad7))||
    f->rip<S846_CODE||f->rip>=S846_CODE+P||f->rsp<=S846_STACK||f->rsp>S846_STACK+P||
    (f->vector!=14&&cr2))return RT865_INVALID;
 if(f->vector==0x50)
    return armed&&!f->error&&va==S846_IST+P-16-sizeof *f?RT865_TICK:RT865_INVALID;
 /* The existing restricted adapter masks the timer for call/fault slices.
  * Asynchronous syscall-while-timer-armed requires separate IRQ retirement. */
 if(armed)return RT865_INVALID;
 struct s847_raw n=*f;n.rflags&=~UINT64_C(0x200);
 struct up_context c;struct up_event e;
 if(!s847_decode(&n,va,cr2,child,cs,ss,&c,&e))return RT865_INVALID;
 return e.kind==UP_FAULT||f->gpr[0]==S847_CALL_EXIT?RT865_TERMINAL:RT865_SYSCALL;
}
enum rt865_frame_kind rt865_classify_native_frame(const struct s847_raw *f,uint64_t va,
 uint64_t cr2,uint64_t child,uint64_t safe,uint16_t cs,uint16_t ss,bool armed)
{
 enum rt865_frame_kind kind=rt865_classify_frame(f,va,cr2,child,safe,cs,ss,armed);
 if(kind!=RT865_INVALID||!region(f,sizeof *f,_Alignof(struct s847_raw)))return kind;
#if defined(STYLEOS_OWNED_SHELL_865) || defined(STYLEOS_TEST_RUNTIME_ENTRY_CLASSIFIER_865)
 /* These additional CPU exceptions are terminal only. User RIP, RSP and
  * arithmetic/debug flags are not evidence of kernel corruption. Root,
  * selectors, IST, IF and vector-specific hardware error remain mandatory. */
 if(!armed&&child&&child<LOW&&!(child&(P-1))&&safe&&safe<LOW&&!(safe&(P-1))&&child!=safe&&
    (cs&7)==3&&(ss&7)==3&&cs>=11&&ss>=11&&cs!=ss&&f->cs==cs&&f->ss==ss&&
    (f->rflags&0x202)==0x202&&!(f->rflags&~UINT64_C(0x254fd7))&&
    va==S846_IST+4*P-16-sizeof *f){
  if(!cr2&&!f->error&&(f->vector==0||f->vector==1))return RT865_USER_FAULT;
  /* IDT-selector error from a denied software INT, not an external IRQ. */
  if(!cr2&&f->vector==13&&(f->error&7)==2&&f->error<=0x7fa)return RT865_USER_FAULT;
  /* Instruction-fetch PF (including NX). Never recover a reserved-bit or
   * supervisor fault by classifying it as an ordinary user exception. */
  if(f->vector==14&&(f->error&0x14)==0x14&&!(f->error&~UINT64_C(0x15)))return RT865_USER_FAULT;
 }
#endif
 if(f->rsp>S846_STACK&&f->rsp<=S846_STACK+P)return kind;
 /* Only RSP is user-controlled and out of range. Check every other field,
  * including exact IST, selectors, roots, flags and vector/error evidence.
  * The temporary copy is never published or used for IRET. */
 struct s847_raw copy=*f;copy.rsp=S846_STACK+P;
 return rt865_classify_frame(&copy,va,cr2,child,safe,cs,ss,armed)==RT865_INVALID?
    RT865_INVALID:RT865_USER_FAULT;
}
#endif

bool s847_apply_raw(struct s847_control *ctl,struct s847_raw *f,uint64_t va,uint64_t cr2,uint64_t root,struct s847_decision *out){
 if(!region(ctl,sizeof *ctl,_Alignof(struct s847_control))||!region(f,sizeof *f,_Alignof(struct s847_raw))||!region(out,sizeof *out,_Alignof(struct s847_decision))||!separate(ctl,sizeof *ctl,f,sizeof *f)||!separate(ctl,sizeof *ctl,out,sizeof *out)||!separate(f,sizeof *f,out,sizeof *out)||ctl->cs>UINT16_MAX||ctl->ss>UINT16_MAX)return false;
 struct up_context c;struct up_event e;struct s847_decision d;
 if(!s847_decode(f,va,cr2,root,(uint16_t)ctl->cs,(uint16_t)ctl->ss,&c,&e)||!s847_trap(ctl,&c,&e,&d))return false;
 if(d.action==S847_RETURN)f->gpr[0]=d.result;
 *out=d;return true;
}
