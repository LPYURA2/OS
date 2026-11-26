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
extern void s849_host_capture(struct s849_image *);
extern void s849_host_roundtrip(const struct s849_image *,const struct s849_image *,struct s849_image *,struct s849_image *,struct s849_image *,struct s849_image *);
static uint32_t read32(const struct s849_image *x,unsigned n){
 return (uint32_t)x->bytes[n]|((uint32_t)x->bytes[n+1]<<8)|((uint32_t)x->bytes[n+2]<<16)|((uint32_t)x->bytes[n+3]<<24);
}
static void write32(struct s849_image *x,unsigned n,uint32_t v){
 for(unsigned j=0;j<4;j++)x->bytes[n+j]=(uint8_t)(v>>(8*j));
}
static void actual_fx(void){
 struct s849_image host={0},a[4]={0},oa={0},ob={0},again={0},backup={0};
 s849_host_capture(&host);
 uint32_t reported=read32(&host,28),mask=reported?(reported&0xffffu):S849_MXCSR_FALLBACK;
 unsigned n=0;
 C(s849_image_valid(&host,mask));
 /* Distinct valid x87 normal values, rounding modes and all sixteen XMM
  * registers. Existing assembly backs up/restores the host's own state. */
 for(unsigned i=0;i<4;i++){
  a[i].bytes[0]=0x7f;a[i].bytes[1]=3;a[i].bytes[4]=1;
  a[i].bytes[39]=0x80;a[i].bytes[40]=(uint8_t)(0xff+i);
  a[i].bytes[41]=(uint8_t)(i?0x40:0x3f);
  write32(&a[i],24,0x1f80|(i<<13));write32(&a[i],28,reported);
  for(unsigned j=160;j<416;j++)a[i].bytes[j]=(uint8_t)(i*61+j);
  C(s849_image_valid(&a[i],mask));
  struct s849_image altered=a[i];
  altered.bytes[511]^=0xff;C(s849_arch_equal(&a[i],&altered,mask));
  altered.bytes[160]^=1;C(!s849_arch_equal(&a[i],&altered,mask));
  altered=a[i];altered.bytes[32]^=1;C(!s849_arch_equal(&a[i],&altered,mask));
 }
 for(unsigned round=0;round<64;round++)for(unsigned i=0;i<4;i++){
  unsigned j=(i+1)%4;
  s849_host_roundtrip(&a[i],&a[j],&oa,&ob,&backup,&again);
  const struct s849_image *actual[]={&oa,&ob,&again};
  const struct s849_image *expected[]={&a[i],&a[j],&a[i]};
  for(unsigned k=0;k<3;k++){
   C(s849_arch_equal(actual[k],expected[k],mask));
   C(!memcmp(actual[k]->bytes,expected[k]->bytes,5));
   C(!memcmp(actual[k]->bytes+32,expected[k]->bytes+32,10));
   C(read32(actual[k],24)==read32(expected[k],24));
   C(!memcmp(actual[k]->bytes+160,expected[k]->bytes+160,256));
   ++n;
  }
 }
 printf("S864 four actual host CPU FX images: %u captures PASS (not guest CPL3)\n",n);
}
int main(void){
 struct s849_caps caps={.leaf1_edx=1u|(1u<<24)|(1u<<25)|(1u<<26),.cr0=0x23,.cr4=0x600,.mxcsr_mask=0xffbf};
 uint64_t roots[4]={0x1000,0x2000,0x3000,0x4000},gen[4]={1,2,3,4};
 uint64_t bad[4]={0x1000,0x2000,0x3000,0x1000};
 N(s849_init_set(&b,&caps,bad,gen));bad[3]=0;N(s849_init_set(&b,&caps,roots,bad));bad[0]=1;bad[1]=2;bad[2]=3;bad[3]=3;N(s849_init_set(&b,&caps,roots,bad));
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
 memset(&b,0,sizeof b);
 C(s865_fx_open(&b,&caps));N(s865_fx_open(&b,&caps));
 N(s849_dispatch(&b,2,roots[2],10,&out));
 C(s865_fx_bind(&b,2,roots[2],10));
 N(s865_fx_bind(&b,0,roots[2],11));
 N(s865_fx_bind(&b,0,roots[0],10));
 C(s849_dispatch(&b,2,roots[2],10,&out));out.bytes[160]=0x5a;
 N(s865_fx_bind(&b,0,roots[0],11));
 C(s849_suspend(&b,2,roots[2],10,&out));
 C(s865_fx_bind(&b,0,roots[0],11));
 C(s849_retire(&b,0,roots[0],11));
 N(s865_fx_bind(&b,0,roots[0],11));
 C(s865_fx_bind(&b,0,roots[0],12));
 C(s849_dispatch(&b,0,roots[0],12,&out));C(out.bytes[160]==0);
 C(s849_suspend(&b,0,roots[0],12,&out));C(s849_retire(&b,0,roots[0],12));
 C(s849_dispatch(&b,2,roots[2],10,&out));C(out.bytes[160]==0x5a);
 C(s849_suspend(&b,2,roots[2],10,&out));C(s849_retire(&b,2,roots[2],10));
 printf("S864 four FX contexts HOST ONLY: %u checks, %u immutable refusals PASS\n",checks,refusals);
 actual_fx();
}
