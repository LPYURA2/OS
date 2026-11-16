#include "styleos/storage.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"storage test failed line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static unsigned char disk[ST_DISK_SECTORS][ST_SECTOR_BYTES];
static unsigned char durable[ST_DISK_SECTORS][ST_SECTOR_BYTES];
static unsigned char backup[ST_DISK_SECTORS][ST_SECTOR_BYTES];
static _Alignas(4096) union st_workspace workspace;
static struct st_store store;
static struct st_pending token;
static struct st_record record;
static const unsigned char name[]="note.txt";
static const unsigned char a[]="StyleOS note A\n";
static const unsigned char b[]="StyleOS note B: updated\n";
static unsigned groups;
struct model {
    unsigned reads,writes,flushes,fail_read,fail_write,fail_flush;
    unsigned persist_data_writes,failed_flush_persists,stop_after;
    struct {char kind;uint64_t sector;} log[8192];
    size_t count;
};
static struct model model;
static jmp_buf power_cut;
static void stop_if_requested(void)
{ if(model.stop_after && model.count==model.stop_after)longjmp(power_cut,1); }
static void event(char kind,uint64_t sector)
{
    CHECK(model.count<sizeof model.log/sizeof model.log[0]);
    model.log[model.count].kind=kind;model.log[model.count].sector=sector;++model.count;
}
static int read_sector(void *opaque,uint64_t lba,void *data)
{
    CHECK(opaque==&model);CHECK(lba<ST_DISK_SECTORS);CHECK(data!=NULL);
    event('R',lba);if(++model.reads==model.fail_read)return -1;
    memcpy(data,disk[lba],ST_SECTOR_BYTES);stop_if_requested();return 0;
}
static int write_sector(void *opaque,uint64_t lba,const void *data)
{
    CHECK(opaque==&model);CHECK(lba<ST_DISK_SECTORS);CHECK(data!=NULL);
    event('W',lba);if(++model.writes==model.fail_write)return -1;
    memcpy(disk[lba],data,ST_SECTOR_BYTES);
    /* An adversarial cache may persist repaired metadata/payload before a
     * pending commit invalidation. Only FLUSH orders the latter durably. */
    if(model.persist_data_writes && lba!=ST_SLOT0_LBA+3 && lba!=ST_SLOT1_LBA+3)
        memcpy(durable[lba],data,ST_SECTOR_BYTES);
    stop_if_requested();return 0;
}
static int flush(void *opaque)
{
    CHECK(opaque==&model);event('F',0);
    if(++model.flushes==model.fail_flush){
        if(model.failed_flush_persists)memcpy(durable,disk,sizeof disk);
        return -1;
    }
    memcpy(durable,disk,sizeof disk);stop_if_requested();return 0;
}
static struct st_io transport(void)
{ return (struct st_io){&model,ST_DISK_SECTORS,read_sector,write_sector,flush}; }
static void reset_counters(void){memset(&model,0,sizeof model);}
static void fresh(void)
{
    memset(disk,0,sizeof disk);memset(durable,0,sizeof durable);
    memset(&store,0,sizeof store);memset(&workspace,0,sizeof workspace);reset_counters();
}
static void crash(void)
{
    memcpy(disk,durable,sizeof disk);memset(&store,0,sizeof store);
    memset(&workspace,0xa5,sizeof workspace);reset_counters();
}
static void initialize(void)
{
    struct st_io io=transport();CHECK(st_init(&io,&workspace,&store)==ST_OK);
    CHECK(store.mounted==1 && store.active==-1 && store.generation==0);
}
static void save(const unsigned char *data,size_t length)
{
    CHECK(st_prepare(&store,&workspace,name,sizeof name-1,data,length,&token)==ST_OK);
    CHECK(st_commit(&store,&workspace,&token)==ST_OK);
}
static void mount(void)
{
    struct st_io io=transport();CHECK(st_mount(&io,&workspace,&store)==ST_OK);
}
static void expect(uint64_t generation,const unsigned char *data,size_t length)
{
    CHECK(st_read(&store,&workspace,&record)==ST_OK);
    CHECK(record.generation==generation && record.name_length==sizeof name-1);
    CHECK(record.length==length && !memcmp(record.name,name,sizeof name));
    CHECK(!length || !memcmp(record.data,data,length));
    for(size_t i=length;i<ST_DATA_MAX;++i)CHECK(record.data[i]==0);
}
static uint64_t base(unsigned slot){return slot?ST_SLOT1_LBA:ST_SLOT0_LBA;}
static void put32(unsigned char *p,uint32_t value)
{for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(value>>(i*8));}
static void put64(unsigned char *p,uint64_t value)
{put32(p,(uint32_t)value);put32(p+4,(uint32_t)(value>>32));}
static uint32_t get32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void fix_crc(unsigned char *sector,size_t offset)
{put32(sector+offset,0);put32(sector+offset,st_crc32(sector,512));}
static void repair_meta(unsigned slot)
{
    unsigned char *meta=disk[base(slot)],*commit=disk[base(slot)+3];
    fix_crc(meta,36);put32(commit+28,get32(meta+36));fix_crc(commit,48);
}

