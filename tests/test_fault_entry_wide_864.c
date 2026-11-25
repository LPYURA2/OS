#include <styleos/process_entry_847.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned ok,refused;
#define CHECK(x) do{assert(x);++ok;}while(0)
int main(void){
 const uint64_t fault_frame=S846_IST+4*4096-16-sizeof(struct s847_raw);
 for(unsigned v=6;v<=13;v+=7){
  struct s847_raw f={.vector=v,.rip=S846_CODE+53,.cs=0x5b,.ss=0x63,.rsp=S846_STACK+4096,.rflags=2};
  struct up_context c;struct up_event e;
  CHECK(s847_decode(&f,fault_frame,0,0x2000,0x5b,0x63,&c,&e));
  CHECK(e.kind==UP_FAULT&&e.vector==v&&!e.syndrome&&!e.fault_va&&e.active_root==0x2000&&e.kernel_sp==fault_frame&&c.pc==f.rip);
  for(unsigned i=0;i<12;i++){
   struct s847_raw bad=f;uint64_t frame=fault_frame,cr2=0,root=0x2000;
   switch(i){
   case 0:bad.error=1;break;
   case 1:cr2=S846_IST;break;
   case 2:frame=S846_IST+4096-16-sizeof f;break;
   case 3:frame+=8;break;
   case 4:bad.cs=0x38;break;
   case 5:bad.ss=0x40;break;
   case 6:bad.rip=S846_CODE+4096;break;
   case 7:bad.rsp=S846_STACK+4097;break;
   case 8:bad.rflags=0x3002;break;
   case 9:bad.vector=8;break;
   case 10:root=0x2001;break;
   case 11:bad.rflags=0x202;break;
   }
   struct up_context before=c;struct up_event old=e;
   assert(!s847_decode(&bad,frame,cr2,root,0x5b,0x63,&c,&e));
   assert(!memcmp(&c,&before,sizeof c)&&!memcmp(&e,&old,sizeof e));++refused;
  }
 }
 printf("S864 UD/GP fault frame decode: %u positive checks, %u immutable refusals PASS\n",ok,refused);
}
