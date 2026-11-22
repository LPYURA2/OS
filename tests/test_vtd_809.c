#include <styleos/vtd_809.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
int main(void){
 struct vtd809_regs r={.version=0x10,.cap=UINT64_C(0xcafebabe),.ecap=0x800,.gsts=0,.gcmd=0};
 struct vtd809_state s={0};
 assert(vtd809_decode(&r,&s));assert(s.major==1&&s.minor==0&&!s.translation_status&&!s.translation_command);
 r.gsts=UINT32_C(0xc0000000);r.gcmd=UINT32_C(0x80000000);
 assert(vtd809_decode(&r,&s)&&s.translation_status&&s.translation_command&&s.root_set_status);
 r.version=0;assert(!vtd809_decode(&r,&s));r.version=0x10;
 r.cap=0;assert(!vtd809_decode(&r,&s));r.cap=1;
 r.ecap=UINT64_MAX;assert(!vtd809_decode(&r,&s));r.ecap=0;
 assert(vtd809_decode(&r,&s));r.version=0x100;assert(!vtd809_decode(&r,&s));
 assert(!vtd809_decode(0,&s));assert(!vtd809_decode(&r,0));
 puts("S809 host VT-d register model PASS");return 0;
}
