#include <styleos/runtime_irq_859.h>
#include <assert.h>
#include <stdio.h>
int main(void){
 const uint32_t version=0x50014u;
 uint32_t before[IRQ859_LVT_COUNT]={0x30020,0x10000,0x10000,0x700,0x400,0x10000};
 const uint32_t expected[IRQ859_LVT_COUNT]={0x30020,0x10000,0x10000,0x10700,0x10400,0x10000};
 unsigned positive=0,negative=0;uint32_t out[IRQ859_LVT_COUNT];
 for(unsigned state=0;state<4;state++){
  before[3]=0x700u|((state&1u)?0x10000u:0);before[4]=0x400u|((state&2u)?0x10000u:0);
  assert(irq859_lvt_plan(version,before,out));for(unsigned j=0;j<IRQ859_LVT_COUNT;j++)assert(out[j]==expected[j]);positive++;
  for(unsigned i=0;i<IRQ859_LVT_COUNT;i++)for(unsigned bit=0;bit<32;bit++){
   uint32_t saved=before[i];before[i]^=UINT32_C(1)<<bit;
   for(unsigned j=0;j<IRQ859_LVT_COUNT;j++)out[j]=0xdeadbeefu;
   bool ok=((i==3||i==4)&&bit==16);
   assert(irq859_lvt_plan(version,before,out)==ok);
   for(unsigned j=0;j<IRQ859_LVT_COUNT;j++)assert(out[j]==(ok?expected[j]:0xdeadbeefu));
   if(ok)positive++;else negative++;before[i]=saved;
  }
  for(unsigned bit=0;bit<32;bit++){
   for(unsigned j=0;j<IRQ859_LVT_COUNT;j++)out[j]=0xdeadbeefu;
   assert(!irq859_lvt_plan(version^(UINT32_C(1)<<bit),before,out));negative++;
   for(unsigned j=0;j<IRQ859_LVT_COUNT;j++)assert(out[j]==0xdeadbeefu);
  }
 }
 for(unsigned j=0;j<IRQ859_LVT_COUNT;j++)out[j]=0xdeadbeefu;
 assert(!irq859_lvt_plan(version,0,out));negative++;
 for(unsigned j=0;j<IRQ859_LVT_COUNT;j++)assert(out[j]==0xdeadbeefu);
 assert(!irq859_lvt_plan(version,before,0));negative++;
 printf("Exact LVT bank isolation model: %u allowed / %u refused; refused output unchanged\n",positive,negative);
}
