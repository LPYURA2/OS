#include <styleos/vtd_809.h>
#include <stddef.h>
#include <stdint.h>
/* Intel VT-d version: minor bits 3:0, major bits 7:4. */
bool vtd809_decode(const struct vtd809_regs *in,struct vtd809_state *out){
 if(!in||!out||!in->cap||in->cap==UINT64_MAX||in->ecap==UINT64_MAX||
    in->version==UINT32_MAX||(in->version&UINT32_C(0xffffff00)))return false;
 unsigned major=(in->version>>4)&15u,minor=in->version&15u;
 if(!major)return false;
 out->major=(uint8_t)major;out->minor=(uint8_t)minor;
 out->translation_status=(in->gsts&(UINT32_C(1)<<31))!=0;
 out->translation_command=(in->gcmd&(UINT32_C(1)<<31))!=0;
 out->root_set_status=(in->gsts&(UINT32_C(1)<<30))!=0;
 return true;
}
