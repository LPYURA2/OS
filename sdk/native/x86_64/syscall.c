#include <styleos/native_v1.h>
#if !defined(__x86_64__)
#error This candidate SDK transport is x86-64 only
#endif
struct sty_reply sty_call(uint64_t op,uint64_t arg,uint64_t address,uint64_t length){
 /* Returning syscalls preserve RBX/RCX and other GPRs; RDX is a
  * secondary return for Deep Drop. Compiler memory barrier is required
  * for user DATA buffers written through the trap. */
 __asm__ volatile("int $0x80" : "+a"(op),"+d"(length) : "b"(arg),"c"(address) : "cc","memory");
 return (struct sty_reply){op,length};
}
