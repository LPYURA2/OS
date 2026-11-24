#include <styleos/pit_x86_855.h>
#include <assert.h>
#include <stdio.h>
int main(void){unsigned ok=0,no=0;for(unsigned b=0;b<1024;b++){
struct pit_x86_contract_855 c={.root=(b&1)?0x2000:0,.terminal=!!(b&2),.exclusive=!!(b&4),.post_ebs=!!(b&8),.reference_attested=!!(b&16)};
bool want=b==1023;bool got=pit_x86_gate_855(&c,(b&32)?0x2000:0x3000,(b&64)?2:0x202,(b&128)?8:11,!!(b&256),!!(b&512),true); /* SSE2 tested separately; 1023 required */
assert(got==want);if(got)ok++;else no++;}
struct pit_x86_contract_855 c={0x2000,true,true,true,true};assert(!pit_x86_gate_855(&c,0x2000,2,8,true,true,false));assert(!pit_x86_gate_855(0,0x2000,2,8,true,true,true));assert(!pit_x86_gate_855(&c,0x2000,2,0,true,true,true));c.root=0x2001;assert(!pit_x86_gate_855(&c,0x2001,2,8,true,true,true));no+=4;printf("PIT x86 gate: %u positive, %u refusals PASS\n",ok,no);}
