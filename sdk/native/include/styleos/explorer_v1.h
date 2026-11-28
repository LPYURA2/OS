#ifndef STYLEOS_EXPLORER_V1_H
#define STYLEOS_EXPLORER_V1_H
#include <styleos/native_v1.h>
/* Candidate public data contract, no syscall advertised until live binding.
 * LE version, kind, name length, reserved; id, size, revision; UTF-8 name
 * and zero padding. File IDs are metadata, not capabilities. */
enum { STY_EXPLORER_ROW_BYTES=296, STY_EXPLORER_NAME=255 };
enum sty_explorer_op { STY_EX_ROW=1, STY_EX_ENTER=2, STY_EX_UP=3, STY_EX_VOLUME=4 };
struct sty_explorer_row { uint64_t id,bytes,revision; uint32_t kind,name_bytes; uint8_t name[256]; };
static inline uint64_t sty_ex_le64(const uint8_t *p){
 return (uint64_t)sty_le32(p)|((uint64_t)sty_le32(p+4)<<32);
}
static inline bool sty_ex_name(const uint8_t *p,unsigned n){
 if(!n||n>255||(p[0]=='.'&&(n==1||(n==2&&p[1]=='.'))))return false;
 for(unsigned i=0;i<n;){
  uint8_t a=p[i++];if(a<128){if(a<32||a==127||a=='/'||a=='\\')return false;continue;}
  unsigned k;uint32_t cp,min;
  if(a>=0xc2&&a<=0xdf){k=1;cp=a&31;min=128;}
  else if(a>=0xe0&&a<=0xef){k=2;cp=a&15;min=2048;}
  else if(a>=0xf0&&a<=0xf4){k=3;cp=a&7;min=65536;}else return false;
  if(k>n-i)return false;
  while(k--){uint8_t c=p[i++];if((c&0xc0)!=0x80)return false;cp=(cp<<6)|(c&63);}
  if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||(cp>=128&&cp<=159))return false;
 }
 return true;
}
static inline bool sty_explorer_decode(const uint8_t *p,size_t n,struct sty_explorer_row *out){
 if(!p||!out||n!=STY_EXPLORER_ROW_BYTES||sty_le32(p)!=1||sty_le32(p+12))return false;
 struct sty_explorer_row r={.kind=sty_le32(p+4),.name_bytes=sty_le32(p+8),
  .id=sty_ex_le64(p+16),.bytes=sty_ex_le64(p+24),.revision=sty_ex_le64(p+32)};
 if(r.id<2||(r.kind!=1&&r.kind!=2)||!sty_ex_name(p+40,r.name_bytes)||
    (r.kind==1?!r.revision:(r.bytes||r.revision)))return false;
 for(unsigned i=0;i<256;++i){if(i>=r.name_bytes&&p[40+i])return false;r.name[i]=p[40+i];}
 *out=r;return true;
}
enum { STY_EXPLORER_VOLUME_BYTES=64 };
struct sty_explorer_volume {uint64_t mount,sequence,volume_bytes,visible_block_bytes,free_bytes,largest_run_bytes;bool writable;};
static inline bool sty_explorer_volume_decode(const uint8_t *p,size_t n,struct sty_explorer_volume *out){
 if(!p||!out||n!=STY_EXPLORER_VOLUME_BYTES||sty_le32(p)!=1||sty_le32(p+4)>1||sty_ex_le64(p+56))return false;
 struct sty_explorer_volume v={.mount=sty_ex_le64(p+8),.sequence=sty_ex_le64(p+16),
 .volume_bytes=sty_ex_le64(p+24),.visible_block_bytes=sty_ex_le64(p+32),
 .free_bytes=sty_ex_le64(p+40),.largest_run_bytes=sty_ex_le64(p+48),.writable=sty_le32(p+4)==1};
 if(!v.mount||!v.sequence||!v.volume_bytes||v.volume_bytes>v.visible_block_bytes||v.free_bytes>v.volume_bytes||
    v.largest_run_bytes>v.free_bytes||((v.volume_bytes|v.visible_block_bytes|v.free_bytes|v.largest_run_bytes)&511u))return false;
 *out=v;return true;
}
/* Live candidate 113. Submit returns request ID in value; take consumes
 * exactly once when ready. A directory grant must be provided by supervisor. */
static inline struct sty_reply sty_explorer_submit(unsigned op,uint64_t ordinal){
 if(op<STY_EX_ROW||op>STY_EX_VOLUME||(op==STY_EX_UP&&ordinal))return (struct sty_reply){STY_F_INVALID,0};
 return sty_call(op==STY_EX_VOLUME?0x144u:0x140u+op-1,ordinal,0,0);
}
static inline struct sty_reply sty_explorer_take(uint64_t id,uint8_t *packet,size_t size){
 if(!id||(!packet)!=(!size)||(size&&size!=STY_EXPLORER_ROW_BYTES&&size!=STY_EXPLORER_VOLUME_BYTES))
  return (struct sty_reply){STY_F_INVALID,0};
 return sty_call(0x143u,id,(uintptr_t)packet,size);
}
#endif
