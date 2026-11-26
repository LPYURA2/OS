#include <styleos/stylefs_filebind64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { DIR=2+16*16, DATA=4096, TOTAL=DIR+DATA };
static uint8_t disk[TOTAL][512],saved[TOTAL][512];
static struct bs_service block;
static struct dc64_state dir;
static struct sfs64_journal data;
static struct ns64_proof proof;
static struct ns64_node nodes[100];
static struct fb64_view view;
static struct fb64_workspace work;
static uint64_t dt,jt,gen,writes,fail=UINT64_MAX;
static bool rd(void *ctx,uint64_t l,uint8_t *p){(void)ctx;if(l>=TOTAL)return false;memcpy(p,disk[l],512);return true;}
static bool wr(void *ctx,uint64_t l,const uint8_t *p){(void)ctx;if(++writes==fail||l>=TOTAL)return false;memcpy(disk[l],p,512);return true;}
static bool fl(void *ctx){(void)ctx;return true;}
static void attach(void){
 struct bs_backend be={0,1,TOTAL,true,false,rd,wr,fl};
 assert(bs_attach(&block,&be,&gen)==BS_OK);
 assert(bs_grant_range(&block,gen,7,0,DIR,BS_READ|BS_WRITE,&dt)==BS_OK);
 assert(bs_grant_range(&block,gen,7,DIR,DATA,BS_READ|BS_WRITE,&jt)==BS_OK);
}
static void mount(void){
 assert(dc64_mount(&dir,&block,7,dt,DIR,&work.writer)==DC64_OK);
 assert(sfs64_journal_mount(&data,&block,7,jt,DATA,32)==SJ_OK);
 assert(ns64_validate(&dir,&work.reader,nodes,100,&proof)==NS64_OK);
}
static void reopen(void){assert(bs_detach(&block,gen)==BS_OK);fail=UINT64_MAX;attach();mount();}
static struct sd64_record entry(const char *name,uint64_t id,uint8_t type){
 struct sd64_record e={.key={.parent=1},.target=id,.kind=type};
 e.key.length=(uint16_t)strlen(name);memcpy(e.key.name,name,e.key.length);return e;
}
static bool source(void *ctx,uint64_t off,uint8_t *p,size_t n){
 if(ctx)return false;
 for(size_t i=0;i<n;i++)p[i]=(uint8_t)((off+i)^0x5a);
 return true;
}
static void bind(void){assert(fb64_bind(&view,&dir,&proof,&data)==FB64_OK);}
static void verify(void){
 struct sfs64_file file={0};
 assert(fb64_stat(&view,(const uint8_t *)"/hello",6,work.reader.page,&file)==FB64_OK);
 assert(file.id==2&&file.revision==1&&file.bytes==1041);
 size_t done=0;uint8_t buf[1041];
 assert(sfs64_stream_read(&data,2,1,0,buf,sizeof buf,&done)==BS_OK&&done==sizeof buf);
 for(size_t i=0;i<done;i++)assert(buf[i]==(uint8_t)(i^0x5a));
}
int main(void){
 attach();assert(dc64_provision(&block,7,dt,DIR)==DC64_OK);
 assert(sfs64_journal_provision(&block,7,jt,DATA,32)==SJ_OK);mount();bind();
 struct sd64_record e=entry("hello",2,SD64_FILE);
 memcpy(saved,disk,sizeof saved);
 uint64_t start=writes;
 assert(fb64_create(&view,&work,&e,1041,source,(void *)1)==FB64_CANCELLED&&writes==start);
 assert(fb64_create(&view,&work,&e,1041,source,0)==FB64_OK&&!view.valid);
 uint64_t operations=writes-start;assert(operations==22);
 reopen();bind();verify();
 start=writes;assert(fb64_create(&view,&work,&e,0,0,0)==FB64_CONFLICT&&writes==start);
 /* Inject every write failure across data and namespace commits. */
 for(uint64_t i=1;i<=operations;i++){
  memcpy(disk,saved,sizeof disk);reopen();bind();fail=writes+i;
  assert(fb64_create(&view,&work,&e,1041,source,0)==FB64_UNCERTAIN);
  reopen();bind();assert(dir.count==0);
  struct sfs64_file file;enum sj_status rc=sfs64_stream_stat(&data,2,&file);
  if(i>12){assert(rc==SJ_OK);start=writes;assert(fb64_create(&view,&work,&e,1041,source,0)==FB64_CONFLICT&&writes==start);}
  else assert(rc==SJ_STALE);
 }
 /* Raw namespace write with missing manifest is refused by binding. */
 memcpy(disk,saved,sizeof disk);reopen();
 struct dc64_edit edit={.op=DC64_INSERT,.entry=e};
 assert(ns64_apply(&dir,&proof,&work.writer,&work.reader,&edit)==NS64_OK);
 mount();assert(fb64_bind(&view,&dir,&proof,&data)==FB64_CORRUPT&&!view.valid);
 puts("STYLEFS 26 FILE BIND PASS: durable data before name, 22 write interruptions, orphan retention, missing manifest refusal");
 return 0;
}
