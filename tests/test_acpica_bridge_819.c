#include <styleos/acpica_bridge_819.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
static void table(uint8_t *p,size_t n,int dsdt){
    memset(p,0,n);memcpy(p,dsdt?"DSDT":"SSDT",4);
    p[4]=(uint8_t)n;p[5]=(uint8_t)(n>>8);
    p[8]=2;p[36]=0x81;
    unsigned sum=0;for(size_t i=0;i<n;i++)sum+=p[i];
    p[9]=(uint8_t)(0u-sum);
}
int main(void){
    uint8_t d[64],s[64];table(d,sizeof d,1);table(s,sizeof s,0);
    struct styleos819_table tabs[2]={{0x100000u,d,sizeof d},{0x110000u,s,sizeof s}};
    struct styleos819_gate g={0};
    assert(!styleos819_prepare(&g,tabs,0,1));
    assert(!styleos819_prepare(&g,tabs,2,0));
    assert(styleos819_prepare(&g,tabs,2,0x819));
    assert(!styleos819_activate(&g,0x818));
    assert(styleos819_activate(&g,0x819));
    assert(!styleos819_activate(&g,0x819));
    const uint8_t *p=AcpiOsMapMemory(0x100000u,4);
    assert(p&&memcmp(p,"DSDT",4)==0);
    assert(!AcpiOsMapMemory(0x100000u,65));
    assert(!AcpiOsMapMemory(0xe0000000u,4));
    assert(!AcpiOsMapMemory(UINT64_MAX,4));
    const uint8_t *q=AcpiOsMapMemory(0x110000u,4);
    assert(q&&memcmp(q,"SSDT",4)==0);
    AcpiOsUnmapMemory((void *)q,4);
    AcpiOsUnmapMemory((void *)p,4);
    uint64_t val=UINT64_C(0xf00dbeefad831234);uint32_t port=0x1faced00;
    STYLEOS819_PCI_ID id={0,0,3,0};
    assert(AcpiOsReadMemory(0xe0000000u,&val,32)==STYLEOS819_AE_ACCESS);
    assert(AcpiOsWriteMemory(0xe0000000u,val,32)==STYLEOS819_AE_ACCESS);
    assert(AcpiOsReadPort(0x3f8u,&port,8)==STYLEOS819_AE_ACCESS);
    assert(AcpiOsWritePort(0x3f8u,port,8)==STYLEOS819_AE_ACCESS);
    assert(AcpiOsReadPciConfiguration(&id,0,&val,32)==STYLEOS819_AE_ACCESS);
    assert(AcpiOsWritePciConfiguration(&id,0,val,32)==STYLEOS819_AE_ACCESS);
    assert(val==UINT64_C(0xf00dbeefad831234)&&port==0x1faced00);
    assert(AcpiOsReadPciConfiguration(&id,4095,&val,32)==STYLEOS819_AE_BAD_PARAMETER);
    assert(AcpiOsReadMemory(0,&val,7)==STYLEOS819_AE_BAD_PARAMETER);
    assert(g.denied_io==6&&g.mapped==2&&g.denied_maps==3);
    assert(!styleos819_dma_grant(0xffffu));
    styleos819_deactivate(&g);
    assert(!AcpiOsMapMemory(0x100000u,4));
    assert(AcpiOsReadMemory(0,&val,32)==STYLEOS819_AE_ACCESS);
    tabs[1].firmware_pa=0x100020u;
    struct styleos819_gate clean={0};
    assert(!styleos819_prepare(&clean,tabs,2,2));
    assert(!clean.sealed&&!clean.count);
    tabs[1].firmware_pa=0x110000u;s[9]++;
    assert(!styleos819_prepare(&clean,tabs,2,2));
    assert(!clean.sealed&&!clean.count);
    s[9]--;assert(styleos819_prepare(&clean,tabs,2,2));
    assert(styleos819_activate(&clean,2));
    d[36]++;
    assert(!AcpiOsMapMemory(0x100000u,4));
    assert(clean.denied_maps==1&&clean.invalid==1);
    styleos819_deactivate(&clean);
    puts("STYLEOS S819 HOST PASS bounded ACPI table maps and ACPICA-style I/O deny");
    return 0;
}
