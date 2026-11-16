#include "styleos/storage_demo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct builder { unsigned char data[8192];size_t at,struct_end,total; };
static const char names[]="#address-cells\0#size-cells\0bootargs\0compatible\0reg\0status\0";
static void put32(unsigned char *p,uint32_t x)
{p[0]=(unsigned char)(x>>24);p[1]=(unsigned char)(x>>16);p[2]=(unsigned char)(x>>8);p[3]=(unsigned char)x;}
static void u32(struct builder *b,uint32_t x)
{assert(b->at+4<=sizeof b->data);put32(b->data+b->at,x);b->at+=4;}
static void begin(struct builder *b,const char *name)
{
    u32(b,1);size_t n=strlen(name)+1;assert(b->at+n<=sizeof b->data);
    memcpy(b->data+b->at,name,n);b->at=(b->at+n+3)&~(size_t)3;
}
static unsigned off(const char *name)
{
    size_t at=0;while(at<sizeof names-1){if(!strcmp(names+at,name))return (unsigned)at;at+=strlen(names+at)+1;}
    assert(0);return 0;
}
static void prop(struct builder *b,const char *name,const void *p,size_t n)
{u32(b,3);u32(b,(uint32_t)n);u32(b,off(name));assert(b->at+n<=sizeof b->data);memcpy(b->data+b->at,p,n);b->at=(b->at+n+3)&~(size_t)3;}
static void cell(struct builder *b,const char *name,uint32_t x)
{unsigned char bytes[4];put32(bytes,x);prop(b,name,bytes,sizeof bytes);}
static void root(struct builder *b,unsigned address_cells)
{memset(b,0,sizeof *b);b->at=56;begin(b,"");cell(b,"#address-cells",address_cells);cell(b,"#size-cells",2);}
static void chosen(struct builder *b,const char *args)
{begin(b,"chosen");prop(b,"bootargs",args,strlen(args)+1);u32(b,2);}
static void device(struct builder *b,uint32_t address,uint32_t bytes,const char *status)
{
    unsigned char reg[16]={0};put32(reg+4,address);put32(reg+12,bytes);
    begin(b,"virtio_mmio@a003e00");prop(b,"compatible","virtio,mmio",sizeof "virtio,mmio");
    prop(b,"reg",reg,sizeof reg);if(status)prop(b,"status",status,strlen(status)+1);u32(b,2);
}
static void finish(struct builder *b)
{
    u32(b,2);u32(b,9);b->struct_end=b->at;memcpy(b->data+b->at,names,sizeof names);b->at+=sizeof names;b->total=b->at;
    put32(b->data,UINT32_C(0xd00dfeed));put32(b->data+4,(uint32_t)b->total);
    put32(b->data+8,56);put32(b->data+12,(uint32_t)b->struct_end);put32(b->data+16,40);
    put32(b->data+20,17);put32(b->data+24,16);put32(b->data+32,sizeof names);put32(b->data+36,(uint32_t)(b->struct_end-56));
}
static void valid(struct builder *b,const char *args)
{root(b,2);chosen(b,args);device(b,UINT32_C(0x0a003e00),512,0);finish(b);}
static void reject(struct builder *b)
{
    struct storage_boot out,before;memset(&out,0xa5,sizeof out);memcpy(&before,&out,sizeof out);
    assert(storage_fdt_parse(b->data,b->total,&out)!=SF_OK);assert(!memcmp(&out,&before,sizeof out));
}
int main(void)
{
    struct builder b;struct storage_boot out;
    static const char *const phases[]={"init-a","read-a","update-b","read-b","prepare-b","recover-a"};
    for(unsigned n=0;n<6;++n){char args[128];snprintf(args,sizeof args,"console=ttyAMA0 styleos.storage=%s",phases[n]);valid(&b,args);
        assert(storage_fdt_parse(b.data,b.total,&out)==SF_OK);assert(out.phase==(enum storage_phase)(n+1));assert(out.count==1);
        assert(out.mmio[0].base==UINT64_C(0x0a003e00) && out.mmio[0].bytes==512);assert(!strcmp(storage_phase_name(out.phase),phases[n]));}
    for(unsigned n=0;n<5;++n){static const char *const bad[]={"styleos.storage=nope","styleos.storage=","console=ttyAMA0", "styleos.storage=read-a styleos.storage=read-a","styleos.storage=read-a styleos.storage=read-b"};valid(&b,bad[n]);reject(&b);}
    root(&b,1);chosen(&b,"styleos.storage=read-a");device(&b,UINT32_C(0x0a003e00),512,0);finish(&b);reject(&b);
    root(&b,2);chosen(&b,"styleos.storage=read-a");chosen(&b,"styleos.storage=read-a");device(&b,UINT32_C(0x0a003e00),512,0);finish(&b);reject(&b);
    root(&b,2);chosen(&b,"styleos.storage=read-a");device(&b,UINT32_C(0x0a003e00),512,0);device(&b,UINT32_C(0x0a003e00),512,0);finish(&b);reject(&b);
    for(unsigned n=0;n<4;++n){uint32_t bases[]={UINT32_C(0x09000000),UINT32_C(0x0a004000),UINT32_C(0x0a000001),UINT32_C(0xffffffff)};root(&b,2);chosen(&b,"styleos.storage=read-a");device(&b,bases[n],512,0);finish(&b);reject(&b);}
    root(&b,2);chosen(&b,"styleos.storage=read-a");device(&b,UINT32_C(0x0a003e00),4096,0);finish(&b);reject(&b);
    root(&b,2);chosen(&b,"styleos.storage=read-a");device(&b,UINT32_C(0x0a003e00),512,"disabled");finish(&b);reject(&b);
    root(&b,2);chosen(&b,"styleos.storage=read-a");begin(&b,"bus");device(&b,UINT32_C(0x0a003e00),512,0);u32(&b,2);finish(&b);reject(&b);
    valid(&b,"styleos.storage=read-a");b.data[0]^=1;reject(&b);
    valid(&b,"styleos.storage=read-a");put32(b.data+32,UINT32_MAX);reject(&b);
    valid(&b,"styleos.storage=read-a");put32(b.data+12,56);reject(&b);
    valid(&b,"styleos.storage=read-a");put32(b.data+36,4);reject(&b);
    valid(&b,"styleos.storage=read-a");assert(storage_fdt_parse(b.data,b.total-1,&out)!=SF_OK);
    assert(storage_fdt_parse(0,b.total,&out)==SF_ARGUMENT);assert(storage_fdt_parse(b.data,b.total,0)==SF_ARGUMENT);
    puts("storage FDT host PASS: phases, fixed MMIO, malformed bounds/profile, unchanged refusals");
    return 0;
}
