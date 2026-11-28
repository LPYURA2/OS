#ifndef STYLEOS_SDK_NATIVE_V1_H
#define STYLEOS_SDK_NATIVE_V1_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Public user-side candidate SDK. No privileged types or kernel pointers.
 * Version is a compile-time wire contract, not runtime negotiation. */
#define STY_SDK_ABI_VERSION 1u
#define STY_INPUT_BYTES 64u
#define STY_FRAME_RECTS 32u
enum sty_window_status { STY_OK=0,STY_INVALID,STY_NOT_READY,STY_DENIED,
 STY_MISSING,STY_FULL,STY_EMPTY,STY_STALE,STY_OVERFLOW };
enum sty_event_kind { STY_KEY=1,STY_TEXT=2,STY_POINTER=3,STY_DROP=4 };
enum sty_operation { STY_YIELD=1,STY_EXIT=2,STY_PIXEL=0x100,STY_TILE=0x101,
 STY_INPUT=0x102,STY_FRAME=0x103 };
struct sty_reply { uint64_t status,value; };
struct sty_rect { uint32_t x,y,w,h,rgb; };
_Static_assert(sizeof(struct sty_rect)==20,"rectangle ABI");
struct sty_event {
 uint32_t sequence,window,kind,code,modifiers,down,size;
 uint8_t bytes[32];
};
/* Only the x86_64 implementation traps. The kernel authenticates the
 * caller. Buffers passed to it MUST reside in the mapped private DATA
 * extent, not stack, code or foreign pages. Window 0 means unique port. */
struct sty_reply sty_call(uint64_t op,uint64_t arg,uint64_t address,uint64_t length);
static inline uint32_t sty_le32(const uint8_t *p){
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
/* Does not alter the destination on malformed or unknown input. */
static inline bool sty_decode_event(const uint8_t *wire,size_t bytes,struct sty_event *out){
 if(!wire||!out||bytes!=STY_INPUT_BYTES||sty_le32(wire)!=STY_SDK_ABI_VERSION)return false;
 struct sty_event e={.sequence=sty_le32(wire+4),.window=sty_le32(wire+8),
 .kind=sty_le32(wire+12),.code=sty_le32(wire+16),.modifiers=sty_le32(wire+20),
 .down=sty_le32(wire+24),.size=sty_le32(wire+28)};
 if(!e.sequence||!e.window||e.kind<STY_KEY||e.kind>STY_DROP||e.down>1||e.size>32)return false;
 if((e.kind==STY_KEY&&e.size)||(e.kind==STY_TEXT&&!e.size)||
    (e.kind==STY_POINTER&&e.size!=8)||(e.kind==STY_DROP&&e.size!=8))return false;
 for(unsigned i=0;i<32;++i){
  if(i>=e.size&&wire[32+i])return false;
  e.bytes[i]=wire[32+i];
 }
 *out=e;return true;
}
static inline uint64_t sty_window_input(uint32_t window,uint8_t *data,size_t bytes){
 if(!data||bytes!=STY_INPUT_BYTES)return STY_INVALID;
 return sty_call(STY_INPUT,window,(uintptr_t)data,bytes).status;
}
static inline uint64_t sty_window_frame(uint32_t window,const struct sty_rect *data,size_t count){
 if(!data||!count||count>STY_FRAME_RECTS)return STY_INVALID;
 return sty_call(STY_FRAME,window,(uintptr_t)data,count).status;
}
static inline uint64_t sty_window_pixel(uint32_t window,uint32_t rgb){
 if(rgb>0xffffffu)return STY_INVALID;
 return sty_call(STY_PIXEL,window,rgb,0).status;
}
static inline uint64_t sty_window_tile(uint32_t window,const uint32_t *data,unsigned w,unsigned h){
 if(!data||!w||!h||w>16||h>16)return STY_INVALID;
 return sty_call(STY_TILE,window,(uintptr_t)data,((uint64_t)h<<16)|w).status;
}
static inline uint64_t sty_yield(void){return sty_call(STY_YIELD,0,0,0).status;}
#endif
