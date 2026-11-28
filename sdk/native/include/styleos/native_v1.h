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
 STY_INPUT=0x102,STY_FRAME=0x103,STY_PATCH=0x120,STY_FRAME_GEN=0x121,STY_TEXT_LAYER=0x122,
 STY_FILE_READ=0x130,STY_FILE_WRITE=0x131,STY_FILE_TAKE=0x132,STY_FILE_CANCEL=0x133,STY_FILE_RECEIVE=0x134 };
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
/* Candidate wire v1: fixed-size LE packet in caller private DATA.
 * Generation obtained from frame query or previous successful patch.
 * No-op patch returns the unchanged generation; failure returns zero. */
#define STY_PATCH_BYTES 1048u
static inline void sty_put32(uint8_t *p,uint32_t v){
 for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));
}
static inline bool sty_patch_encode(uint8_t *wire,size_t bytes,uint32_t generation,
 uint32_t x,uint32_t y,unsigned w,unsigned h,const uint32_t *pixels){
 if(!wire||bytes!=STY_PATCH_BYTES||!generation||!pixels||!w||!h||w>16||h>16||x%16||y%16)return false;
 uintptr_t a=(uintptr_t)wire,b=(uintptr_t)pixels;
 size_t n=(size_t)w*h*sizeof *pixels;
 if(bytes>UINTPTR_MAX-a||n>UINTPTR_MAX-b||
    !(a+bytes<=b||b+n<=a))return false;
 for(unsigned i=0;i<w*h;++i)if(pixels[i]>0xffffffu)return false;
 sty_put32(wire,1);sty_put32(wire+4,generation);sty_put32(wire+8,x);
 sty_put32(wire+12,y);sty_put32(wire+16,w);sty_put32(wire+20,h);
 for(unsigned i=0;i<256;++i)sty_put32(wire+24+4*i,i<w*h?pixels[i]:0);
 return true;
}
static inline struct sty_reply sty_window_generation(uint32_t window){
 return sty_call(STY_FRAME_GEN,window,0,0);
}
static inline struct sty_reply sty_window_patch(uint32_t window,const uint8_t *wire,size_t bytes){
 if(!wire||bytes!=STY_PATCH_BYTES)return (struct sty_reply){STY_INVALID,0};
 return sty_call(STY_PATCH,window,(uintptr_t)wire,bytes);
}
#define STY_TEXT_RUNS 8u
#define STY_TEXT_GLYPHS 64u
#define STY_TEXT_HEADER 12u
#define STY_TEXT_RUN_BYTES 280u
#define STY_TEXT_WIRE_MAX (STY_TEXT_HEADER+STY_TEXT_RUNS*STY_TEXT_RUN_BYTES)
struct sty_text_run {
 uint32_t x,y,width,foreground,background,count;
 uint32_t cp[STY_TEXT_GLYPHS];
};
static inline size_t sty_text_wire_size(unsigned count){
 return count<=STY_TEXT_RUNS?STY_TEXT_HEADER+(size_t)count*STY_TEXT_RUN_BYTES:0;
}
/* Validate all runs before encoding. Coordinates also checked against
 * actual window by kernel. Does not change wire on refusal. */
