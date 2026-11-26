#include <styleos/stylefs_alloc64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define G4 (UINT64_C(1)<<32)
#define L (UINT64_C(1)<<33)
static void eq(struct sfs64_run r,uint64_t first,uint64_t sectors)
{assert(r.first==first&&r.sectors==sectors);}
static void exhaustive(void)
{
    /* Independent bitmap oracle for all pairs of occupancy masks on
     * eight data sectors, including sharing and fragmented holes. */
    for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b){
        struct sfs64_run x[8],y[8],out[8];size_t nx=0,ny=0;
        for(unsigned i=0;i<8;++i){
            if(a&(1u<<i))x[nx++]=(struct sfs64_run){i+1,1};
            if(b&(1u<<i))y[ny++]=(struct sfs64_run){i+1,1};
        }
        struct sfs64_inventory ia={x,nx},ib={y,ny};
        unsigned used=a|b,free=0,longest=0,streak=0,gaps=0;
        for(unsigned i=0;i<8;++i){
            if(used&(1u<<i))streak=0;
            else {++free;if(!streak)++gaps;if(++streak>longest)longest=streak;}
        }
        struct sfs64_space stats;
        assert(sfs64_space_query(9,1,ia,ib,&stats)==SFS64_ALLOC_OK);
        assert(stats.free_sectors==free&&stats.largest_run==longest&&stats.gaps==gaps);
        for(unsigned need=0;need<=9;++need){
            memset(out,0xa5,sizeof out);struct sfs64_run before[8];memcpy(before,out,sizeof out);
            size_t n=99;
            enum sfs64_alloc_status r=sfs64_allocate(9,1,ia,ib,(uint64_t)need*512,out,8,&n);
            if(need>free){assert(r==SFS64_ALLOC_FULL&&n==99&&!memcmp(out,before,sizeof out));continue;}
            assert(r==SFS64_ALLOC_OK&&n<=8);
            unsigned allocated=0,mask=0,expected=0,left=need;
            for(unsigned i=0;i<8&&left;++i)if(!(used&(1u<<i))){expected|=1u<<i;--left;}
            for(size_t i=0;i<n;++i){
                assert(out[i].first>=1&&out[i].first+out[i].sectors<=9&&out[i].sectors);
                if(i)assert(out[i-1].first+out[i-1].sectors<out[i].first);
                for(uint64_t j=0;j<out[i].sectors;++j){unsigned bit=1u<<(out[i].first+j-1);assert(!(mask&bit));mask|=bit;++allocated;}
            }
            assert(allocated==need&&mask==expected&&!(mask&used));
        }
    }
}
int main(void)
{
    const struct sfs64_run live[]={{10,5},{20,5},{L,100}};
    const struct sfs64_run old[]={{8,4},{15,3},{23,4},{L+50,100}};
    struct sfs64_inventory a={live,3},b={old,4},empty={0,0};
    struct sfs64_run out[8],before[8];size_t n=99;
    const uint64_t cap=L+1000;
    struct sfs64_space stats;
    assert(sfs64_space_query(cap,4,a,b,&stats)==SFS64_ALLOC_OK);
    assert(stats.free_sectors==cap-4-17-150&&stats.largest_run==L-27&&stats.gaps==4);
    memset(out,0xa5,sizeof out);memcpy(before,out,sizeof out);
    assert(sfs64_allocate(cap,4,a,b,G4+1,out,2,&n)==SFS64_ALLOC_OUTPUT&&n==3&&!memcmp(out,before,sizeof out));
    assert(sfs64_allocate(cap,4,a,b,G4+1,0,0,&n)==SFS64_ALLOC_OUTPUT&&n==3);
    assert(sfs64_allocate(cap,4,a,b,G4+1,out,8,&n)==SFS64_ALLOC_OK&&n==3);
    eq(out[0],4,4);eq(out[1],18,2);eq(out[2],27,sfs64_sectors(G4+1)-6);
    uint8_t encoded[112];struct sfs64_map m;
    struct sfs64_file file={9,1,G4+1,123};
    assert(sfs64_map_encode(encoded,sizeof encoded,file,cap,4,out,n));
    assert(sfs64_map_decode(encoded,sizeof encoded,cap,4,&m)&&m.file.bytes==G4+1);
    struct sfs_extent64 e;uint64_t offset;size_t bytes;
    assert(sfs64_map_chunk(&m,G4,1,&e,&offset,&bytes)&&bytes==1&&offset==G4-6*512);
    assert(sfs64_allocate(cap,4,a,b,0,0,0,&n)==SFS64_ALLOC_OK&&n==0);
    struct sfs64_run bad[]={{10,5},{14,2}};
    n=99;memset(out,0xa5,sizeof out);
    assert(sfs64_allocate(cap,4,(struct sfs64_inventory){bad,2},empty,512,out,8,&n)==SFS64_ALLOC_INVALID&&n==99&&!memcmp(out,before,sizeof out));
    bad[1]=(struct sfs64_run){4,1};
    assert(sfs64_allocate(cap,4,(struct sfs64_inventory){bad,2},empty,512,out,8,&n)==SFS64_ALLOC_INVALID);
    bad[0]=(struct sfs64_run){UINT64_MAX-1,2};
    assert(sfs64_allocate(UINT64_MAX,4,(struct sfs64_inventory){bad,1},empty,512,out,8,&n)==SFS64_ALLOC_INVALID);
    const struct sfs64_run full={4,cap-4};
    assert(sfs64_allocate(cap,4,(struct sfs64_inventory){&full,1},empty,1,out,8,&n)==SFS64_ALLOC_FULL);
    assert(sfs64_allocate(UINT64_MAX,UINT64_MAX-2,empty,empty,1024,out,8,&n)==SFS64_ALLOC_OK&&n==1);
    eq(out[0],UINT64_MAX-2,2);
    assert(sfs64_allocate(UINT64_MAX,4,empty,empty,UINT64_MAX,out,8,&n)==SFS64_ALLOC_OK&&n==1);
    eq(out[0],4,sfs64_sectors(UINT64_MAX));
    exhaustive();
    puts("STYLEFS ALLOC64 PASS: 65536 snapshot pairs / 655360 requests, union protection, fragmentation, full disk, 64-bit limits, map roundtrip");
}
