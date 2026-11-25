#include <styleos/runtime_irq_859.h>
#include <assert.h>
#include <stdio.h>
int main(void){
 const uint32_t version=0x50014u;
 uint32_t before[IRQ859_LVT_COUNT]={0x10000,0x10000,0x10000,0x700,0x10000,0x10000};
 unsigned positive=0,negative=0;uint32_t out=0;
 for(unsigned state=0;state<2;state++){
  before[3]=state?0x10700:0x700;
  assert(irq859_lvt_plan(version,before,&out)&&out==0x10700u);positive++;
  for(unsigned i=0;i<IRQ859_LVT_COUNT;i++)for(unsigned bit=0;bit<32;bit++){
   uint32_t saved=before[i];before[i]^=UINT32_C(1)<<bit;out=0xdeadbeefu;
   bool ok=(i==3?bit==16:(bit!=16&&bit!=12));
   assert(irq859_lvt_plan(version,before,&out)==ok);
   if(ok){assert(out==0x10700u);positive++;}else{assert(out==0xdeadbeefu);negative++;}before[i]=saved;
  }
  for(unsigned v=0;v<256;v++){
   out=0xdeadbeefu;bool ok=v==5;
   assert(irq859_lvt_plan((v<<16)|0x14u,before,&out)==ok);
   if(ok){assert(out==0x10700u);positive++;}else{assert(out==0xdeadbeefu);negative++;}
  }
 }
 out=0xdeadbeefu;assert(!irq859_lvt_plan(version,0,&out)&&out==0xdeadbeefu);negative++;
 assert(!irq859_lvt_plan(version,before,0));negative++;
 printf("LVT ExtINT isolation model: %u allowed / %u refused; refused output unchanged\n",positive,negative);
}
