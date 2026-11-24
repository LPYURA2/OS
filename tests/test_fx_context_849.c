#include <styleos/fx_context_849.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <cpuid.h>
static unsigned checks;
#define C(x) do{assert(x);++checks;}while(0)
static void set32(struct s849_image *i,unsigned at,uint32_t x){for(unsigned j=0;j<4;j++)i->bytes[at+j]=(uint8_t)(x>>(j*8));}
static uint32_t get32(const struct s849_image *i,unsigned at){uint32_t x=0;for(unsigned j=0;j<4;j++)x|=(uint32_t)i->bytes[at+j]<<(j*8);return x;}
extern void s849_host_capture(struct s849_image *);
extern void s849_host_roundtrip(const struct s849_image *,const struct s849_image *,struct s849_image *,struct s849_image *,struct s849_image *,struct s849_image *);
int main(void){
 struct s849_caps c={.leaf1_edx=(1u<<0)|(1u<<24)|(1u<<25)|(1u<<26),.cr0=0x23,.cr4=0x600,.mxcsr_mask=0xffbf};uint32_t mask=0;
 C(s849_admit(&c,&mask)&&mask==0xffbf);struct s849_caps bad=c;
 for(unsigned j=0;j<4;j++){unsigned bits[]={0,24,25,26};bad=c;bad.leaf1_edx^=1u<<bits[j];C(!s849_admit(&bad,&mask)&&mask==0xffbf);}
 for(unsigned j=1;j<=5;j++){if(j==4)continue;bad=c;bad.cr0^=1u<<j;C(!s849_admit(&bad,&mask));}
 for(unsigned j=9;j<=10;j++){bad=c;bad.cr4^=1u<<j;C(!s849_admit(&bad,&mask));}
 bad=c;bad.cr4|=1u<<18;C(!s849_admit(&bad,&mask));bad=c;bad.leaf1_ecx|=1u<<27;C(!s849_admit(&bad,&mask));bad=c;bad.mxcsr_mask=0x10000;C(!s849_admit(&bad,&mask));bad=c;bad.mxcsr_mask=0;C(s849_admit(&bad,&mask)&&mask==0xffbf);
 struct s849_bank b={0},before;struct s849_image a={0},z={0},out={0};
 C(!s849_init(&b,&c,0x1000,0x1000));C(!s849_init(&b,&c,0x1001,0x2000));C(s849_init(&b,&c,0x1000,0x2000));before=b;C(!s849_init(&b,&c,0x3000,0x4000));C(!memcmp(&b,&before,sizeof b));
 C(!s849_dispatch(&b,0,0x2000,1,&a));C(!s849_dispatch(&b,0,0x1000,2,&a));C(!s849_dispatch(&b,0,0x1000,1,&b.slot[0].image));C(!memcmp(&b,&before,sizeof b));
 C(s849_dispatch(&b,0,0x1000,1,&a));C(get32(&a,24)==0x1f80&&a.bytes[0]==0x7f&&a.bytes[1]==3);C(!s849_dispatch(&b,1,0x2000,1,&z));C(!s849_retire(&b,0,0x1000,1));
 for(unsigned j=160;j<416;j++)a.bytes[j]=(uint8_t)(j*3+7);
 before=b;set32(&a,24,0x10000);C(!s849_suspend(&b,0,0x1000,1,&a));C(!memcmp(&b,&before,sizeof b));set32(&a,24,0x1f80);set32(&a,28,0xffff);C(!s849_suspend(&b,0,0x1000,1,&a));set32(&a,28,0xffbf);
 C(!s849_suspend(&b,1,0x2000,1,&a));C(s849_suspend(&b,0,0x1000,1,&a));C(s849_dispatch(&b,1,0x2000,1,&z));
 for(unsigned j=160;j<416;j++){C(z.bytes[j]==0);z.bytes[j]=(uint8_t)(j*5+11);}C(s849_suspend(&b,1,0x2000,1,&z));
 for(unsigned k=0;k<16;k++){C(s849_dispatch(&b,0,0x1000,1,&out));C(!memcmp(&a,&out,sizeof a));C(s849_suspend(&b,0,0x1000,1,&out));C(s849_dispatch(&b,1,0x2000,1,&out));C(!memcmp(&z,&out,sizeof z));C(s849_suspend(&b,1,0x2000,1,&out));}
 C(s849_retire(&b,0,0x1000,1));C(!s849_dispatch(&b,0,0x1000,1,&out));for(unsigned j=0;j<sizeof b.slot[0];j++)C(((const unsigned char*)&b.slot[0])[j]==0);C(s849_dispatch(&b,1,0x2000,1,&out));C(!memcmp(&z,&out,sizeof z));C(s849_suspend(&b,1,0x2000,1,&out));C(s849_retire(&b,1,0x2000,1));
 printf("S849 model: %u assertions PASS\n",checks);
 unsigned model_checks=checks;
 unsigned eax,ebx,ecx,edx;C(__get_cpuid(1,&eax,&ebx,&ecx,&edx));C((edx&c.leaf1_edx)==c.leaf1_edx);
 struct s849_image host={0},oa={0},oz={0},backup={0},again={0};s849_host_capture(&host);uint32_t observed=get32(&host,28);uint32_t effective=observed?(observed&0xffffu):S849_MXCSR_FALLBACK;fprintf(stderr,"FX mask=%08x mxcsr=%08x alignment=%zu\n",observed,get32(&host,24),(size_t)((uintptr_t)&host%16));C(s849_image_valid(&host,effective));
 /* Real CPU test: legacy context only, OS owns AVX/XSAVE; no guest mode claim. */
 set32(&a,28,observed);set32(&z,28,observed);set32(&a,24,0x1f80);set32(&z,24,0x3f80);a.bytes[1]=3;z.bytes[1]=7;
 /* Set one nonempty x87 stack entry with an exact 80-bit normal value. */
 a.bytes[4]=1;z.bytes[4]=1;a.bytes[39]=0x80;a.bytes[40]=0xff;a.bytes[41]=0x3f;z.bytes[39]=0x80;z.bytes[40]=0;z.bytes[41]=0x40;
 C(s849_image_valid(&a,effective));C(s849_image_valid(&z,effective));
 s849_host_roundtrip(&a,&z,&oa,&oz,&backup,&again);
 for(unsigned j=160;j<416;j++){C(oa.bytes[j]==a.bytes[j]);C(oz.bytes[j]==z.bytes[j]);C(again.bytes[j]==a.bytes[j]);}
 C(get32(&oa,24)==0x1f80);C(get32(&oz,24)==0x3f80);C(oa.bytes[0]==0x7f&&oa.bytes[1]==3);C(oz.bytes[0]==0x7f&&oz.bytes[1]==7);C(oa.bytes[4]==1&&oz.bytes[4]==1);C(!memcmp(a.bytes+32,oa.bytes+32,10));C(!memcmp(z.bytes+32,oz.bytes+32,10));C(get32(&again,24)==0x1f80);C(!memcmp(a.bytes+32,again.bytes+32,10));C(again.bytes[0]==0x7f&&again.bytes[1]==3&&again.bytes[4]==1);
 printf("S849 host CPU legacy FP/SSE roundtrip: %u additional assertions PASS (NOT QEMU CPL3)\n",checks-model_checks);return 0;
}
