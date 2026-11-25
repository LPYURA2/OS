#include <styleos/fx_context_849.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
_Static_assert(S849_SLOT_COUNT==4,"wide fixture only");
static struct s849_bank b;
static struct s849_image saved[4],out;
static unsigned checks,refusals;
#define C(x) do{assert(x);++checks;}while(0)
#define N(x) do{struct s849_bank before=b;struct s849_image old=out;assert(!(x));assert(!memcmp(&b,&before,sizeof b)&&!memcmp(&out,&old,sizeof out));++refusals;}while(0)
int main(void){
 struct s849_caps caps={.leaf1_edx=1u|(1u<<24)|(1u<<25)|(1u<<26),.cr0=0x23,.cr4=0x600,.mxcsr_mask=0xffbf};
 uint64_t roots[4]={0x1000,0x2000,0x3000,0x4000},gen[4]={1,2,3,4};
 uint64_t bad[4]={0x1000,0x2000,0x3000,0x1000};
 N(s849_init_set(&b,&caps,bad,gen));bad[3]=4;N(s849_init_set(&b,&caps,roots,bad));bad[0]=1;bad[1]=2;bad[2]=3;bad[3]=3;N(s849_init_set(&b,&caps,roots,bad));
 C(s849_init_set(&b,&caps,roots,gen));N(s849_init_set(&b,&caps,roots,gen));
 for(unsigned i=0;i<4;i++){
  C(s849_dispatch(&b,i,roots[i],gen[i],&out));
  for(unsigned j=160;j<416;j++){C(!out.bytes[j]);out.bytes[j]=(uint8_t)(i*61+j);}
  saved[i]=out;C(s849_suspend(&b,i,roots[i],gen[i],&out));
 }
 for(unsigned round=0;round<64;round++)for(unsigned i=0;i<4;i++){
  N(s849_dispatch(&b,i,roots[(i+1)%4],gen[i],&out));
  N(s849_dispatch(&b,i,roots[i],gen[i]+1,&out));
  C(s849_dispatch(&b,i,roots[i],gen[i],&out));C(!memcmp(&out,&saved[i],sizeof out));
  N(s849_dispatch(&b,(i+1)%4,roots[(i+1)%4],gen[(i+1)%4],&out));
  N(s849_suspend(&b,(i+1)%4,roots[(i+1)%4],gen[(i+1)%4],&out));
  N(s849_retire(&b,i,roots[i],gen[i]));
  C(s849_suspend(&b,i,roots[i],gen[i],&out));
 }
 for(unsigned i=0;i<4;i++){
  C(s849_retire(&b,i,roots[i],gen[i]));
  N(s849_dispatch(&b,i,roots[i],gen[i],&out));
  const unsigned char *p=(const void *)&b.slot[i];
  for(size_t j=0;j<sizeof b.slot[i];j++)C(!p[j]);
 }
 memset(&b,0,sizeof b);
 for(unsigned i=0;i<4;i++)gen[i]+=4;
 C(s849_init_set(&b,&caps,roots,gen));
 for(unsigned i=0;i<4;i++){
  N(s849_dispatch(&b,i,roots[i],gen[i]-4,&out));
  C(s849_dispatch(&b,i,roots[i],gen[i],&out));
  for(unsigned j=160;j<416;j++)C(!out.bytes[j]);
  C(s849_suspend(&b,i,roots[i],gen[i],&out));C(s849_retire(&b,i,roots[i],gen[i]));
 }
 printf("S864 four FX contexts HOST ONLY: %u checks, %u immutable refusals PASS\n",checks,refusals);
}
