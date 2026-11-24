#include <styleos/user_preempt_848.h>
#define IF UINT64_C(0x200)
#define P UINT64_C(4096)
#define USER_FLAGS UINT64_C(0xad7)
static bool region(const void *p,size_t n,size_t a){return p&&!((uintptr_t)p%a)&&n<=UINTPTR_MAX-(uintptr_t)p;}
static bool disjoint(const void *a,size_t n,const void *b,size_t m){return (uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a;}
static bool raw_ok(const struct s847_raw *f){return (f->rflags&(IF|2))==(IF|2)&&!(f->rflags&~USER_FLAGS);}
bool s848_user_dispatch(struct s847_control *c,uint64_t root,unsigned *id,struct up_context *f){
 if(!s847_dispatch(c,root,id,f))return false;
 f->status|=IF;return true;
}
bool s848_timer_suspend(struct s847_control *c,const struct s847_raw *f,const struct s848_irq_evidence *e){
 if(!region(c,sizeof *c,_Alignof(struct s847_control))||!region(f,sizeof *f,_Alignof(struct s847_raw))||!region(e,sizeof *e,_Alignof(struct s848_irq_evidence))||!disjoint(c,sizeof *c,f,sizeof *f)||!disjoint(c,sizeof *c,e,sizeof *e)||!disjoint(f,sizeof *f,e,sizeof *e))return false;
 if(!raw_ok(f)||f->vector!=0x50||f->error||e->live_flags&IF||!(e->isr&0x10000u)||e->frame_va!=S846_IST+P-16-sizeof *f)return false;
 struct up_context ctx={.sp=f->rsp,.pc=f->rip,.cs=f->cs,.ss=f->ss,.status=f->rflags&~IF};
 for(unsigned i=0;i<15;i++)ctx.gpr[i]=f->gpr[i];
 return s848_suspend_tick(c,&ctx,e->root);
}
bool s848_user_trap(struct s847_control *c,struct s847_raw *f,uint64_t va,uint64_t cr2,uint64_t root,uint64_t live_flags,struct s847_decision *out){
 if(!region(c,sizeof *c,_Alignof(struct s847_control))||!region(f,sizeof *f,_Alignof(struct s847_raw))||!region(out,sizeof *out,_Alignof(struct s847_decision))||!disjoint(c,sizeof *c,f,sizeof *f)||!disjoint(c,sizeof *c,out,sizeof *out)||!disjoint(f,sizeof *f,out,sizeof *out)||!raw_ok(f)||(live_flags&IF))return false;
 struct s847_raw normalized=*f;normalized.rflags&=~IF;
 struct s847_decision d;
 if(!s847_apply_raw(c,&normalized,va,cr2,root,&d))return false;
 if(d.action==S847_RETURN)f->gpr[0]=normalized.gpr[0];
 *out=d;return true;
}
