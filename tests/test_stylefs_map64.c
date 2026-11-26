#include <styleos/stylefs_map64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define GIB4 (UINT64_C(1)<<32)
#define HIGH (UINT64_C(1)<<34)
#define CAP (UINT64_C(1)<<35)
#define DATA 4096u
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));}
static void put64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;++i)p[i]=(uint8_t)(v>>(8*i));}
static void seal(uint8_t *p,size_t n){
    uint32_t c=UINT32_MAX;put32(p+56,0);
    for(size_t i=0;i<n;++i){c^=p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}
    put32(p+56,~c);
}
static void rejected(uint8_t *p,size_t n,struct sfs64_map *m){
    uint64_t id=m->file.id;assert(!sfs64_map_decode(p,n,CAP,DATA,m)&&m->file.id==id);
}
int main(void){
    struct sfs64_file file={7,9,GIB4+531,UINT32_C(0x12345678)};
    const struct sfs64_run runs[]={{HIGH,GIB4/512},{DATA,2}};
    uint8_t good[96],bad[96];struct sfs64_map m={0};
    assert(sfs64_map_length(2)==sizeof good&&!sfs64_map_length(SIZE_MAX));
    assert(sfs64_map_encode(good,sizeof good,file,CAP,DATA,runs,2));
    assert(!memcmp(good,"SF64MAP1",8));
    assert(good[8]==19&&good[9]==2&&good[12]==1); /* 4GiB + 531 LE */
    assert(sfs64_map_decode(good,sizeof good,CAP,DATA,&m));
    assert(m.file.id==7&&m.file.revision==9&&m.file.bytes==file.bytes&&m.file.checksum==file.checksum&&m.count==2);
    struct sfs_extent64 e;uint64_t local;size_t n;
    assert(sfs64_map_chunk(&m,GIB4-3,8,&e,&local,&n)&&e.first==HIGH&&e.bytes==GIB4&&local==GIB4-3&&n==3);
    assert(sfs64_map_chunk(&m,GIB4,5,&e,&local,&n)&&e.first==DATA&&e.bytes==531&&local==0&&n==5);
    assert(sfs64_map_chunk(&m,1,SFS64_WINDOW_BYTES,&e,&local,&n)&&n==SFS64_WINDOW_BYTES-1);
    assert(sfs64_map_chunk(&m,file.bytes-1,1,&e,&local,&n)&&local==530&&n==1);
    assert(!sfs64_map_chunk(&m,file.bytes,1,&e,&local,&n));
    assert(!sfs64_map_chunk(&m,file.bytes-1,2,&e,&local,&n));
    assert(!sfs64_map_decode(good,sizeof good,CAP-1,DATA,&m));
    assert(!sfs64_map_decode(good,sizeof good,CAP,DATA+1,&m));
    for(size_t i=0;i<sizeof good;++i){memcpy(bad,good,sizeof bad);bad[i]^=1;rejected(bad,sizeof bad,&m);}
    for(size_t i=0;i<sizeof good;++i)rejected(good,i,&m);
    /* Recompute CRC after semantic corruption: CRC alone is not validation. */
    memcpy(bad,good,sizeof bad);put64(bad+80,HIGH+1);seal(bad,sizeof bad);rejected(bad,sizeof bad,&m);
    memcpy(bad,good,sizeof bad);put64(bad+80,DATA-1);seal(bad,sizeof bad);rejected(bad,sizeof bad,&m);
    memcpy(bad,good,sizeof bad);put64(bad+80,CAP-1);seal(bad,sizeof bad);rejected(bad,sizeof bad,&m);
    memcpy(bad,good,sizeof bad);put64(bad+80,UINT64_MAX);seal(bad,sizeof bad);rejected(bad,sizeof bad,&m);
    memcpy(bad,good,sizeof bad);put64(bad+88,0);seal(bad,sizeof bad);rejected(bad,sizeof bad,&m);
    memcpy(bad,good,sizeof bad);put64(bad+88,3);seal(bad,sizeof bad);rejected(bad,sizeof bad,&m);
    memcpy(bad,good,sizeof bad);put32(bad+48,UINT32_MAX);seal(bad,sizeof bad);rejected(bad,sizeof bad,&m);
    memcpy(bad,good,sizeof bad);put64(bad+32,0);seal(bad,sizeof bad);rejected(bad,sizeof bad,&m);
    memcpy(bad,good,sizeof bad);put64(bad+40,0);seal(bad,sizeof bad);rejected(bad,sizeof bad,&m);
    memcpy(bad,good,sizeof bad);put32(bad+60,1);seal(bad,sizeof bad);rejected(bad,sizeof bad,&m);
    memset(bad,0x5a,sizeof bad);struct sfs64_run overlap[]={{HIGH,GIB4/512},{HIGH+1,2}};
    assert(!sfs64_map_encode(bad,sizeof bad,file,CAP,DATA,overlap,2));
    for(size_t i=0;i<sizeof bad;++i)assert(bad[i]==0x5a);
    uint8_t empty[64];struct sfs64_file zero={8,1,0,0};
    assert(sfs64_map_encode(empty,sizeof empty,zero,CAP,DATA,0,0)&&sfs64_map_decode(empty,sizeof empty,CAP,DATA,&m));
    assert(!sfs64_map_chunk(&m,0,1,&e,&local,&n));
    struct sfs64_file huge={9,1,UINT64_MAX,0};
    struct sfs64_run last={DATA,sfs64_sectors(UINT64_MAX)};
    uint8_t wide[80];uint64_t cap=DATA+last.sectors;
    assert(sfs64_map_encode(wide,sizeof wide,huge,cap,DATA,&last,1)&&sfs64_map_decode(wide,sizeof wide,cap,DATA,&m));
    assert(sfs64_map_chunk(&m,UINT64_MAX-1,1,&e,&local,&n)&&e.bytes==UINT64_MAX&&local==UINT64_MAX-1&&n==1);
    puts("STYLEFS WIDE MAP PASS: LE/CRC, 64-bit sizes, fragmented physical runs, boundary windows, semantic corruption, empty and maximum offsets");
}
