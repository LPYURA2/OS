#include "styleos/disk_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"disk-app test line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static _Alignas(4) unsigned char file[DA_FILE_MAX+1];
static _Alignas(4096) unsigned char destination[4096];
static struct da_info info;
static unsigned groups;
static void put32(unsigned char *p,uint32_t v)
{ for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8)); }
/* Independent bitwise fixture checksum, with the checksum field zeroed by the
 * caller; the loader has its own omitted-field implementation. */
static uint32_t crc(const unsigned char *p,size_t n)
{
    uint32_t c=~UINT32_C(0);
    while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=c&1?(c>>1)^UINT32_C(0xedb88320):c>>1;}
    return ~c;
}
static void seal(size_t n)
{ put32(file+28,0);put32(file+28,crc(file,n)); }
static void fixture(uint32_t marker,unsigned exit)
{
    memset(file,0,sizeof file);memcpy(file,"STYAPP1",8);put32(file+8,1);
    file[12]=183;file[14]=32;put32(file+16,28);
    const uint32_t words[]={0xb5000060,0xd2a24021,0xf9400022,
        0xd2800013|((marker&0xffffu)<<5),0xf2a00013|((marker>>16)<<5),
        0xd2800000|(exit<<5),0xd4000041};
    for(unsigned i=0;i<7;++i)put32(file+32+i*4,words[i]);
    seal(60);
}
static void unchanged_failure(size_t n,enum da_result expected)
{
    struct da_info before;memset(&info,0xa5,sizeof info);before=info;
    memset(destination,0xcc,sizeof destination);
    CHECK(da_validate(file,n,&info)==expected);CHECK(!memcmp(&info,&before,sizeof info));
    CHECK(da_copy(file,n,destination,sizeof destination,&info)==expected);
    CHECK(!memcmp(&info,&before,sizeof info));
    for(size_t i=0;i<sizeof destination;++i)CHECK(destination[i]==0xcc);
}
static void header_and_bounds(void)
{
    fixture(0x1818a001,41);CHECK(da_validate(file,60,&info)==DA_OK);
    CHECK(info.code_length==28 && info.entry_offset==0 && info.fault_offset==8 && info.exit_offset==24);
    CHECK(info.marker==0x1818a001 && info.exit_code==41);
    unchanged_failure(0,DA_SIZE);unchanged_failure(31,DA_SIZE);
    unchanged_failure(1025,DA_SIZE);unchanged_failure(SIZE_MAX,DA_ARGUMENT);
    for(unsigned i=0;i<8;++i){fixture(1,2);file[i]^=1;unchanged_failure(60,DA_MAGIC);}
    fixture(1,2);put32(file+8,2);unchanged_failure(60,DA_VERSION);
    fixture(1,2);file[12]=62;unchanged_failure(60,DA_ISA);
    fixture(1,2);file[14]=36;unchanged_failure(60,DA_HEADER);
    fixture(1,2);file[24]=1;unchanged_failure(60,DA_HEADER);
    const uint32_t lengths[]={0,1,27,29,993,UINT32_MAX};
    for(unsigned i=0;i<sizeof lengths/sizeof *lengths;++i){
        fixture(1,2);put32(file+16,lengths[i]);unchanged_failure(60,DA_SIZE);
    }
    fixture(1,2);unchanged_failure(59,DA_SIZE);
    const uint32_t entries[]={1,4,28,UINT32_MAX};
    for(unsigned i=0;i<sizeof entries/sizeof *entries;++i){
        fixture(1,2);put32(file+20,entries[i]);unchanged_failure(60,DA_ENTRY);
    }
    fixture(1,2);put32(file+16,DA_CODE_MAX);seal(1024);unchanged_failure(1024,DA_OPCODE);
    fixture(1,2);file[28]^=1;unchanged_failure(60,DA_CHECKSUM);
    fixture(1,2);file[59]^=1;unchanged_failure(60,DA_CHECKSUM);
    ++groups;
}
static void complete_opcode_grammar(void)
{
    /* Every bit in all seven executable words: only the three declared
     * immediate fields may vary; any other change is rejected before copy. */
    for(unsigned word=0;word<7;++word)for(unsigned bit=0;bit<32;++bit){
        fixture(0x1818a001,41);file[32+word*4+bit/8]^=(unsigned char)(1u<<(bit%8));seal(60);
        if(word>=3 && word<=5 && bit>=5 && bit<=20){
            CHECK(da_validate(file,60,&info)==DA_OK);
            uint32_t marker=0x1818a001,exit_code=41;
            if(word==3)marker^=1u<<(bit-5);
            if(word==4)marker^=1u<<(bit-5+16);
            if(word==5)exit_code^=1u<<(bit-5);
            CHECK(info.marker==marker && info.exit_code==exit_code);
        }else unchanged_failure(60,DA_OPCODE);
    }
    /* Explicit indirect branch, FP, system register access and privileged
     * exception return cannot be smuggled into any of the seven positions. */
    const uint32_t forbidden[]={0xd61f0000,0x1e602800,0xd5381000,0xd69f03e0,0x14000000};
    for(unsigned i=0;i<sizeof forbidden/sizeof *forbidden;++i)
        for(unsigned word=0;word<7;++word){
            fixture(1,2);put32(file+32+word*4,forbidden[i]);seal(60);unchanged_failure(60,DA_OPCODE);
        }
    fixture(UINT32_MAX,UINT16_MAX);CHECK(da_validate(file,60,&info)==DA_OK);
    CHECK(info.marker==UINT32_MAX && info.exit_code==UINT16_MAX);
    fixture(0,0);CHECK(da_validate(file,60,&info)==DA_OK);
    CHECK(info.marker==0 && info.exit_code==0);
    ++groups;
}
static void copy_arguments_and_atomicity(void)
{
    fixture(0x1818b002,73);memset(destination,0xcc,sizeof destination);
    CHECK(da_copy(file,60,destination,sizeof destination,&info)==DA_OK);
    CHECK(!memcmp(destination,file+32,28));CHECK(info.marker==0x1818b002 && info.exit_code==73);
    for(size_t i=28;i<sizeof destination;++i)CHECK(destination[i]==0xcc);
    struct da_info before;memset(&info,0xa5,sizeof info);before=info;
    memset(destination,0xcc,sizeof destination);
    CHECK(da_copy(file,60,destination,27,&info)==DA_CAPACITY);
    CHECK(!memcmp(&info,&before,sizeof info));
    for(size_t i=0;i<sizeof destination;++i)CHECK(destination[i]==0xcc);
    CHECK(da_validate(NULL,60,&info)==DA_ARGUMENT);
    CHECK(da_validate(file,60,NULL)==DA_ARGUMENT);
    CHECK(da_validate(file,60,(struct da_info *)(void *)(destination+1))==DA_ARGUMENT);
    CHECK(da_validate(file,60,(struct da_info *)(uintptr_t)(UINTPTR_MAX-3))==DA_ARGUMENT);
    CHECK(da_copy(file,60,NULL,28,&info)==DA_ARGUMENT);
    CHECK(da_copy(file,60,destination+1,28,&info)==DA_ARGUMENT);
    CHECK(da_copy(file,60,destination,SIZE_MAX,&info)==DA_ARGUMENT);
    CHECK(da_copy(file,60,file,28,&info)==DA_OVERLAP);
    CHECK(da_copy(file,60,file+32,28,&info)==DA_OVERLAP);
    CHECK(da_copy(file,60,destination,4096,(struct da_info *)(void *)destination)==DA_OVERLAP);
    unsigned char backup[sizeof file];memcpy(backup,file,sizeof file);
    CHECK(da_validate(file,60,(struct da_info *)(void *)file)==DA_OVERLAP);
    CHECK(!memcmp(file,backup,sizeof file));
    CHECK(!strcmp(da_result_name(DA_OK),"OK"));CHECK(!strcmp(da_result_name(DA_OPCODE),"OPCODE"));
    CHECK(!strcmp(da_result_name((enum da_result)999),"UNKNOWN"));
    ++groups;
}
int main(void)
{
    header_and_bounds();complete_opcode_grammar();copy_arguments_and_atomicity();
    printf("disk-app host loader: %u groups PASS (header/bounds, all opcode bits, atomic copy)\n",groups);
    return 0;
}
