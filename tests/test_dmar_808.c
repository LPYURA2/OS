#include <styleos/dmar_808.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static void ck(int ok,const char *name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}}
static void w16(uint8_t *p,uint16_t v){p[0]=v&255;p[1]=v>>8;}
static void w32(uint8_t *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void w64(uint8_t *p,uint64_t v){w32(p,(uint32_t)v);w32(p+4,(uint32_t)(v>>32));}
static void fix(uint8_t *p,size_t n){p[9]=0;uint8_t s=0;for(size_t i=0;i<n;i++)s=(uint8_t)(s+p[i]);p[9]=(uint8_t)(0-s);}
int main(void){
 uint8_t t[128]={0};memcpy(t,"DMAR",4);w32(t+4,64);t[8]=1;t[36]=39;
 w16(t+48,0);w16(t+50,16);t[52]=1;w64(t+56,UINT64_C(0xfed90000));fix(t,64);
 struct dmar808_info out={0};ck(dmar808_parse(t,64,&out)&&out.drhd_count==1&&out.drhd[0].base==UINT64_C(0xfed90000),"basic drhd");
 ck(!dmar808_parse(t,63,&out),"truncation");t[9]^=1;ck(!dmar808_parse(t,64,&out),"checksum");t[9]^=1;
 t[52]=2;fix(t,64);ck(!dmar808_parse(t,64,&out),"drhd flag");t[52]=1;
 w64(t+56,0);fix(t,64);ck(!dmar808_parse(t,64,&out),"drhd zero base");w64(t+56,UINT64_C(0xfed90000));
 w16(t+50,15);fix(t,64);ck(!dmar808_parse(t,64,&out),"invalid drhd length");w16(t+50,16);
 t[36]=31;fix(t,64);ck(!dmar808_parse(t,64,&out),"invalid host width");t[36]=39;
 w32(t+4,88);w16(t+64,1);w16(t+66,24);w64(t+72,0x100000);w64(t+80,0x101fff);fix(t,88);
 ck(!dmar808_parse(t,88,&out),"rmrr without scopes");
 w16(t+66,32);w32(t+4,96);t[88]=1;t[89]=8;t[92]=0;t[93]=0;t[94]=1;t[95]=0;fix(t,96);
 ck(dmar808_parse(t,96,&out)&&out.rmrr_count==1&&out.rmrr[0].scope_count==1,"rmrr with scope");
 t[89]=7;fix(t,96);ck(!dmar808_parse(t,96,&out),"invalid scope length");
 printf("S808 DMAR host ASAN/UBSAN PASS\n");return 0;
}