static void test_crc_and_wire_format(void)
{
    fresh();CHECK(st_crc32("123456789",9)==UINT32_C(0xcbf43926));
    CHECK(st_crc32(NULL,0)==0);initialize();
    CHECK(model.reads==2048 && model.writes==1 && model.flushes==1);
    CHECK(model.log[2048].kind=='W' && model.log[2048].sector==0 && model.log[2049].kind=='F');
    CHECK(!memcmp(disk[0],"STYSTR1",8));CHECK(get32(disk[0]+8)==1);
    CHECK(get32(disk[0]+12)==512 && get32(disk[0]+16)==2048);
    CHECK(get32(disk[0]+24)==2 && get32(disk[0]+32)==6);
    CHECK(!memcmp(disk,durable,sizeof disk));
    struct st_io io=transport();CHECK(st_mount(&io,&workspace,&store)==ST_EMPTY);
    CHECK(st_read(&store,&workspace,&record)==ST_EMPTY);++groups;
}
static void test_preserved_and_updated_across_fresh_mounts(void)
{
    fresh();initialize();save(a,sizeof a-1);expect(1,a,sizeof a-1);
    CHECK(store.active==0);crash();mount();expect(1,a,sizeof a-1);
    save(b,sizeof b-1);CHECK(store.active==1);expect(2,b,sizeof b-1);
    crash();mount();expect(2,b,sizeof b-1);++groups;
}
static void test_prepare_barrier_keeps_committed_a(void)
{
    fresh();initialize();save(a,sizeof a-1);reset_counters();
    CHECK(st_prepare(&store,&workspace,name,sizeof name-1,b,sizeof b-1,&token)==ST_OK);
    CHECK(token.slot==1 && token.generation==2 && model.writes==4 && model.flushes==2);
    CHECK(model.count==6 && model.log[0].kind=='W' && model.log[0].sector==9);
    CHECK(model.log[1].kind=='F' && model.log[2].kind=='W' && model.log[2].sector==6);
    CHECK(model.log[3].sector==7 && model.log[4].sector==8 && model.log[5].kind=='F');
    CHECK(!memcmp(disk[9],(unsigned char[512]){0},512));
    /* Data and metadata B are durable; its separate commit sector is not.
     * A new store/workspace does not get to reuse the old in-memory value. */
    crash();mount();expect(1,a,sizeof a-1);CHECK(store.active==0);++groups;
}
static void test_exact_three_flush_order_and_stale_tokens(void)
{
    fresh();initialize();save(a,sizeof a-1);reset_counters();
    CHECK(st_prepare(&store,&workspace,name,sizeof name-1,b,sizeof b-1,&token)==ST_OK);
    struct st_pending wrong=token;wrong.metadata_crc^=1;
    size_t before=model.count;CHECK(st_commit(&store,&workspace,&wrong)==ST_STATE);CHECK(model.count==before);
    CHECK(st_prepare(&store,&workspace,name,sizeof name-1,a,sizeof a-1,&wrong)==ST_STATE);
    CHECK(st_commit(&store,&workspace,&token)==ST_OK);
    CHECK(model.count==8 && model.log[1].kind=='F' && model.log[5].kind=='F' &&
          model.log[6].kind=='W' && model.log[6].sector==9 && model.log[7].kind=='F');
    before=model.count;CHECK(st_commit(&store,&workspace,&token)==ST_STATE);CHECK(model.count==before);
    crash();mount();expect(2,b,sizeof b-1);++groups;
}
static void test_old_commit_does_not_validate_new_pending_generation(void)
{
    fresh();initialize();save(a,sizeof a-1);save(b,sizeof b-1);
    static const unsigned char c[]="third pending generation";
    CHECK(st_prepare(&store,&workspace,name,sizeof name-1,c,sizeof c-1,&token)==ST_OK);
    CHECK(token.slot==0 && token.generation==3);
    crash();mount();expect(2,b,sizeof b-1);++groups;
}
static void test_initialization_never_erases_nonempty_disk(void)
{
    fresh();disk[2047][511]=1;memcpy(backup,disk,sizeof disk);struct st_io io=transport();
    CHECK(st_init(&io,&workspace,&store)==ST_NOT_EMPTY);
    CHECK(model.reads==2048 && model.writes==0 && model.flushes==0 && !memcmp(backup,disk,sizeof disk));
    reset_counters();CHECK(st_mount(&io,&workspace,&store)==ST_SIGNATURE);
    CHECK(model.writes==0 && model.flushes==0 && !memcmp(backup,disk,sizeof disk));
    fresh();initialize();save(a,sizeof a-1);memcpy(backup,disk,sizeof disk);reset_counters();
    CHECK(st_init(&io,&workspace,&store)==ST_NOT_EMPTY);
    CHECK(model.writes==0 && model.flushes==0 && !memcmp(backup,disk,sizeof disk));++groups;
}
static void test_superblock_signature_layout_and_checksum_refusals(void)
{
    for(unsigned which=0;which<6;++which){
        fresh();initialize();save(a,sizeof a-1);
        if(which==0)disk[0][0]^=1;
        if(which==1)disk[0][52]^=1;
        if(which==2){put64(disk[0]+24,UINT64_MAX);fix_crc(disk[0],52);}
        if(which==3){put32(disk[0]+44,2048);fix_crc(disk[0],52);}
        if(which==4){disk[0][511]=1;fix_crc(disk[0],52);}
        if(which==5){put32(disk[0]+8,2);fix_crc(disk[0],52);}
        reset_counters();struct st_io io=transport();
        CHECK(st_mount(&io,&workspace,&store)==(which==0?ST_SIGNATURE:ST_CORRUPT));
        CHECK(model.writes==0 && model.flushes==0);
    }++groups;
}
static void corrupt_new(unsigned which)
{
    unsigned char *meta=disk[6],*commit=disk[9];
    if(which==0)meta[0]^=1;
    if(which==1)meta[36]^=1;
    if(which==2)disk[7][0]^=1;
    if(which==3)commit[48]^=1;
    if(which==4){put32(meta+24,ST_DATA_MAX+1);repair_meta(1);}
    if(which==5){put32(meta+28,32);repair_meta(1);}
    if(which==6){meta[64]='/';repair_meta(1);}
    if(which==7){put64(commit+16,1);fix_crc(commit,48);}
    if(which==8){put32(meta+40,0);repair_meta(1);}
    if(which==9){put32(meta+44,UINT32_MAX);repair_meta(1);}
    if(which==10){meta[500]=1;repair_meta(1);}
    if(which==11){put64(meta+16,0);put64(commit+16,0);repair_meta(1);}
    if(which==12){put64(commit+40,UINT64_MAX);fix_crc(commit,48);}
}
static void test_bad_new_version_falls_back_without_writes(void)
{
    for(unsigned which=0;which<13;++which){
        fresh();initialize();save(a,sizeof a-1);save(b,sizeof b-1);corrupt_new(which);
        reset_counters();mount();expect(1,a,sizeof a-1);CHECK(model.writes==0 && model.flushes==0);
    }++groups;
}
static void damaged_b_falls_back_to_a(void)
{
    fresh();initialize();save(a,sizeof a-1);save(b,sizeof b-1);
    disk[7][0]^=1;memcpy(durable,disk,sizeof disk);
    crash();mount();expect(1,a,sizeof a-1);reset_counters();
}
static void test_fallback_reprepare_cannot_reuse_matching_old_commit(void)
{
    damaged_b_falls_back_to_a();
    CHECK(st_prepare(&store,&workspace,name,sizeof name-1,b,sizeof b-1,&token)==ST_OK);
    CHECK(token.generation==2 && token.slot==1 && model.writes==4 && model.flushes==2);
    CHECK(!memcmp(durable[9],(unsigned char[512]){0},512));
    struct st_pending abandoned=token;
    crash();mount();expect(1,a,sizeof a-1);
    size_t before=model.count;
    CHECK(st_commit(&store,&workspace,&abandoned)==ST_STATE && model.count==before);
    save(b,sizeof b-1);crash();mount();expect(2,b,sizeof b-1);++groups;
}
static void test_fallback_prepare_power_cuts_preserve_a(void)
{
    /* Cut after each completed invalidation/write/flush operation. Metadata
     * and payload may persist immediately while commit invalidation waits
     * for FLUSH, so a missing invalidation flush exposes stale-commit reuse. */
    for(volatile unsigned cut=1;cut<=6;++cut){
        damaged_b_falls_back_to_a();model.persist_data_writes=1;model.stop_after=cut;
        if(setjmp(power_cut)==0){
            (void)st_prepare(&store,&workspace,name,sizeof name-1,b,sizeof b-1,&token);
            CHECK(0);
        }
        crash();mount();expect(1,a,sizeof a-1);
    }++groups;
}
static void test_fallback_prepare_failures_preserve_a_and_refuse_tokens(void)
{
    for(unsigned persistence=0;persistence<2;++persistence){
        for(unsigned which=0;which<6;++which){
            damaged_b_falls_back_to_a();model.persist_data_writes=1;
            model.failed_flush_persists=persistence;
            if(which<4)model.fail_write=which+1;else model.fail_flush=which-3;
            memset(&token,0xa5,sizeof token);struct st_pending old=token;
            CHECK(st_prepare(&store,&workspace,name,sizeof name-1,b,sizeof b-1,&token)==ST_IO);
            CHECK(store.mounted==0 && store.prepared==0 && !memcmp(&token,&old,sizeof old));
            size_t before=model.count;
            CHECK(st_commit(&store,&workspace,&token)==ST_STATE && model.count==before);
            crash();mount();expect(1,a,sizeof a-1);
        }
    }++groups;
}
static void test_no_good_version_and_duplicate_generation_refuse(void)
{
    fresh();initialize();save(a,sizeof a-1);disk[3][0]^=1;
    struct st_io io=transport();reset_counters();CHECK(st_mount(&io,&workspace,&store)==ST_CORRUPT);
    CHECK(model.writes==0 && model.flushes==0);
    fresh();initialize();save(a,sizeof a-1);save(b,sizeof b-1);
    put64(disk[6]+16,1);put64(disk[9]+16,1);repair_meta(1);
    reset_counters();CHECK(st_mount(&io,&workspace,&store)==ST_CORRUPT);
    CHECK(model.writes==0 && model.flushes==0);++groups;
}
static void test_validated_arguments_capacity_and_generation_overflow(void)
{
    fresh();struct st_io io=transport();io.sectors=ST_DISK_SECTORS-1;
    CHECK(st_init(&io,&workspace,&store)==ST_CAPACITY && model.count==0);
    io=transport();io.flush=NULL;CHECK(st_init(&io,&workspace,&store)==ST_ARGUMENT && model.count==0);
    initialize();save(a,sizeof a-1);reset_counters();
    CHECK(st_prepare(&store,&workspace,name,sizeof name-1,b,ST_DATA_MAX+1,&token)==ST_RANGE);
    CHECK(st_prepare(&store,&workspace,(const unsigned char*)"bad/name",8,b,sizeof b-1,&token)==ST_ARGUMENT);
    CHECK(st_prepare(&store,&workspace,name,0,b,sizeof b-1,&token)==ST_ARGUMENT);
    CHECK(st_prepare(&store,&workspace,name,sizeof name-1,NULL,1,&token)==ST_ARGUMENT);
    CHECK(st_prepare(&store,&workspace,name,sizeof name-1,b,sizeof b-1,(void*)&store)==ST_ARGUMENT);
    CHECK(st_prepare(&store,(void*)(workspace.bytes+1),name,sizeof name-1,b,sizeof b-1,&token)==ST_ARGUMENT);
    CHECK(model.count==0);store.generation=UINT64_MAX;
    CHECK(st_prepare(&store,&workspace,name,sizeof name-1,b,sizeof b-1,&token)==ST_OVERFLOW && model.count==0);++groups;
}
static void test_empty_file_maximum_file_and_name(void)
{
    fresh();initialize();save(NULL,0);crash();mount();expect(1,NULL,0);
    unsigned char data[ST_DATA_MAX],long_name[ST_NAME_MAX];
    for(unsigned i=0;i<ST_DATA_MAX;++i)data[i]=(unsigned char)i;
    memset(long_name,'n',sizeof long_name);
    CHECK(st_prepare(&store,&workspace,long_name,sizeof long_name,data,sizeof data,&token)==ST_OK);
    CHECK(st_commit(&store,&workspace,&token)==ST_OK);crash();mount();
    CHECK(st_read(&store,&workspace,&record)==ST_OK && record.generation==2);
    CHECK(record.length==ST_DATA_MAX && record.name_length==ST_NAME_MAX);
    CHECK(!memcmp(record.data,data,sizeof data) && !memcmp(record.name,long_name,sizeof long_name));++groups;
}
static void test_transport_read_failures_cannot_format_or_mount(void)
{
    fresh();model.fail_read=2048;struct st_io io=transport();
    CHECK(st_init(&io,&workspace,&store)==ST_IO && model.writes==0 && model.flushes==0);
    fresh();initialize();save(a,sizeof a-1);reset_counters();model.fail_read=1;
    CHECK(st_mount(&io,&workspace,&store)==ST_IO && model.writes==0);
    reset_counters();mount();model.fail_read=model.reads+1;
    CHECK(st_read(&store,&workspace,&record)==ST_IO);++groups;
}
static void test_prepare_failures_never_issue_commit_and_recover_a(void)
{
    for(unsigned which=0;which<6;++which){
        fresh();initialize();save(a,sizeof a-1);reset_counters();
        if(which<4)model.fail_write=which+1;else model.fail_flush=which-3;
        memset(&token,0xa5,sizeof token);struct st_pending old=token;
        CHECK(st_prepare(&store,&workspace,name,sizeof name-1,b,sizeof b-1,&token)==ST_IO);
        CHECK(!memcmp(&token,&old,sizeof old) && store.mounted==0);
        for(size_t i=0;i<model.count;++i)
            CHECK(model.log[i].kind!='W' || model.log[i].sector!=9 || i==0);
        CHECK(!memcmp(disk[9],(unsigned char[512]){0},512));
        CHECK(st_commit(&store,&workspace,&token)==ST_STATE);crash();mount();expect(1,a,sizeof a-1);
    }++groups;
}
static void test_commit_write_or_flush_failure_never_claims_durable_b(void)
{
    for(unsigned which=0;which<2;++which){
        fresh();initialize();save(a,sizeof a-1);reset_counters();
        CHECK(st_prepare(&store,&workspace,name,sizeof name-1,b,sizeof b-1,&token)==ST_OK);
        if(which==0)model.fail_write=5;else model.fail_flush=3;
        CHECK(st_commit(&store,&workspace,&token)==ST_IO && store.mounted==0);
        CHECK(st_commit(&store,&workspace,&token)==ST_STATE);crash();mount();expect(1,a,sizeof a-1);
    }++groups;
}
int main(void)
{
    test_crc_and_wire_format();test_preserved_and_updated_across_fresh_mounts();
    test_prepare_barrier_keeps_committed_a();test_exact_three_flush_order_and_stale_tokens();
    test_old_commit_does_not_validate_new_pending_generation();test_initialization_never_erases_nonempty_disk();
    test_superblock_signature_layout_and_checksum_refusals();test_bad_new_version_falls_back_without_writes();
    test_fallback_reprepare_cannot_reuse_matching_old_commit();test_fallback_prepare_power_cuts_preserve_a();
    test_fallback_prepare_failures_preserve_a_and_refuse_tokens();
    test_no_good_version_and_duplicate_generation_refuse();test_validated_arguments_capacity_and_generation_overflow();
    test_empty_file_maximum_file_and_name();test_transport_read_failures_cannot_format_or_mount();
    test_prepare_failures_never_issue_commit_and_recover_a();test_commit_write_or_flush_failure_never_claims_durable_b();
    printf("storage format host PASS: %u groups; CRC, whole-file versions, ordered invalidation/data/commit flushes, crash barrier, refusals and transport failures\n",groups);
    return 0;
}
