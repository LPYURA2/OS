#include <styleos/acpi_osl_818.h>
#include <stdint.h>
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|
 ((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int acpi818_open(struct acpi818_gate *g,const uint8_t *p,size_t n,uint32_t generation){
 if(!g||!p||!generation||n<36u||n>ACPI818_MAX_TABLE||rd32(p+4)!=n||
    !((p[0]=='D'&&p[1]=='S'&&p[2]=='D'&&p[3]=='T')||
      (p[0]=='S'&&p[1]=='S'&&p[2]=='D'&&p[3]=='T')))return 0;
 uint8_t checksum=0;
 for(size_t i=0;i<n;i++)checksum=(uint8_t)(checksum+p[i]);
 if(checksum)return 0;
 *g=(struct acpi818_gate){.table=p,.length=(uint32_t)n,
                            .generation=generation,.valid=1};
 return 1;
}
static enum acpi818_status charge(struct acpi818_gate *g){
 if(!g||!g->valid)return ACPI818_INVALID;
 if(g->queries>=ACPI818_MAX_QUERIES)return ACPI818_EXHAUSTED;
 ++g->queries;return ACPI818_OK;
}
enum acpi818_status acpi818_table_read(struct acpi818_gate *g,uint32_t generation,
 size_t offset,unsigned bits,uint64_t *value){
 enum acpi818_status s=charge(g);
 if(s!=ACPI818_OK)return s;
 if(!value||generation!=g->generation||!(bits==8||bits==16||bits==32||bits==64)||
    offset>g->length||((size_t)bits/8u)>g->length-offset){
  ++g->invalid;return ACPI818_INVALID;
 }
 uint64_t result=0;
 for(unsigned i=0;i<bits/8u;i++)result|=(uint64_t)g->table[offset+i]<<(8u*i);
 *value=result;++g->copied_reads;return ACPI818_OK;
}
enum acpi818_status acpi818_region(struct acpi818_gate *g,unsigned space,
 uint64_t address,unsigned bits,int write,uint64_t *value){
 enum acpi818_status s=charge(g);
 if(s!=ACPI818_OK)return s;
 /* Check the request metadata, but intentionally never consume value. */
 if(!value||space>255u||!(bits==8||bits==16||bits==32||bits==64)||
    (write!=0&&write!=1)||address>UINT64_MAX-(bits/8u-1u)){
  ++g->invalid;return ACPI818_INVALID;
 }
 /* All spaces including system memory, system I/O, PCI config, EC,
  * and vendor-specific spaces remain denied. */
 (void)space;(void)address;(void)write;++g->denied;
 return ACPI818_DENIED;
}
int acpi818_dma_grant(const struct acpi818_gate *g,uint16_t requester){
 (void)g;(void)requester;return 0;
}
