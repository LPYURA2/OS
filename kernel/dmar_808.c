#include <styleos/dmar_808.h>
#include <stdint.h>
#include <stddef.h>
static uint16_t r16(const uint8_t *p){return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8));}
static uint32_t r32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t r64(const uint8_t *p){return r32(p)|((uint64_t)r32(p+4)<<32);}
static int checksum(const uint8_t *p,size_t n){uint8_t v=0;for(size_t i=0;i<n;i++)v=(uint8_t)(v+p[i]);return v==0;}
/* Each Device Scope: 6-byte prefix + one or more PCI path pairs. */
static int scopes(const uint8_t *p,size_t len,uint16_t *out){
 uint16_t n=0;size_t off=0;
 while(off<len){
  if(len-off<6)return 0;
  uint8_t type=p[off],size=p[off+1];
  if(type<1||type>5||size<8||size>len-off||((size-6)&1u))return 0;
  /* Types and PCI paths retained only as scope-count inventory. */
  if(++n>64)return 0;
  off+=size;
 }
 *out=n;return 1;
}
int dmar808_parse(const uint8_t *p,size_t len,struct dmar808_info *out){
 if(!p||!out||len<48||len>DMAR808_MAX_TABLE)return 0;
 if(p[0]!='D'||p[1]!='M'||p[2]!='A'||p[3]!='R'||r32(p+4)!=len||!checksum(p,len))return 0;
 struct dmar808_info x={0};x.length=(uint32_t)len;x.revision=p[8];
 x.host_address_width=p[36];x.flags=p[37];
 if(x.host_address_width<32||x.host_address_width>63)return 0;
 if((x.flags&~3u)!=0)return 0;
 for(size_t off=48;off<len;){
  if(len-off<4)return 0;
  uint16_t type=r16(p+off),size=r16(p+off+2);
  if(size<4||size>len-off)return 0;
  const uint8_t *v=p+off;uint16_t count=0;
  if(type==0){
   if(size<16||x.drhd_count==DMAR808_MAX_DRHD)return 0;
   uint8_t flags=v[4];uint16_t seg=r16(v+6);uint64_t base=r64(v+8);
   if((flags&~1u)||seg!=0||!base||(base&4095u)||base>UINT64_MAX-4095u)return 0;
   if(!scopes(v+16,size-16,&count))return 0;
   if(!(flags&1u)&&count==0)return 0;
   for(uint16_t j=0;j<x.drhd_count;j++)if(x.drhd[j].base==base)return 0;
   x.drhd[x.drhd_count++]=(struct dmar808_drhd){seg,count,flags,base};
  }else if(type==1){
   if(size<24||x.rmrr_count==DMAR808_MAX_RMRR)return 0;
   uint16_t seg=r16(v+6);uint64_t base=r64(v+8),limit=r64(v+16);
   if(seg!=0||base>limit||!base||limit==UINT64_MAX)return 0;
   if(!scopes(v+24,size-24,&count)||!count)return 0;
   for(uint16_t j=0;j<x.rmrr_count;j++)if(!(limit<x.rmrr[j].base||base>x.rmrr[j].limit_inclusive))return 0;
   x.rmrr[x.rmrr_count++]=(struct dmar808_rmrr){seg,count,base,limit};
  }else{
   /* Unknown/unsupported units remain unresolved. Report, never authorize DMA. */
   if(++x.unknown_count>64)return 0;
  }
  off+=size;
 }
 if(!x.drhd_count)return 0;
 *out=x;return 1;
}
