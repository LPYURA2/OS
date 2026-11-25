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
    /* Optical path must use the independently observed, pinned BAR; old
     * terminal contract must still reject it. */
    struct bm857_snapshot optical=base;optical.bar4=0x6061;
    word=0xabcd;assert(!bm857_plan(&optical,root,&word)&&word==0xabcd);
    assert(bm857_plan_at(&optical,root,0x6061,&word)&&word==3);
    optical.command_status=0x02800003;assert(bm857_confirm_at(&optical,root,0x6061));
    assert(!bm857_confirm_at(&optical,root,0x60e1)&&!bm857_confirm(&optical,root));
    for(unsigned b=0;b<4;b++){optical.bm[b]=1;assert(!bm857_confirm_at(&optical,root,0x6061));optical.bm[b]=0;}
    optical.bar4=0x60c1;assert(!bm857_confirm_at(&optical,root,0x60c1));
    positive+=2;negative+=8;
    /* Exhaust the optical command field. Only IO + optional memory/master
     * can pass, and confirmation requires master clear with unchanged IO/MEM. */
    optical=base;optical.bar4=0x6061;
    for(unsigned i=0;i<65536;i++){
      optical.command_status=0x02800000u|i;word=0xabcd;
      bool allowed=i==1||i==3||i==5||i==7;
      assert(bm864_optical_plan(&optical,root,&word)==allowed);
      assert(word==(allowed?(i&3u):0xabcdu));
      assert(bm864_optical_confirm(&optical,root,1)==(i==1));
      assert(bm864_optical_confirm(&optical,root,3)==(i==3));
      positive+=allowed+(i==1)+(i==3);negative+=!allowed+(i!=1)+(i!=3);
    }
    optical.command_status=0x02800005;optical.bm[0]=1;word=0xabcd;
    assert(!bm864_optical_plan(&optical,root,&word)&&word==0xabcd);
    optical.bm[0]=0;optical.command_status^=0x10000;assert(!bm864_optical_plan(&optical,root,&word));negative+=2;
    /* Actual S864 optical guest receipt: primary IRQ latch, secondary
     * direction; neither engine is started or active. Exhaust every byte
     * separately for both planning and confirmation; no errors accepted. */
    optical=base;optical.bar4=0x6061;optical.bm[1]=4;optical.bm[2]=8;
    assert(bm864_optical_plan(&optical,root,&word)&&word==3);
    optical.command_status=0x02800003;assert(bm864_optical_confirm(&optical,root,3));
    assert(!bm857_confirm_at(&optical,root,0x6061));positive+=2;negative++;
    for(unsigned i=0;i<4;i++)for(unsigned v=0;v<256;v++){
      struct bm857_snapshot sample=optical;sample.bm[i]=(uint8_t)v;
      bool ok=v==0||v==((i&1u)?4u:8u);
      assert(bm864_optical_byte(i,(uint8_t)v)==ok);
      assert(bm864_optical_confirm(&sample,root,3)==ok);
      sample.command_status=0x02800007;word=0xabcd;
      assert(bm864_optical_plan(&sample,root,&word)==ok);
      assert(word==(ok?3:0xabcd));positive+=2*ok;negative+=2*!ok;
    }
    assert(!bm864_optical_byte(4,0));negative++;
    printf("BM857 plan/readback: %u positive, %u refusals PASS\n",positive,negative);
}
