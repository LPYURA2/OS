#include <styleos/volume_service.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { N=SFS_V2_MIN_SECTORS, OFFSET=8 };
struct disk {uint8_t *bytes;uint32_t sectors;unsigned writes,reads,flushes;bool fail_flush;};
static struct bs_service block;
static struct sv_service volumes;
static struct sfs_volume provision;
static struct sfs_workspace work;
static bool rd(void *ctx,uint64_t lba,uint8_t out[512]){
    struct disk *d=ctx;if(lba>=d->sectors)return false;
    ++d->reads;memcpy(out,d->bytes+lba*512,512);return true;
}
static bool wr(void *ctx,uint64_t lba,const uint8_t in[512]){
    struct disk *d=ctx;if(lba>=d->sectors)return false;
    ++d->writes;memcpy(d->bytes+lba*512,in,512);return true;
}
static bool fl(void *ctx){struct disk *d=ctx;++d->flushes;return !d->fail_flush;}
static bool oldrd(void *ctx,uint32_t lba,uint8_t out[512]){return rd(ctx,(uint64_t)lba+OFFSET,out);}
static bool oldwr(void *ctx,uint32_t lba,const uint8_t in[512]){return wr(ctx,(uint64_t)lba+OFFSET,in);}
static bool sink(void *ctx,uint32_t offset,const uint8_t *in,size_t n){
    struct sv_ref root;assert(sv_root(&volumes,1,&root)==SV_BUSY);
    memcpy((uint8_t *)ctx+offset,in,n);return true;
}
int main(void){
    struct disk a={.sectors=N+2*OFFSET},b={.sectors=N+2*OFFSET};
    a.bytes=calloc(a.sectors,512);b.bytes=calloc(b.sectors,512);
    assert(a.bytes&&b.bytes);
    struct sfs_io io={&a,N,oldrd,oldwr,fl};
    assert(sfs_format(&provision,&work,&io)==SFS_OK);
    io.context=&b;assert(sfs_format(&provision,&work,&io)==SFS_OK);
    memset(a.bytes,0x5a,512);memset(a.bytes+(N+OFFSET)*512u,0xa5,512);
    struct bs_backend da={&a,1,a.sectors,true,false,rd,wr,fl};
    struct bs_backend db={&b,2,b.sectors,false,true,rd,0,0};
    uint64_t ga,gb,ma=0,mb=0,bad=99;
    assert(bs_attach(&block,&da,&ga)==BS_OK&&bs_attach(&block,&db,&gb)==BS_OK);
    unsigned writes=a.writes,other=b.writes;
    assert(sv_mount(&volumes,&block,ga,10,0,N,true,&bad)==SV_SIGNATURE&&bad==99&&a.writes==writes);
    assert(sv_mount(&volumes,&block,ga,10,OFFSET,N,true,&ma)==SV_OK&&a.writes==writes);
    assert(sv_mount(&volumes,&block,gb,20,OFFSET,N,false,&mb)==SV_OK&&b.writes==other);
    struct sv_info info;assert(sv_info(&volumes,ma,&info)==SV_OK&&info.format==2&&info.writable);
    struct sv_ref root,system,users,file,tmp,second;
    assert(sv_root(&volumes,ma,&root)==SV_OK&&sv_root(&volumes,mb,&second)==SV_OK);
    assert(sv_mkdir(&volumes,second,"deny",&tmp)==SV_DENIED&&b.writes==other);
    assert(sv_mkdir(&volumes,root,"StyleOS",&system)==SV_OK);
    assert(sv_mkdir(&volumes,root,"Пользователи",&users)==SV_STALE);
    assert(sv_root(&volumes,ma,&root)==SV_OK);
    assert(sv_mkdir(&volumes,root,"Пользователи",&users)==SV_OK);
    const uint8_t message[]="persistent user document";
    assert(sv_create(&volumes,users,"note.txt",message,sizeof message,&file)==SV_OK);
    struct sfs_entry meta;
    assert(sv_stat(&volumes,users,&meta)==SV_STALE);
    assert(sv_stat(&volumes,file,&meta)==SV_OK&&meta.size==sizeof message);
    uint8_t output[sizeof message]={0};
    assert(sv_read_to(&volumes,file,sink,output)==SV_OK&&!memcmp(message,output,sizeof message));
    assert(sv_root(&volumes,ma,&root)==SV_OK);
    assert(sv_next(&volumes,root,0,&tmp,&meta)==SV_OK&&!strcmp(meta.name,"StyleOS"));
    assert(sv_next(&volumes,root,tmp.index,&tmp,&meta)==SV_OK&&!strcmp(meta.name,"Пользователи"));
    assert(sv_next(&volumes,root,tmp.index,&tmp,&meta)==SV_NOT_FOUND);
    assert(sv_lookup(&volumes,second,"Пользователи",&tmp)==SV_NOT_FOUND);
    assert(sv_unmount(&volumes,ma)==SV_OK&&sv_stat(&volumes,file,&meta)==SV_STALE);
    uint64_t old=ma;assert(sv_mount(&volumes,&block,ga,10,OFFSET,N,true,&ma)==SV_OK&&ma>old);
    assert(sv_root(&volumes,ma,&root)==SV_OK);
    assert(sv_lookup(&volumes,root,"Пользователи",&users)==SV_OK);
    assert(sv_lookup(&volumes,users,"note.txt",&file)==SV_OK);
    assert(sv_read_to(&volumes,file,sink,output)==SV_OK&&!memcmp(message,output,sizeof message));
    a.fail_flush=true;
    assert(sv_mkdir(&volumes,root,"uncertain",&tmp)==SV_UNCERTAIN);
    writes=a.writes;assert(sv_mkdir(&volumes,root,"retry",&tmp)==SV_RECOVERY&&a.writes==writes);
    assert(sv_info(&volumes,mb,&info)==SV_OK&&!info.writable);
    assert(sv_unmount(&volumes,ma)==SV_OK);
    assert(bs_detach(&block,ga)==BS_OK);a.fail_flush=false;
    assert(bs_attach(&block,&da,&ga)==BS_OK);
    assert(sv_mount(&volumes,&block,ga,10,OFFSET,N,true,&ma)==SV_OK);
    assert(sv_root(&volumes,ma,&root)==SV_OK);
    assert(sv_lookup(&volumes,root,"Пользователи",&users)==SV_OK);
    assert(sv_lookup(&volumes,users,"note.txt",&file)==SV_OK);
    assert(sv_read_to(&volumes,file,sink,output)==SV_OK&&!memcmp(message,output,sizeof message));
    assert(bs_detach(&block,ga)==BS_OK);
    assert(sv_stat(&volumes,file,&meta)==SV_STALE);
    assert(sv_unmount(&volumes,ma)==SV_OK&&sv_unmount(&volumes,mb)==SV_OK);
    for(unsigned i=0;i<512;++i){assert(a.bytes[i]==0x5a);assert(a.bytes[(N+OFFSET)*512u+i]==0xa5);}
    assert(b.writes==other);
    free(a.bytes);free(b.bytes);
    puts("VOLUME SERVICE PASS: actual v2 images, root enumeration, read-only, persistent file, stale generations, fault isolation, remount, offset guards");
}
