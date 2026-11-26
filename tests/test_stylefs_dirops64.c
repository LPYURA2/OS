#include <styleos/stylefs_dircommit64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { SECTORS=2+16*16 };
static uint8_t disk[SECTORS][512];
static struct bs_service b;
static struct dc64_state s;
static struct dc64_workspace w,r;
static uint64_t token,reads,writes,fail_read=UINT64_MAX;
static bool rd(void *c,uint64_t l,uint8_t *p){
 (void)c;++reads;if(l>=SECTORS||l==fail_read)return false;memcpy(p,disk[l],512);return true;
}
static bool wr(void *c,uint64_t l,const uint8_t *p){
 (void)c;++writes;if(l>=SECTORS)return false;memcpy(disk[l],p,512);return true;
}
static bool fl(void *c){(void)c;return true;}
static void attach(void){
 memset(&b,0,sizeof b);uint64_t gen;
 struct bs_backend be={0,1,SECTORS,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK&&bs_grant_range(&b,gen,1,0,SECTORS,BS_READ|BS_WRITE,&token)==BS_OK);
}
static struct sd64_key key(uint64_t parent,unsigned i){
 struct sd64_key k={.parent=parent};
 int n=snprintf((char *)k.name,sizeof k.name,"entry-%04u",i);
 assert(n>0&&n<SD64_NAME);k.length=(uint16_t)n;return k;
}
static struct sd64_record item(unsigned i){return (struct sd64_record){key(1,i),UINT64_C(0x100000000)+i,SD64_FILE};}
static bool next(void *ctx,uint64_t i,struct sd64_record *out){(void)ctx;*out=item((unsigned)i*2);return true;}
static void expect(unsigned i,struct sd64_record x){
 struct sd64_record y={0};uint64_t before=reads;
 assert(dc64_entry_at(&s,&r,s.epoch,i,&y)==DC64_OK&&reads-before==8);
 assert(sd64_compare(&x.key,&y.key)==0&&x.target==y.target&&x.kind==y.kind);
}
static void reboot(void){attach();assert(dc64_mount(&s,&b,1,token,SECTORS,&w)==DC64_OK);}
int main(void){
 attach();assert(dc64_provision(&b,1,token,SECTORS)==DC64_OK);reboot();
 assert(dc64_publish(&s,&w,40,next,0)==DC64_OK);
 for(unsigned i=0;i<40;i++)expect(i,item(i*2));
 uint64_t epoch=s.epoch,before=writes;
 struct dc64_edit e={.op=DC64_INSERT,.entry=item(15)};
 assert(dc64_apply(&s,&w,&r,epoch,&e)==DC64_OK&&s.count==41);
 expect(7,item(14));expect(8,item(15));expect(9,item(16));
 before=writes;
 assert(dc64_apply(&s,&w,&r,epoch,&e)==DC64_STALE&&writes==before);
 assert(dc64_apply(&s,&w,&r,s.epoch,&e)==DC64_CONFLICT&&writes==before);
 e.op=DC64_RENAME;e.destination=key(1,16);
 assert(dc64_apply(&s,&w,&r,s.epoch,&e)==DC64_CONFLICT&&writes==before);
 e.destination=key(2,1);
 assert(dc64_apply(&s,&w,&r,s.epoch,&e)==DC64_OK&&s.count==41);
 expect(8,item(16));struct sd64_record moved=item(15);moved.key=e.destination;expect(40,moved);
 reboot();expect(40,moved);
 e.entry=moved;e.destination=key(1,1);
 assert(dc64_apply(&s,&w,&r,s.epoch,&e)==DC64_OK);moved.key=e.destination;expect(1,moved);
 e.op=DC64_DELETE;e.entry=moved;e.entry.target++;
 before=writes;assert(dc64_apply(&s,&w,&r,s.epoch,&e)==DC64_STALE&&writes==before);
 e.entry=moved;assert(dc64_apply(&s,&w,&r,s.epoch,&e)==DC64_OK&&s.count==40);
 reboot();for(unsigned i=0;i<40;i++)expect(i,item(i*2));
 struct sd64_record y={.target=99};before=reads;
 assert(dc64_entry_at(&s,&r,s.epoch,40,&y)==DC64_MISSING&&y.target==99&&reads==before);
 assert(dc64_entry_at(&s,&r,s.epoch-1,0,&y)==DC64_STALE&&reads==before);
 before=writes;assert(dc64_apply(&s,&w,&w,s.epoch,&e)==DC64_INVALID&&writes==before);
 fail_read=s.root.region_start;
 assert(dc64_entry_at(&s,&r,s.epoch,0,&y)==DC64_IO&&!s.valid&&y.target==99);
 fail_read=UINT64_MAX;reboot();expect(0,item(0));
 assert(dc64_publish(&s,&w,0,0,0)==DC64_OK);
 e.op=DC64_INSERT;e.entry=item(7);
 assert(dc64_apply(&s,&w,&r,s.epoch,&e)==DC64_OK);expect(0,item(7));
 e.op=DC64_DELETE;assert(dc64_apply(&s,&w,&r,s.epoch,&e)==DC64_OK);
 reboot();assert(s.count==0);
 puts("STYLEFS 23 DIRECTORY OPS PASS: ordinal reads, atomic insert/delete/rename, stale/conflict/IO and empty tree");
 return 0;
}
