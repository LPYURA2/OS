#include <styleos/stylefs_directory64.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { FILES=100000, PAGES=10000 };
static uint8_t *disk;
static uint64_t reads;
static uint64_t fail_at=UINT64_MAX;
static bool rd(void *ctx,uint64_t lba,uint8_t out[512]){
 (void)ctx;++reads;if(lba>=PAGES*8u||lba==fail_at)return false;
 memcpy(out,disk+lba*512,512);return true;
}
static struct sd64_key key(unsigned i){
 struct sd64_key k={.parent=1+i/1000};
 int n=snprintf((char *)k.name,sizeof k.name,"file-%06u",i);
 assert(n>0&&n<=SD64_NAME);k.length=(uint16_t)n;return k;
}
static struct sd64_record entry(unsigned i){
 return (struct sd64_record){.key=key(i),.target=UINT64_C(0x100000000)+i,
  .kind=i%31?SD64_FILE:SD64_DIRECTORY};
}
static void seal(uint8_t *p){
 memset(p+24,0,4);uint32_t c=UINT32_MAX;
 for(unsigned i=0;i<SD64_PAGE;i++){
  c^=p[i];for(unsigned b=0;b<8;b++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);
 }c=~c;for(unsigned i=0;i<4;i++)p[24+i]=(uint8_t)(c>>(8*i));
}
static uint64_t build(unsigned *height){
 struct sd64_record *index=calloc(PAGES,sizeof *index);assert(index);
 unsigned used=1, count=0;
 for(unsigned i=0;i<FILES;){
  struct sd64_record r[SD64_FANOUT];unsigned n=0;
  while(i<FILES&&n<SD64_FANOUT)r[n++]=entry(i++);
  assert(used<PAGES&&sd64_encode(disk+used*SD64_PAGE,7,0,r,n));
  index[count++]=(struct sd64_record){r[n-1].key,(uint64_t)used++*8,SD64_BRANCH};
 }
 *height=0;
 while(count>1){
  unsigned next=0;++*height;
  for(unsigned i=0;i<count;){
   unsigned n=count-i<SD64_FANOUT?count-i:SD64_FANOUT;
   assert(used<PAGES&&sd64_encode(disk+used*SD64_PAGE,7,*height,index+i,n));
   index[next++]=(struct sd64_record){index[i+n-1].key,(uint64_t)used++*8,SD64_BRANCH};
   i+=n;
  }count=next;
 }
 uint64_t root=index[0].target;free(index);return root;
}
static void lookup_ok(struct bs_service *b,uint64_t token,struct sd64_root *r,unsigned i){
 struct sd64_key k=key(i);uint8_t scratch[SD64_PAGE],kind=0;uint64_t obj=0,before=reads;
 assert(sd64_lookup(b,1,token,r,&k,scratch,&obj,&kind)==SD64_OK);
 assert(obj==entry(i).target&&kind==entry(i).kind&&reads-before==(r->level+1u)*8u);
}
int main(void){
 disk=calloc(PAGES,SD64_PAGE);assert(disk);
 unsigned height;uint64_t root=build(&height);
 struct sd64_root r={root,7,8,(PAGES-1)*8u,height};
 struct bs_service b={0};uint64_t gen,token;
 struct bs_backend backend={0,1,PAGES*8u,false,false,rd,0,0};
 assert(bs_attach(&b,&backend,&gen)==BS_OK);
 assert(bs_grant_range(&b,gen,1,0,PAGES*8u,BS_READ,&token)==BS_OK);
 for(unsigned i=0;i<FILES;i+=137)lookup_ok(&b,token,&r,i);
 lookup_ok(&b,token,&r,FILES-1);
 struct sd64_key k=key(FILES);uint8_t scratch[SD64_PAGE],kind=99;uint64_t obj=99;
 assert(sd64_lookup(&b,1,token,&r,&k,scratch,&obj,&kind)==SD64_MISSING&&obj==99&&kind==99);
 k=key(0);uint64_t before=reads;
 assert(sd64_lookup(&b,2,token,&r,&k,scratch,&obj,&kind)==SD64_INVALID&&reads==before);
 struct sd64_root bad=r;bad.region_sectors=UINT64_MAX-7;
 assert(sd64_lookup(&b,1,token,&bad,&k,scratch,&obj,&kind)==SD64_INVALID&&reads==before);
 uint8_t *page=disk+root*512;uint8_t saved[SD64_PAGE];memcpy(saved,page,sizeof saved);
 page[200]^=1;
 assert(sd64_lookup(&b,1,token,&r,&k,scratch,&obj,&kind)==SD64_CORRUPT&&obj==99);
 memcpy(page,saved,sizeof saved);
 /* Syntactically valid CRC with an invalid/self child. */
 for(unsigned j=0;j<8;j++)page[72+j]=(uint8_t)(root>>(8*j));
 seal(page);assert(sd64_lookup(&b,1,token,&r,&k,scratch,&obj,&kind)==SD64_CORRUPT);
 memcpy(page,saved,sizeof saved);
 /* Valid root CRC but child is substituted with unrelated leaf. */
 memset(page+72,0,8);page[72]=8;seal(page);
 assert(sd64_lookup(&b,1,token,&r,&k,scratch,&obj,&kind)==SD64_CORRUPT);
 memcpy(page,saved,sizeof saved);
 struct sd64_record rec[2]={entry(0),entry(0)};
 memset(scratch,0xab,sizeof scratch);
 assert(!sd64_encode(scratch,7,0,rec,2)&&scratch[0]==0xab);
 rec[0].key.length=2;memset(rec[0].key.name,0,sizeof rec[0].key.name);
 rec[0].key.name[0]=0xc0;rec[0].key.name[1]=0x80;
 assert(!sd64_encode(scratch,7,0,rec,1));
 rec[0].key.length=6;memset(rec[0].key.name,0,sizeof rec[0].key.name);
 memcpy(rec[0].key.name,"\xd1\x87\xd0\xb0\xd0\xb9",6);
 assert(sd64_encode(scratch,7,0,rec,1));
 fail_at=root+3;
 assert(sd64_lookup(&b,1,token,&r,&k,scratch,&obj,&kind)==SD64_IO&&obj==99&&kind==99);
 assert(b.devices[0].recovery);
 free(disk);
 printf("STYLEFS 21 DIRECTORY PASS: %u entries, height %u, bounded page reads, corruption and access refusals\n",FILES,height);
 return 0;
}
