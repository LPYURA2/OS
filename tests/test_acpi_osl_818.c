#include <styleos/acpi_osl_818.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
static uint8_t blob[64];
static void table(void){
 memset(blob,0,sizeof blob);
 memcpy(blob,"DSDT",4);blob[4]=64;
 unsigned sum=0;for(size_t i=0;i<64;i++)sum+=blob[i];blob[9]=(uint8_t)(0u-sum);
}
int main(void){
 struct acpi818_gate g={0};uint64_t v=UINT64_C(0xdeadbeefcafebabe);
 table();assert(acpi818_open(&g,blob,sizeof blob,0x818));
 assert(acpi818_table_read(&g,0x818,0,32,&v)==ACPI818_OK&&v==0x54445344u);
 assert(acpi818_table_read(&g,0x818,63,16,&v)==ACPI818_INVALID&&v==0x54445344u);
 assert(acpi818_table_read(&g,0x819,0,32,&v)==ACPI818_INVALID&&v==0x54445344u);
 assert(acpi818_table_read(&g,0x818,SIZE_MAX,64,&v)==ACPI818_INVALID);
 assert(acpi818_table_read(&g,0x818,0,24,&v)==ACPI818_INVALID);
 unsigned denied=0;
 for(unsigned sp=0;sp<256;sp++){
  if(g.queries>=ACPI818_MAX_QUERIES)break;
  assert(acpi818_region(&g,sp,UINT64_C(0xe0000000),32,0,&v)==ACPI818_DENIED);
  denied++;assert(v==0x54445344u);
 }
 assert(denied>200&&g.denied==denied&&g.copied_reads==1);
 assert(acpi818_region(&g,ACPI818_SYSTEM_IO,0xcf8,32,1,&v)==ACPI818_EXHAUSTED);
 assert(!acpi818_dma_grant(&g,0));assert(!acpi818_dma_grant(&g,65535));
 struct acpi818_gate old=g;
 table();blob[15]++;
 assert(!acpi818_open(&g,blob,sizeof blob,0x819));
 assert(memcmp(&g,&old,sizeof g)==0); /* open failure never mutates context */
 table();assert(!acpi818_open(&g,blob,sizeof blob,0));
 blob[4]=0;assert(!acpi818_open(&g,blob,sizeof blob,1));
 puts("STYLEOS S818 HOST PASS checksummed table snapshot, stale generation, bounds, budget, operationregion all denied, DMA denied");
 return 0;
}
