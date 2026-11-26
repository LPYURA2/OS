#include <styleos/stylefs_extent64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define GIB4 (UINT64_C(1)<<32)
#define START (UINT64_C(1)<<33)
#define CAPACITY (UINT64_C(1)<<34)
enum { CELLS=8 };
struct cell {uint64_t lba;bool used;uint8_t bytes[512];};
struct disk {struct cell cells[CELLS];unsigned reads,writes,flushes;uint64_t fail_read,fail_write;bool fail_flush;};
static struct cell *find(struct disk *d,uint64_t lba,bool create)
{
    struct cell *free_cell=0;
    for(unsigned i=0;i<CELLS;++i){
        struct cell *c=&d->cells[i];
        if(c->used&&c->lba==lba)return c;
        if(!c->used&&!free_cell)free_cell=c;
    }
    if(create&&free_cell){free_cell->used=true;free_cell->lba=lba;return free_cell;}
    return 0;
}
static bool rd(void *p,uint64_t lba,uint8_t out[512])
{
    struct disk *d=p;++d->reads;
    if(lba==d->fail_read||lba>=CAPACITY)return false;
    struct cell *c=find(d,lba,false);
    if(c)memcpy(out,c->bytes,512);else memset(out,0,512);
    return true;
}
static bool wr(void *p,uint64_t lba,const uint8_t in[512])
{
    struct disk *d=p;++d->writes;
    if(lba==d->fail_write||lba>=CAPACITY)return false;
    struct cell *c=find(d,lba,true);if(!c)return false;
    memcpy(c->bytes,in,512);return true;
}
static bool fl(void *p){struct disk *d=p;++d->flushes;return !d->fail_flush;}
static unsigned calls(const struct disk *d){return d->reads+d->writes+d->flushes;}
int main(void)
{
    struct bs_service b={0};struct disk d={0},peer={0};
    struct bs_backend backend={&d,1,CAPACITY,true,false,rd,wr,fl};
    struct bs_backend other={&peer,2,CAPACITY,false,true,rd,0,0};
    uint64_t gen,pg,token,ro;
    assert(bs_attach(&b,&backend,&gen)==BS_OK&&bs_attach(&b,&other,&pg)==BS_OK);
    const uint64_t count=UINT64_C(1)<<25;
    assert(bs_grant_range(&b,gen,100,START,count,BS_READ|BS_WRITE,&token)==BS_OK);
    assert(bs_grant_range(&b,pg,200,START,count,BS_READ,&ro)==BS_OK);
    struct sfs_extent64 e={17,GIB4+531};
    assert(sfs64_sectors(UINT64_MAX)==(UINT64_C(1)<<55));
    assert(sfs64_extent_fits(e,count));
    assert(sfs64_extent_fits((struct sfs_extent64){UINT64_MAX-1,512},UINT64_MAX));
    assert(!sfs64_extent_fits((struct sfs_extent64){UINT64_MAX-1,513},UINT64_MAX));
    assert(!sfs64_extent_fits((struct sfs_extent64){1,0},count));
    struct sfs_window64 window={0};
    assert(sfs64_window(e,count,GIB4-1,2,&window)&&window.offset==511&&window.bytes==1);
    assert(window.sector==17+GIB4/512-1);
    assert(sfs64_window(e,count,GIB4,1,&window)&&window.offset==0&&window.sector==17+GIB4/512);
    assert(!sfs64_window(e,count,UINT64_MAX,1,&window));
    uint8_t input[1043],output[1043];for(unsigned i=0;i<sizeof input;++i)input[i]=(uint8_t)(i^(i>>3));
    size_t done=99;unsigned before=calls(&d);
    assert(sfs64_stage_window(&b,100,token,e,GIB4-511,input,512,&done)==BS_RANGE&&done==0);
    assert(sfs64_stage_window(&b,100,token,e,GIB4-512,input,513,&done)==BS_RANGE&&done==0);
    assert(sfs64_stage_window(&b,101,token,e,GIB4-512,input,sizeof input,&done)==BS_DENIED&&done==0);
    assert(sfs64_stage_window(&b,200,ro,e,GIB4-512,input,sizeof input,&done)==BS_DENIED&&done==0);
    assert(sfs64_read_window(&b,100,token,e,e.bytes,output,1,&done)==BS_RANGE&&done==0);
    assert(sfs64_read_window(&b,100,token,(struct sfs_extent64){count-1,1024},0,output,1,&done)==BS_RANGE);
    assert(calls(&d)==before&&!calls(&peer));
    uint8_t bounded[SFS64_WINDOW_BYTES];
    assert(sfs64_read_window(&b,100,token,e,1,bounded,sizeof bounded,&done)==BS_RANGE&&done==0);
    assert(calls(&d)==before);
    assert(sfs64_stage_window(&b,100,token,e,GIB4-512,input,sizeof input,&done)==BS_OK&&done==sizeof input);
    uint64_t first=START+17+GIB4/512-1;
    assert(d.writes==3&&d.flushes==3);
    for(unsigned i=0;i<3;++i)assert(find(&d,first+i,false));
    struct cell *tail=find(&d,first+2,false);
    for(unsigned i=19;i<512;++i)assert(tail->bytes[i]==0);
    assert(sfs64_read_window(&b,100,token,e,GIB4-512,output,sizeof output,&done)==BS_OK&&done==sizeof output&&!memcmp(input,output,sizeof input));
    assert(sfs64_read_window(&b,100,token,e,GIB4-3,output,8,&done)==BS_OK&&done==8&&!memcmp(input+509,output,8));
    assert(sfs64_read_window(&b,100,token,e,e.bytes,0,0,&done)==BS_OK&&!done);
    assert(sfs64_read_window(&b,200,ro,e,GIB4,output,512,&done)==BS_OK&&done==512);
    memset(output,0xa5,sizeof output);d.fail_read=first+1;
    assert(sfs64_read_window(&b,100,token,e,GIB4-512,output,sizeof output,&done)==BS_IO&&done==512);
    assert(!memcmp(input,output,512));
    for(unsigned i=512;i<sizeof output;++i)assert(output[i]==0xa5);
    before=calls(&d);assert(sfs64_stage_window(&b,100,token,e,GIB4-512,input,sizeof input,&done)==BS_RECOVERY&&!done&&calls(&d)==before);
    assert(sfs64_read_window(&b,200,ro,e,GIB4,output,512,&done)==BS_OK);
    assert(bs_detach(&b,gen)==BS_OK);
    assert(sfs64_read_window(&b,100,token,e,GIB4,output,512,&done)==BS_STALE);
    assert(bs_attach(&b,&backend,&gen)==BS_OK);
    assert(bs_grant_range(&b,gen,100,START,count,BS_READ|BS_WRITE,&token)==BS_OK);
    d.fail_read=0;d.fail_flush=true;before=d.writes;
    assert(sfs64_stage_window(&b,100,token,e,GIB4-512,input,sizeof input,&done)==BS_UNCERTAIN&&done==0&&d.writes==before+1);
    assert(sfs64_stage_window(&b,100,token,e,GIB4-512,input,sizeof input,&done)==BS_RECOVERY&&!done&&d.writes==before+1);
    puts("STYLEFS 64BIT EXTENT PASS: actual high LBA bytes crossing 4GiB file offset, bounds, 64-sector budget, tail, owner, recovery and peer isolation");
}
