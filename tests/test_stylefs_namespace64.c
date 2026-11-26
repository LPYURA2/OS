#include <styleos/stylefs_namespace64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { SECTORS=2+16*16 };
static uint8_t disk[SECTORS][512];
static struct bs_service block;
static struct dc64_state s;
static struct dc64_workspace w,r;
static struct ns64_node nodes[100];
static struct ns64_proof proof;
static uint8_t scratch[SD64_PAGE];
static uint64_t token,writes,reads;
static bool rd(void *c,uint64_t l,uint8_t *p){(void)c;++reads;if(l>=SECTORS)return false;memcpy(p,disk[l],512);return true;}
static bool wr(void *c,uint64_t l,const uint8_t *p){(void)c;++writes;if(l>=SECTORS)return false;memcpy(disk[l],p,512);return true;}
static bool fl(void *c){(void)c;return true;}
static struct sd64_record rec(uint64_t parent,const char *name,uint64_t id,uint8_t kind){
 struct sd64_record e={.key={.parent=parent},.target=id,.kind=kind};
 size_t n=strlen(name);assert(n<=SD64_NAME);e.key.length=(uint16_t)n;memcpy(e.key.name,name,n);return e;
}
static bool next(void *ctx,uint64_t i,struct sd64_record *out){*out=((struct sd64_record *)ctx)[i];return true;}
static void validate(void){assert(ns64_validate(&s,&r,nodes,100,&proof)==NS64_OK);}
static void resolve(const char *path,enum ns64_status expected,uint64_t id,uint8_t kind){
 uint64_t out=999;uint8_t type=99;
 assert(ns64_resolve(&s,&proof,(const uint8_t *)path,strlen(path),scratch,&out,&type)==expected);
 if(expected==NS64_OK)assert(out==id&&type==kind);else assert(out==999&&type==99);
}
static void reopen(void){
 memset(&block,0,sizeof block);uint64_t gen;
 struct bs_backend b={0,1,SECTORS,true,false,rd,wr,fl};
 assert(bs_attach(&block,&b,&gen)==BS_OK&&bs_grant_range(&block,gen,1,0,SECTORS,BS_READ|BS_WRITE,&token)==BS_OK);
 assert(dc64_mount(&s,&block,1,token,SECTORS,&w)==DC64_OK);
}
int main(void){
 uint64_t gen;struct bs_backend b={0,1,SECTORS,true,false,rd,wr,fl};
 assert(bs_attach(&block,&b,&gen)==BS_OK&&bs_grant_range(&block,gen,1,0,SECTORS,BS_READ|BS_WRITE,&token)==BS_OK);
 assert(dc64_provision(&block,1,token,SECTORS)==DC64_OK);reopen();validate();
 resolve("/",NS64_OK,1,SD64_DIRECTORY);resolve("/a",NS64_MISSING,0,0);
 struct sd64_record entries[]={rec(1,"a",2,SD64_DIRECTORY),rec(1,"z",5,SD64_FILE),
  rec(2,"b",3,SD64_DIRECTORY),rec(3,"file",4,SD64_FILE)};
 assert(dc64_publish(&s,&w,4,next,entries)==DC64_OK);
 assert(ns64_validate(&s,&r,nodes,3,&proof)==NS64_FULL&&!proof.valid);
 validate();resolve("/a/b/file",NS64_OK,4,SD64_FILE);
 resolve("/z/a",NS64_NOT_DIRECTORY,0,0);resolve("/a//b",NS64_INVALID,0,0);
 resolve("/no/..",NS64_INVALID,0,0);resolve("/a/",NS64_INVALID,0,0);
 struct dc64_edit e={.op=DC64_RENAME,.entry=entries[0],.destination=rec(3,"a",2,SD64_DIRECTORY).key};
 uint64_t before=writes;
 assert(ns64_apply(&s,&proof,&w,&r,&e)==NS64_CYCLE&&writes==before);
 e.op=DC64_DELETE;assert(ns64_apply(&s,&proof,&w,&r,&e)==NS64_NOT_EMPTY&&writes==before);
 e.op=DC64_INSERT;e.entry=rec(1,"copy",4,SD64_FILE);
 assert(ns64_apply(&s,&proof,&w,&r,&e)==NS64_CONFLICT&&writes==before);
 e.entry=rec(5,"x",6,SD64_FILE);
 assert(ns64_apply(&s,&proof,&w,&r,&e)==NS64_NOT_DIRECTORY&&writes==before);
 e.entry=rec(9,"x",6,SD64_FILE);
 assert(ns64_apply(&s,&proof,&w,&r,&e)==NS64_NOT_DIRECTORY&&writes==before);
 e.op=DC64_RENAME;e.entry=entries[3];e.destination=rec(1,"file",4,SD64_FILE).key;
 assert(ns64_apply(&s,&proof,&w,&r,&e)==NS64_OK&&!proof.valid);
 resolve("/file",NS64_STALE,0,0);reopen();validate();resolve("/file",NS64_OK,4,SD64_FILE);
 resolve("/a/b/file",NS64_MISSING,0,0);
 e.op=DC64_DELETE;e.entry=entries[2];assert(ns64_apply(&s,&proof,&w,&r,&e)==NS64_OK);
 validate();resolve("/a/b",NS64_MISSING,0,0);
 /* Raw snapshot API can represent invalid graph; namespace must refuse. */
 struct sd64_record orphan[]={rec(9,"a",2,SD64_DIRECTORY)};
 assert(dc64_publish(&s,&w,1,next,orphan)==DC64_OK);
 assert(ns64_validate(&s,&r,nodes,100,&proof)==NS64_CORRUPT&&!proof.valid);
 struct sd64_record cycle[]={rec(2,"a",3,SD64_DIRECTORY),rec(3,"b",2,SD64_DIRECTORY)};
 assert(dc64_publish(&s,&w,2,next,cycle)==DC64_OK);
 assert(ns64_validate(&s,&r,nodes,100,&proof)==NS64_CYCLE&&!proof.valid);
 struct sd64_record dup[]={rec(1,"a",2,SD64_FILE),rec(1,"b",2,SD64_FILE)};
 assert(dc64_publish(&s,&w,2,next,dup)==DC64_OK);
 assert(ns64_validate(&s,&r,nodes,100,&proof)==NS64_CORRUPT&&!proof.valid);
 struct sd64_record deep[80];
 for(unsigned i=0;i<80;i++)deep[i]=rec(i+1,"d",i+2,SD64_DIRECTORY);
 assert(dc64_publish(&s,&w,80,next,deep)==DC64_OK);uint64_t before_read=reads;
 validate();assert(reads-before_read==48);
 char path[161];for(unsigned i=0;i<80;i++){path[i*2]='/';path[i*2+1]='d';}path[160]=0;
 resolve(path,NS64_OK,81,SD64_DIRECTORY);
 assert(bs_revoke(&block,1,token)==BS_OK);resolve("/",NS64_STALE,0,0);
 puts("STYLEFS 25 NAMESPACE PASS: graph, paths, atomic checked edits, cycle/orphan/alias refusals, 80-level chain");
 return 0;
}
