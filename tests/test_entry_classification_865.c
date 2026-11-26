#include <styleos/process_entry_847.h>
#include <assert.h>
#include <stdio.h>
#define VA (S846_IST+4096-16-sizeof(struct s847_raw))
#define FAULT_VA (VA+3*4096)
static unsigned n;
#define CHECK(x) do{assert(x);++n;}while(0)
static enum rt865_frame_kind check(struct s847_raw f,uint64_t va,uint64_t cr2,bool armed){
 return rt865_classify_frame(&f,va,cr2,0x2000,0x1000,0x5b,0x63,armed);
}
int main(void){
 struct s847_raw f={.vector=128,.rip=S846_CODE+17,.rsp=S846_STACK+128,.cs=0x5b,.ss=0x63,.rflags=0x202};
 for(unsigned i=0;i<15;i++)f.gpr[i]=0x12345678u*i;
 for(unsigned i=0;i<256;i++){
  f.gpr[0]=i;CHECK(check(f,VA,0,false)==(i==S847_CALL_EXIT?RT865_TERMINAL:RT865_SYSCALL));
 }
 CHECK(check(f,VA,0,true)==RT865_INVALID);
 CHECK(check(f,VA+1,0,false)==RT865_INVALID);
 CHECK(check(f,VA,1,false)==RT865_INVALID);
 f.vector=0x50;
 CHECK(check(f,VA,0,true)==RT865_TICK);
 CHECK(check(f,VA,0,false)==RT865_INVALID);
 f.error=1;CHECK(check(f,VA,0,true)==RT865_INVALID);f.error=0;
 f.vector=14;f.error=5;
 CHECK(check(f,FAULT_VA,S846_IST,false)==RT865_TERMINAL);
 CHECK(check(f,VA,S846_IST,false)==RT865_INVALID);
 CHECK(check(f,FAULT_VA,S846_IST,true)==RT865_INVALID);
 f.error=0;CHECK(check(f,FAULT_VA,S846_IST,false)==RT865_INVALID);
 for(unsigned v=6;v<=13;v+=7){
  f.vector=v;CHECK(check(f,FAULT_VA,0,false)==RT865_TERMINAL);
  CHECK(check(f,FAULT_VA,1,false)==RT865_INVALID);
 }
 f.vector=128;f.gpr[0]=1;
 f.cs=0x38;CHECK(check(f,VA,0,false)==RT865_INVALID);f.cs=0x5b;
 f.rflags=2;CHECK(check(f,VA,0,false)==RT865_INVALID);f.rflags=0x202;
 f.rsp=S846_STACK;CHECK(check(f,VA,0,false)==RT865_INVALID);f.rsp=S846_STACK+16;
 f.rip=S846_CODE+4096;CHECK(check(f,VA,0,false)==RT865_INVALID);f.rip=S846_CODE;
 CHECK(rt865_classify_frame(&f,VA,0,0x1000,0x1000,0x5b,0x63,false)==RT865_INVALID);
 CHECK(rt865_classify_frame(&f,VA,0,0x2001,0x1000,0x5b,0x63,false)==RT865_INVALID);
 /* Existing strict expectations above are unchanged. New terminal-only
  * classifier accepts bad RSP, not forged kernel evidence. */
 f.rsp=0x12000000;
 for(unsigned i=0;i<3;i++){
  f.vector=i==0?6:i==1?128:0x50;
  uint64_t va=i==0?FAULT_VA:VA;bool armed=i==2;
  CHECK(check(f,va,0,armed)==RT865_INVALID);
  CHECK(rt865_classify_native_frame(&f,va,0,0x2000,0x1000,0x5b,0x63,armed)==RT865_USER_FAULT);
  CHECK(rt865_classify_native_frame(&f,va+8,0,0x2000,0x1000,0x5b,0x63,armed)==RT865_INVALID);
  CHECK(rt865_classify_native_frame(&f,va,0,0x1000,0x1000,0x5b,0x63,armed)==RT865_INVALID);
  f.cs=0x38;CHECK(rt865_classify_native_frame(&f,va,0,0x2000,0x1000,0x5b,0x63,armed)==RT865_INVALID);f.cs=0x5b;
  f.error=1;CHECK(rt865_classify_native_frame(&f,va,0,0x2000,0x1000,0x5b,0x63,armed)==RT865_INVALID);f.error=0;
  CHECK(rt865_classify_native_frame(&f,va,0,0x2000,0x1000,0x5b,0x63,!armed)==RT865_INVALID);
 }
 printf("S865 host frame classifier PASS: %u checks (not hardware evidence)\n",n);
}
