#include <styleos/task_stacks_845.h>
#define PAGE UINT64_C(4096)
#define LOW (UINT64_C(1)<<47)
_Static_assert(TASKS_MAX==2 && TASKS_STACK_BYTES==4*PAGE,"two four-page task stacks");
static bool overlap(struct boot_range a,struct boot_range b){return a.start<b.end&&b.start<a.end;}
static bool within(struct boot_range a,struct boot_range b){return a.start<a.end&&b.start<=a.start&&a.end<=b.end;}
static bool object(const void *p,size_t n,size_t align,struct boot_range *out){
 uint64_t x=(uintptr_t)p;if(!x||x%align||n>UINT64_MAX-x)return false;
 *out=(struct boot_range){x,x+n};return true;
}
static bool separate(const struct boot_range *r,unsigned n){
 for(unsigned i=0;i<n;i++)for(unsigned j=0;j<i;j++)if(overlap(r[i],r[j]))return false;
 return true;
}
static uint32_t owner(unsigned i){return i?S845_OWNER_B:S845_OWNER_A;}
static bool metadata(struct s844_arena *a,struct s845_pair *p,struct s843_scheduler *s,
 struct boot_range ist,struct boot_range r[5]){
 if(!object(a,sizeof *a,_Alignof(struct s844_arena),&r[0])||
 !object(p,sizeof *p,_Alignof(struct s845_pair),&r[1])||
 !object(s,sizeof *s,_Alignof(struct s843_scheduler),&r[2])||
 !a->base||a->base>LOW-S844_BYTES||a->base%PAGE||
 !ist.start||ist.start>=ist.end||ist.end>LOW||ist.end-ist.start!=PAGE||ist.start%PAGE)return false;
 r[3]=(struct boot_range){a->base,a->base+S844_BYTES};r[4]=ist;
 return separate(r,5);
}
bool s845_prepare(struct s844_arena *a,struct s845_pair *p,struct s843_scheduler *s,
 const struct s845_code code[2],struct boot_range ist,uint64_t root,uint64_t cs,uint64_t ss){
 struct boot_range r[8];unsigned used;
 if(!metadata(a,p,s,ist,r)||!object(code,2*sizeof *code,_Alignof(struct s845_code),&r[5])||
 p->live||s->ready||!s844_count(a,&used)||used||a->next>UINT64_MAX-2)return false;
 for(unsigned i=0;i<2;i++){
  r[6+i]=code[i].span;
  if(!r[6+i].start||r[6+i].start>=r[6+i].end||r[6+i].end>LOW||
     code[i].entry<r[6+i].start||code[i].entry>=r[6+i].end)return false;
 }
 if(!separate(r,8)||!root||root%PAGE||root>=LOW||!cs||cs>0xffff||(cs&7)||ss>0xffff||(ss&7))return false;
 struct s845_pair next={0};struct s843_task task[2];
 if(!s844_acquire(a,owner(0),4,&next.lease[0]))return false;
 if(!s844_acquire(a,owner(1),4,&next.lease[1])){
  (void)s844_release(a,owner(0),next.lease[0]);return false;
 }
 for(unsigned i=0;i<2;i++)task[i]=(struct s843_task){code[i].span,
   {next.lease[i].pa,next.lease[i].pa+TASKS_STACK_BYTES},code[i].entry};
 if(!s843_init(s,task,ist,root,cs,ss)){
  (void)s844_release(a,owner(1),next.lease[1]);
  (void)s844_release(a,owner(0),next.lease[0]);return false;
 }
 next.live=true;*p=next;return true;
}
static bool same(struct s844_lease a,struct s844_lease b){return a.pa==b.pa&&a.owner==b.owner&&a.generation==b.generation&&a.pages==b.pages;}
bool s845_release_terminal(struct s844_arena *a,struct s845_pair *p,struct s843_scheduler *s,
 struct s843_frame *f,uint64_t rsp,uint64_t root,uint64_t flags){
 struct boot_range r[5],fr;unsigned used;
 if(!s||((uintptr_t)s%_Alignof(struct s843_scheduler))||!metadata(a,p,s,s->ist,r)||
 !p->live||!s->ready||!root||root!=s->root||(flags&0x200)||!(flags&2)||
 !object(f,sizeof *f,8,&fr)||!within(fr,s->ist)||rsp<s->ist.start+256||rsp>=fr.start||
 s->ticks!=16||s->control.current!=0||s->control.dispatches!=17||
 !s844_count(a,&used)||used!=8)return false;
 for(unsigned i=0;i<2;i++){
  const struct s844_lease v=p->lease[i];
  if(v.pa<a->base||v.pa>a->base+S844_BYTES-TASKS_STACK_BYTES||v.pa%PAGE||
  v.owner!=owner(i)||v.pages!=4||!v.generation||!same(a->slots[(v.pa-a->base)/PAGE],v)||
  s->task[i].stack.start!=v.pa||s->task[i].stack.end!=v.pa+TASKS_STACK_BYTES||
  s->control.slots[i].stack.start!=v.pa||s->control.slots[i].stack.end!=v.pa+TASKS_STACK_BYTES||
  s->control.slots[i].yields!=8||s->control.slots[i].state!=(i?TASKS_READY:TASKS_RUNNING))return false;
 }
 if(p->lease[0].pa==p->lease[1].pa)return false;
 /* Irrevocable terminal path: discard all resumable state before releasing
  * stacks. Exact validated leases + serialized allocator ensure both releases. */
 struct s844_lease x=p->lease[0],y=p->lease[1];
 *f=(struct s843_frame){0};*s=(struct s843_scheduler){0};*p=(struct s845_pair){0};
 return s844_release(a,owner(0),x)&&s844_release(a,owner(1),y);
}
