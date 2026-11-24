#include <styleos/rtc_x86_854.h>
#include <assert.h>
#include <stdio.h>
int main(void){
 unsigned positive=0,negative=0;
 for(unsigned bits=0;bits<256;bits++){
   struct rtc_x86_contract_854 c={.expected_root=(bits&128)?0:0x2000,.known_index=0x8d,
     .terminal_fixture=!!(bits&1),.exclusive_cmos=!!(bits&2),.known_shadow=!!(bits&4),.post_ebs=!!(bits&8)};
   bool expected=bits==31;
   bool actual=rtc_x86_gate_854(&c,(bits&32)?0x3000:0x2000,(bits&64)?0x202:2,8,!!(bits&16));
   assert(actual==expected);if(actual)positive++;else negative++;
 }
 struct rtc_x86_contract_854 c={0x2000,0x0d,true,true,true,true};
 assert(!rtc_x86_gate_854(0,0x2000,2,8,true));negative++;
 assert(!rtc_x86_gate_854(&c,0x2000,2,0,true));negative++;
 assert(!rtc_x86_gate_854(&c,0x2000,2,11,true));negative++;
 c.expected_root=0x2001;assert(!rtc_x86_gate_854(&c,0x2001,2,8,true));negative++;
 printf("x86 gate: %u positive, %u refusals PASS (host-only)\n",positive,negative);
}
