#include <styleos/ecam_quarantine_827.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define A UINT64_C(0xe0000000)
#define B UINT64_C(0xf0000000)
#define PAGE UINT64_C(4096)
#define STRIDE 48u
static uint8_t mcfg[4096], drac[28], map[STRIDE*257];
static struct ecam827_report out,saved;
static struct ecam827_work work;
static size_t mn;
static unsigned tests;
static void wr(uint8_t*p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static void desc(unsigned i,unsigned t,uint64_t lo,uint64_t hi,uint64_t attr){
    uint8_t*p=map+i*STRIDE;memset(p,0,STRIDE);wr(p,t,4);wr(p+8,lo,8);wr(p+24,(hi-lo)/PAGE,8);wr(p+32,attr,8);
}
static int run(size_t n){return ecam827_quarantine(mcfg,mn,drac,sizeof drac,map,n*STRIDE,STRIDE,1,&out,&work);}
static void ok(size_t n,uint64_t exp,uint64_t gap){
    assert(run(n));assert(out.ecam.start==A&&out.ecam.end==B);
    assert(out.explicit_pages==exp&&out.gap_pages==gap);
    assert(out.diagnostic.nr_candidates==1&&out.diagnostic.candidates[0].start==0x100000&&out.diagnostic.candidates[0].end==0x200000);
    assert(out.diagnostic.nr_reserved>=1);int hit=0;
    for(size_t i=0;i<out.diagnostic.nr_reserved;i++)if(out.diagnostic.reserved[i].start<=A&&out.diagnostic.reserved[i].end>=B)hit=1;
    assert(hit);tests++;
}
static void bad(size_t n){memset(&out,0xa5,sizeof out);saved=out;assert(!run(n));assert(!memcmp(&out,&saved,sizeof out));tests++;}
static void inputs(const char *mp,const char*dp){
    FILE*f=fopen(mp,"rb");assert(f);mn=fread(mcfg,1,sizeof mcfg,f);assert(feof(f));fclose(f);
    f=fopen(dp,"rb");assert(f);assert(fread(drac,1,28,f)==28&&fgetc(f)==EOF);fclose(f);
}
static void seal(void){unsigned sum=0;mcfg[9]=0;for(size_t i=0;i<mn;i++)sum+=mcfg[i];mcfg[9]=(uint8_t)(0u-sum);}
int main(int argc,char**argv){
    assert(argc==3);inputs(argv[1],argv[2]);
    desc(0,7,0x100000,0x200000,8);ok(1,0,65536);
    desc(1,11,A,B,0);ok(2,65536,0);
    desc(1,0,A,B,0);ok(2,65536,0);
    desc(1,0,A,A+PAGE,0);desc(2,11,A+PAGE,B,0);ok(3,65536,0);
    desc(1,11,A+PAGE,B-PAGE,0);ok(2,65534,2);
    desc(1,11,B,B+PAGE,0);ok(2,0,65536);
    desc(1,11,A-PAGE,A,0);ok(2,0,65536);
    desc(1,7,A,B,8);bad(2);
    desc(1,2,A,A+PAGE,8);bad(2);
    desc(1,5,A,A+PAGE,0);bad(2);
    desc(1,11,A,B,UINT64_C(1)<<63);bad(2);
    desc(1,0,A,B,UINT64_C(1)<<63);bad(2);
    desc(1,0x80000000,A,B,0);bad(2);
    desc(1,12,A,B,0);bad(2);
    desc(1,11,A,B,0);desc(2,0,A,A+PAGE,0);bad(3);
    desc(1,11,A+1,A+1+PAGE,0);bad(2);
    desc(1,11,A,A,0);bad(2);
    desc(1,11,UINT64_MAX-PAGE+1,0,0);wr(map+STRIDE+24,2,8);bad(2);
    desc(1,11,A,B,0);mcfg[9]^=1;bad(2);mcfg[9]^=1;
    mcfg[52]=1;seal();bad(2);mcfg[52]=0;seal();
    drac[5]^=1;bad(2);drac[5]^=1;
    drac[22]^=1;bad(2);drac[22]^=1;
    drac[27]=1;bad(2);drac[27]=0;
    desc(0,0,0x100000,0x200000,0);bad(2);desc(0,7,0x100000,0x200000,8);
    memset(&out,0xa5,sizeof out);saved=out;
    assert(!ecam827_quarantine(mcfg,mn,drac,28,map,STRIDE+1,STRIDE,1,&out,&work));assert(!memcmp(&out,&saved,sizeof out));tests++;
    assert(!ecam827_quarantine(mcfg,mn,drac,28,map,STRIDE,STRIDE,2,&out,&work));assert(!memcmp(&out,&saved,sizeof out));tests++;
    assert(!ecam827_quarantine(mcfg,mn,drac,28,map,STRIDE,STRIDE,1,(struct ecam827_report*)&work,&work));tests++;
    assert(!ecam827_quarantine(mcfg,mn,drac,28,&work,STRIDE,STRIDE,1,&out,&work));assert(!memcmp(&out,&saved,sizeof out));tests++;
    assert(!ecam827_quarantine(mcfg,mn,drac,28,map,STRIDE*257,STRIDE,1,&out,&work));assert(!memcmp(&out,&saved,sizeof out));tests++;
    printf("S827 preflight %u/%u PASS; prior MCFG/DRAC bytes + synthetic EFI map, NOT hardware or ownership\n",tests,tests);
}