static inline bool sty_text_encode(uint8_t *wire,size_t bytes,uint32_t generation,
 const struct sty_text_run *runs,unsigned count){
 if(!wire||!generation||!bytes||bytes!=sty_text_wire_size(count)||(count&&!runs))return false;
 uintptr_t a=(uintptr_t)wire,b=(uintptr_t)runs;
 size_t n=(size_t)count*sizeof *runs;
 if(bytes>UINTPTR_MAX-a||(count&&(n>UINTPTR_MAX-b||!(a+bytes<=b||b+n<=a))))return false;
 for(unsigned i=0;i<count;++i){
  const struct sty_text_run *q=&runs[i];
  if(!q->width||!q->count||q->count>STY_TEXT_GLYPHS||q->foreground>0xffffffu||q->background>0xffffffu)return false;
  for(unsigned j=0;j<STY_TEXT_GLYPHS;++j){
   uint32_t cp=q->cp[j];
   if(j>=q->count?cp!=0:(cp<32u||(cp>=127u&&cp<=159u)||cp>0x10ffffu||(cp>=0xd800u&&cp<=0xdfffu)))return false;
  }
 }
 sty_put32(wire,1);sty_put32(wire+4,generation);sty_put32(wire+8,count);
 for(unsigned i=0;i<count;++i){
  uint8_t *p=wire+STY_TEXT_HEADER+i*STY_TEXT_RUN_BYTES;
  const struct sty_text_run *q=&runs[i];
  sty_put32(p,q->x);sty_put32(p+4,q->y);sty_put32(p+8,q->width);
  sty_put32(p+12,q->foreground);sty_put32(p+16,q->background);sty_put32(p+20,q->count);
  for(unsigned j=0;j<STY_TEXT_GLYPHS;++j)sty_put32(p+24+4*j,q->cp[j]);
 }
 return true;
}
static inline struct sty_reply sty_window_text(uint32_t window,const uint8_t *wire,size_t bytes){
 if(!wire||bytes<STY_TEXT_HEADER||bytes>STY_TEXT_WIRE_MAX||
    (bytes-STY_TEXT_HEADER)%STY_TEXT_RUN_BYTES)return (struct sty_reply){STY_INVALID,0};
 return sty_call(STY_TEXT_LAYER,window,(uintptr_t)wire,bytes);
}
/* v1 file status namespace is distinct from window status. Grants come
 * from trusted file selection, not filenames or application-supplied IDs.
 * Submission does not mean a durable save. Poll take; BUSY is pending.
 * A successful take returns actual transaction status and byte count. */
enum sty_file_status { STY_F_OK=0,STY_F_INVALID,STY_F_DENIED,STY_F_STALE,
 STY_F_BUSY,STY_F_FULL,STY_F_IO,STY_F_CORRUPT,STY_F_CANCELLED,STY_F_UNCERTAIN,
 STY_F_END,STY_F_UNAVAILABLE };
#define STY_FILE_MAX 4096u
/* Obtain one already-approved grant; this does not open a picker or
 * authorize any file. END means nothing is pending. */
static inline struct sty_reply sty_file_receive(void){
 return sty_call(STY_FILE_RECEIVE,0,0,0);
}
static inline struct sty_reply sty_file_read(uint64_t grant,size_t capacity){
 if(!grant||!capacity||capacity>STY_FILE_MAX)return (struct sty_reply){STY_F_INVALID,0};
 return sty_call(STY_FILE_READ,grant,0,capacity);
}
static inline struct sty_reply sty_file_write(uint64_t grant,const uint8_t *data,size_t bytes){
 if(!grant||bytes>STY_FILE_MAX||(!data)!=(!bytes))return (struct sty_reply){STY_F_INVALID,0};
 return sty_call(STY_FILE_WRITE,grant,(uintptr_t)data,bytes);
}
static inline struct sty_reply sty_file_take(uint64_t request,uint8_t *out,size_t capacity){
 if(!request||capacity>STY_FILE_MAX||(!out)!=(!capacity))return (struct sty_reply){STY_F_INVALID,0};
 return sty_call(STY_FILE_TAKE,request,(uintptr_t)out,capacity);
}
static inline struct sty_reply sty_file_cancel(uint64_t request){
 if(!request)return (struct sty_reply){STY_F_INVALID,0};
 return sty_call(STY_FILE_CANCEL,request,0,0);
}
static inline bool sty_file_completion(struct sty_reply reply,enum sty_file_status *status,size_t *bytes){
 uint32_t result=(uint32_t)reply.value;uint64_t n=reply.value>>32;
 if(!status||!bytes||reply.status!=STY_F_OK||result>STY_F_END||n>STY_FILE_MAX||
    (result!=STY_F_OK&&n))return false;
 uintptr_t a=(uintptr_t)status,b=(uintptr_t)bytes;
 if(sizeof *status>UINTPTR_MAX-a||sizeof *bytes>UINTPTR_MAX-b||
    !(a+sizeof *status<=b||b+sizeof *bytes<=a))return false;
 *status=(enum sty_file_status)result;*bytes=(size_t)n;return true;
}
static inline uint64_t sty_yield(void){return sty_call(STY_YIELD,0,0,0).status;}
#endif
