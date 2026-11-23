#include <styleos/acpica_bridge_819.h>
#include <stdint.h>
#include <stddef.h>
/* No hardware access is present in this translation unit. */
static struct styleos819_gate *current;
static uint32_t read32(const uint8_t *p){
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int table_valid(const struct styleos819_table *t){
    if(!t||!t->copy||t->length<36||t->length>STYLEOS819_MAX_LENGTH||
       !t->firmware_pa||t->firmware_pa>UINT64_MAX-t->length||
       (t->firmware_pa&3u)||read32(t->copy+4)!=t->length)return 0;
    int dsdt=t->copy[0]=='D'&&t->copy[1]=='S'&&t->copy[2]=='D'&&t->copy[3]=='T';
    int ssdt=t->copy[0]=='S'&&t->copy[1]=='S'&&t->copy[2]=='D'&&t->copy[3]=='T';
    if(!dsdt&&!ssdt)return 0;
    uint8_t check=0;
    for(uint32_t i=0;i<t->length;i++)check=(uint8_t)(check+t->copy[i]);
    return check==0;
}
int styleos819_prepare(struct styleos819_gate *gate,
                      const struct styleos819_table *tables,size_t count,
                      uint32_t generation){
    if(!gate||!tables||!generation||!count||count>STYLEOS819_MAX_TABLES||
       current==gate)return 0;
    unsigned dsdt=0;
    for(size_t i=0;i<count;i++){
        if(!table_valid(&tables[i]))return 0;
        if(tables[i].copy[0]=='D')dsdt++;
        for(size_t j=0;j<i;j++){
            uint64_t a=tables[i].firmware_pa,b=tables[j].firmware_pa;
            if(a<b+tables[j].length&&b<a+tables[i].length)return 0;
        }
    }
    if(dsdt!=1)return 0;
    struct styleos819_gate next={0};
    for(size_t i=0;i<count;i++)next.tables[i]=tables[i];
    next.generation=generation;next.count=(uint16_t)count;next.sealed=1;
    *gate=next;
    return 1;
}
int styleos819_activate(struct styleos819_gate *gate,uint32_t generation){
    if(!gate||!gate->sealed||!generation||gate->generation!=generation||
       current)return 0;
    current=gate;return 1;
}
void styleos819_deactivate(struct styleos819_gate *gate){
    if(current!=gate)return;
    current=0;
    for(unsigned i=0;i<STYLEOS819_MAX_MAPPINGS;i++)gate->maps[i].live=0;
}
/* Called only by a future ACPICA engine, not yet by one in this stage. */
void *AcpiOsMapMemory(uint64_t address,size_t length){
    struct styleos819_gate *g=current;
    if(!g)return 0;
    if(g->queries>=STYLEOS819_MAX_REQUESTS){g->denied_maps++;return 0;}
    g->queries++;
    if(!length||address>UINT64_MAX-length){g->invalid++;g->denied_maps++;return 0;}
    for(unsigned i=0;i<g->count;i++){
        const struct styleos819_table *t=&g->tables[i];
        if(address<t->firmware_pa)continue;
        uint64_t off=address-t->firmware_pa;
        if(off>t->length||length>t->length-off)continue;
        /* Reject mutated firmware copies before exposing one to ACPICA. */
        if(!table_valid(t)){g->invalid++;g->denied_maps++;return 0;}
        for(unsigned k=0;k<STYLEOS819_MAX_MAPPINGS;k++)if(!g->maps[k].live){
            const void *p=t->copy+(size_t)off;
            g->maps[k]=(struct styleos819_map){.ptr=p,.address=address,
                .length=length,.live=1};g->mapped++;return (void *)p;
        }
        g->denied_maps++;return 0;
    }
    g->denied_maps++;return 0;
}
void AcpiOsUnmapMemory(void *logical,size_t length){
    struct styleos819_gate *g=current;
    if(!g)return;
    for(unsigned k=0;k<STYLEOS819_MAX_MAPPINGS;k++){
        if(g->maps[k].live&&g->maps[k].ptr==logical&&g->maps[k].length==length){
            g->maps[k].live=0;return;
        }
    }
    g->invalid++;
}
static STYLEOS819_ACPI_STATUS deny_hw(uint32_t width,int valid){
    struct styleos819_gate *g=current;
    if(!g)return STYLEOS819_AE_ACCESS;
    if(g->queries>=STYLEOS819_MAX_REQUESTS)return STYLEOS819_AE_LIMIT;
    g->queries++;
    if(!valid||!(width==8||width==16||width==32||width==64)){
        g->invalid++;return STYLEOS819_AE_BAD_PARAMETER;
    }
    g->denied_io++;return STYLEOS819_AE_ACCESS;
}
STYLEOS819_ACPI_STATUS AcpiOsReadMemory(uint64_t addr,uint64_t *value,uint32_t width){
    return deny_hw(width,value&&addr<=UINT64_MAX-((uint64_t)width/8u));
}
STYLEOS819_ACPI_STATUS AcpiOsWriteMemory(uint64_t addr,uint64_t value,uint32_t width){
    (void)value;return deny_hw(width,addr<=UINT64_MAX-((uint64_t)width/8u));
}
STYLEOS819_ACPI_STATUS AcpiOsReadPort(uint64_t addr,uint32_t *value,uint32_t width){
    return deny_hw(width,value&&width<=32&&addr<=0xffffu&&addr+(width/8u)<=65536u);
}
STYLEOS819_ACPI_STATUS AcpiOsWritePort(uint64_t addr,uint32_t value,uint32_t width){
    (void)value;return deny_hw(width,width<=32&&addr<=0xffffu&&addr+(width/8u)<=65536u);
}
STYLEOS819_ACPI_STATUS AcpiOsReadPciConfiguration(STYLEOS819_PCI_ID *id,
    uint32_t reg,uint64_t *value,uint32_t width){
    return deny_hw(width,id&&value&&id->Device<32&&id->Function<8&&
                   reg<4096&&width/8u<=4096u-reg);
}
STYLEOS819_ACPI_STATUS AcpiOsWritePciConfiguration(STYLEOS819_PCI_ID *id,
    uint32_t reg,uint64_t value,uint32_t width){
    (void)value;return deny_hw(width,id&&id->Device<32&&id->Function<8&&
                   reg<4096&&width/8u<=4096u-reg);
}
int styleos819_dma_grant(uint16_t requester){(void)requester;return 0;}
