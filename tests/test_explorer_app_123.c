/* Host application controller test. Transport mock only, never linked into package. */
#include <assert.h>
#include <string.h>
#include "../apps/explorer/explorer.c"
static unsigned op,ordinal,dir,phase;
static uint64_t serial;
static bool fail_row;
struct sty_reply sty_call(uint64_t operation,uint64_t arg,uint64_t address,uint64_t length){
 uint8_t *p=(uint8_t *)(uintptr_t)address;
 if(operation==0x140||operation==0x141||operation==0x142||operation==0x144||operation==0x146){
  op=(unsigned)operation;ordinal=(unsigned)arg;phase=0;return (struct sty_reply){0,++serial};
 }
 if(operation==0x143){
  assert(arg==serial);
  if(!phase++)return (struct sty_reply){STY_F_BUSY,0};
  if(op==0x141){dir=1;return (struct sty_reply){0,0};}
  if(op==0x142){dir=0;return (struct sty_reply){0,0};}
  if(op==0x146){dir=0;assert(length==24);memset(p,0,24);p[0]=p[8]=p[16]=1;return (struct sty_reply){0,0};}
  if(op==0x144){assert(length==64);memset(p,0,64);p[0]=p[8]=p[16]=1;
   sty_ex_put64(p+24,UINT64_C(1099511627776));sty_ex_put64(p+32,UINT64_C(1099511627776));sty_ex_put64(p+40,512);sty_ex_put64(p+48,512);return (struct sty_reply){0,0};}
  assert(op==0x140&&length==296);
  if(fail_row&&ordinal==1)return (struct sty_reply){STY_F_STALE,0};
  if(ordinal>=(dir?0:11))return (struct sty_reply){STY_F_END,0};
  memset(p,0,296);p[0]=1;p[4]=ordinal?1:2;p[8]=1;p[16]=(uint8_t)(ordinal+2);p[40]=(uint8_t)('a'+ordinal);
  if(ordinal){p[32]=1;sty_ex_put64(p+24,UINT64_C(1099511627776));}
  return (struct sty_reply){0,0};
 }
 if(operation==STY_FRAME){assert(length<=16&&length>0);return (struct sty_reply){0,0};}
 if(operation==STY_FRAME_GEN)return (struct sty_reply){0,1};
 if(operation==STY_TEXT_LAYER){assert(length<=STY_TEXT_WIRE_MAX&&sty_le32(p+8)<=STY_TEXT_RUNS);return (struct sty_reply){0,2};}
 assert(operation==STY_YIELD);return (struct sty_reply){0,0};
}
int main(void){
 load();assert(v.count==8&&v.more&&v.rows[0].kind==2&&render()&&nt==32);
 key(0x1c);assert(v.depth==1&&!v.count&&v.status==0&&render());
 key(0x0e);assert(!v.depth&&v.count==8);
 for(unsigned i=0;i<8;++i)key(0x150);
 assert(v.first==8&&v.count==3&&!v.more&&render());
 key(0x148);assert(v.first==0&&v.count==8);
 key(0x0f);assert(v.computer&&v.volume.volume_bytes==UINT64_C(1099511627776)&&render());
 key(0x40);assert(v.warm&&render());key(0x0f);assert(!v.computer);
 fail_row=true;load();assert(v.status==STY_F_STALE&&!v.count&&render());
 fail_row=false;key(0x3f);assert(v.status==0&&v.count==8&&render());
 return 0;
}
