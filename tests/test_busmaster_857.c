#include <styleos/busmaster_857.h>
#include <assert.h>
#include <stdio.h>
int main(void) {
    const uint64_t root=0x6064000;
    struct bm857_snapshot base={root,0x70108086,0x01018000,0x02800007,0x60e1,{0,0,0,0}};
    unsigned positive=0, negative=0;
    for (unsigned i=0;i<65536;i++) {
        struct bm857_snapshot s=base; s.command_status=0x02800000u|i;
        uint16_t word=0xabcd;
        assert(bm857_plan(&s,root,&word)==(i==7));
        assert(word==(i==7?3:0xabcd));
        assert(bm857_confirm(&s,root)==(i==3));
        positive+=(i==7)+(i==3); negative+=(i!=7)+(i!=3);
    }
    for (unsigned b=0;b<4;b++) for(unsigned v=1;v<256;v++) {
        struct bm857_snapshot s=base;s.bm[b]=(uint8_t)v;uint16_t word=0xabcd;
        assert(!bm857_plan(&s,root,&word)&&word==0xabcd);s.command_status=0x02800003;
        assert(!bm857_confirm(&s,root));negative+=2;
    }
    for(unsigned bit=0;bit<32;bit++) for(unsigned which=0;which<4;which++) {
        struct bm857_snapshot s=base;uint32_t mask=UINT32_C(1)<<bit;
        if(which==0)s.id^=mask;else if(which==1)s.class_revision^=mask;else if(which==2)s.bar4^=mask;else {if(bit<16)continue;s.command_status^=mask;}
        uint16_t word=0xabcd;assert(!bm857_plan(&s,root,&word)&&word==0xabcd);s.command_status^=4;
        assert(!bm857_confirm(&s,root));negative+=2;
    }
    for(unsigned bit=0;bit<64;bit++) {
        struct bm857_snapshot s=base;s.root^=UINT64_C(1)<<bit;uint16_t word=0xabcd;
        assert(!bm857_plan(&s,root,&word)&&word==0xabcd);s.command_status=0x02800003;assert(!bm857_confirm(&s,root));negative+=2;
    }
    uint16_t word=0xabcd;assert(!bm857_plan(NULL,root,&word)&&word==0xabcd);assert(!bm857_plan(&base,root,NULL));assert(!bm857_confirm(NULL,root));assert(!bm857_plan(&base,0,&word));negative+=4;
    printf("BM857 plan/readback: %u positive, %u refusals PASS\n",positive,negative);
}
