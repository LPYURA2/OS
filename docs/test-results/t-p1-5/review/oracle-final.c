#define main reviewed_fixture_tests_main
#include "/workspace/OS/tests/test_ownership.c"
#undef main
static unsigned oracle_seed=0x69117u;
static unsigned oracle_random(void) { oracle_seed^=oracle_seed<<13;oracle_seed^=oracle_seed>>17;oracle_seed^=oracle_seed<<5;return oracle_seed; }
static int intersected(uint64_t a,uint64_t b,struct boot_range r) {return a<r.end && r.start<b;}
int main(void) {
    unsigned comparisons=0;
    for(unsigned arch=1;arch<=2;arch++) for(unsigned run=0;run<250;run++) {
        fixture(arch);
        unsigned count=oracle_random()%75;
        for(unsigned j=0;j<count;j++) {
            uint64_t start=0x10000u+oracle_random()%0xf00000u;
            uint64_t bytes=1u+oracle_random()%0x1ffffu;
            reserve(start,bytes);
        }
        assert(bm_normalize(&map)==BM_OK);
        struct op_input unchanged=in;struct boot_memory_map unchanged_map=map;
        assert(op_build(&in,&out,&scratch)==OP_OK && out.valid && !out.granted);
        assert(memcmp(&unchanged,&in,sizeof in)==0 && memcmp(&unchanged_map,&map,sizeof map)==0);
        for(uint64_t a=0x10000;a<0x1000000;a+=4096) {
            uint64_t b=a+4096;unsigned expected_protected=0,actual_protected=0,actual_review=0;
            for(size_t j=0;j<map.nr_reserved;j++) expected_protected|=intersected(a,b,map.reserved[j]);
            for(size_t j=0;j<in.nr_claims;j++) {
                struct boot_range r={in.claims[j].pa,in.claims[j].pa+in.claims[j].bytes};
                expected_protected|=intersected(a,b,r);
            }
            for(size_t j=0;j<out.nr_protected;j++) actual_protected+=intersected(a,b,out.protected_ranges[j]);
            for(size_t j=0;j<out.nr_review;j++) actual_review+=intersected(a,b,out.review[j]);
            assert(actual_protected==expected_protected && actual_review==!expected_protected);
            ++comparisons;
        }
        assert(out.protected_pages==pages(out.protected_ranges,out.nr_protected));
        assert(out.review_pages==pages(out.review,out.nr_review));
    }
    reviewed_fixture_tests_main();
    printf("Independent final bitmap oracle PASS (%u per-page comparisons; 500 randomized plans); full repository core suite also passed\n",comparisons);
}
