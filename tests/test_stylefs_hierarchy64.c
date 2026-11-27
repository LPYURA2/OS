
#include <styleos/stylefs_dircommit64.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {N=2+16*24};
static uint8_t disk[N][512],durable[N][512],saved[N][512];
static struct bs_service b;
static struct dc64_state s;
static struct dc64_edit_workspace w;
static struct dc64_node nodes[128];
static uint64_t token;
static unsigned writes,fail;
static bool rd(void *c,uint64_t l,uint8_t *p){(void)c;if(l>=N)return false;memcpy(p,disk[l],512);return true;}
static bool wr(void *c,uint64_t l,const uint8_t *p){(void)c;assert(l<N);if(++writes==fail)return false;memcpy(disk[l],p,512);return true;}
static bool fl(void *c){(void)c;memcpy(durable,disk,sizeof disk);return true;}
static void attach(void){
 memset(&b,0,sizeof b);uint64_t gen;
 struct bs_backend be={0,1,N,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK&&bs_grant_range(&b,gen,7,0,N,BS_READ|BS_WRITE,&token)==BS_OK);
}
static void reopen(void){memcpy(disk,durable,sizeof disk);writes=fail=0;attach();assert(dc64_mount(&s,&b,7,token,N,&w.publish)==DC64_OK);}
static struct sd64_key key(uint64_t parent,const char *name){
 struct sd64_key k={.parent=parent,.length=(uint16_t)strlen(name)};
 assert(k.length<=SD64_NAME);memcpy(k.name,name,k.length);return k;
}
static struct dc64_edit make(enum dc64_edit_op op,uint64_t parent,const char *name,uint64_t id,uint8_t kind){
 return (struct dc64_edit){.op=op,.entry={key(parent,name),id,kind}};
}
static enum dc64_status run(struct dc64_edit e){return dc64_hierarchy_apply(&s,&w,nodes,128,&e);}
static void reject(struct dc64_edit e){unsigned before=writes;assert(run(e)==DC64_INVALID&&writes==before);}
static void path(const char *a,const char *bname,const char *c,uint64_t id,uint8_t type){
 const char *names[]={a,bname,c};struct dc64_path_part parts[3]={0};unsigned count=0;
 while(count<3&&names[count]){parts[count].length=(uint16_t)strlen(names[count]);memcpy(parts[count].name,names[count],parts[count].length);count++;}
 uint64_t found=0;uint8_t kind=0;
 assert(dc64_resolve(&s,&w,nodes,128,parts,count,&found,&kind)==DC64_OK&&found==id&&kind==type);
}
struct raw {struct sd64_record r[70];};
static bool next(void *c,uint64_t i,struct sd64_record *out){*out=((struct raw *)c)->r[i];return true;}
int main(void){
 attach();assert(dc64_provision(&b,7,token,N)==DC64_OK);reopen();
 path(0,0,0,1,SD64_DIRECTORY);
 assert(run(make(DC64_CREATE,1,"A",2,SD64_DIRECTORY))==DC64_OK);
 assert(run(make(DC64_CREATE,2,"B",3,SD64_DIRECTORY))==DC64_OK);
 assert(run(make(DC64_CREATE,3,"file",4,SD64_FILE))==DC64_OK);
 assert(run(make(DC64_CREATE,1,"C",5,SD64_DIRECTORY))==DC64_OK);
 path("A","B","file",4,SD64_FILE);
 reject(make(DC64_CREATE,44,"lost",6,SD64_FILE));
 reject(make(DC64_CREATE,4,"child",6,SD64_DIRECTORY));
 reject(make(DC64_CREATE,1,"alias",4,SD64_FILE));
 reject(make(DC64_CREATE,1,"root",1,SD64_DIRECTORY));
 reject(make(DC64_REMOVE,1,"A",2,SD64_DIRECTORY));
 struct dc64_edit e=make(DC64_RENAME,1,"A",2,SD64_DIRECTORY);
 e.destination=key(3,"loop");reject(e);
 e.destination=key(2,"self");reject(e);
 e.destination=key(1,"C");reject(e);
 e.destination=key(5,"A");
 memcpy(saved,durable,sizeof saved);reopen();
 assert(run(e)==DC64_OK);unsigned operations=writes;assert(operations==10);
 path("C","A","B",3,SD64_DIRECTORY);reopen();
 path("C","A","B",3,SD64_DIRECTORY);
 for(unsigned i=1;i<=operations;i++){
  memcpy(durable,saved,sizeof durable);reopen();fail=i;
  assert(run(e)==DC64_UNCERTAIN&&!s.valid);
  reopen();path("A","B","file",4,SD64_FILE);
 }
 memcpy(durable,saved,sizeof durable);reopen();
 assert(run(make(DC64_REMOVE,3,"file",4,SD64_FILE))==DC64_OK);
 assert(run(make(DC64_REMOVE,2,"B",3,SD64_DIRECTORY))==DC64_OK);
 assert(run(make(DC64_REMOVE,1,"A",2,SD64_DIRECTORY))==DC64_OK);
 assert(run(make(DC64_REMOVE,1,"C",5,SD64_DIRECTORY))==DC64_OK);
 assert(s.count==0);reopen();
 /* Deep chain ordered by parent ID; 64 components accepted, 65 refused. */
 struct raw raw={0};
 for(unsigned i=0;i<64;i++)raw.r[i]=(struct sd64_record){key(i+1,"d"),i+2,SD64_DIRECTORY};
 assert(dc64_publish(&s,&w.publish,64,next,&raw)==DC64_OK);
 assert(dc64_hierarchy_check(&s,&w,nodes,128)==DC64_OK);
 assert(dc64_hierarchy_check(&s,&w,nodes,63)==DC64_FULL);
 reject(make(DC64_CREATE,65,"too-deep",66,SD64_FILE));
 struct dc64_path_part parts[64]={0};for(unsigned i=0;i<64;i++){parts[i].length=1;parts[i].name[0]='d';}
 uint64_t id=0;uint8_t kind=0;
 assert(dc64_resolve(&s,&w,nodes,128,parts,64,&id,&kind)==DC64_OK&&id==65);
 /* Deliberately malformed but structurally valid raw snapshot. */
 raw.r[0]=(struct sd64_record){key(7,"orphan"),2,SD64_DIRECTORY};
 assert(dc64_publish(&s,&w.publish,1,next,&raw)==DC64_OK);
 unsigned before=writes;
 assert(dc64_hierarchy_check(&s,&w,nodes,128)==DC64_CORRUPT);
 assert(run(make(DC64_CREATE,1,"x",3,SD64_FILE))==DC64_CORRUPT&&writes==before);
 id=123;kind=99;
 assert(dc64_resolve(&s,&w,nodes,128,0,0,&id,&kind)==DC64_CORRUPT&&id==123&&kind==99);
 raw.r[0]=(struct sd64_record){key(2,"self"),2,SD64_DIRECTORY};
 assert(dc64_publish(&s,&w.publish,1,next,&raw)==DC64_OK);
 assert(dc64_hierarchy_check(&s,&w,nodes,128)==DC64_CORRUPT);
 raw.r[0]=(struct sd64_record){key(1,"a"),2,SD64_FILE};
 raw.r[1]=(struct sd64_record){key(1,"b"),2,SD64_FILE};
 assert(dc64_publish(&s,&w.publish,2,next,&raw)==DC64_OK);
 assert(dc64_hierarchy_check(&s,&w,nodes,128)==DC64_CORRUPT);
 puts("STYLEFS 24 HIERARCHY PASS: nested paths, move, empty directory removal, cycle/orphan/alias/depth refusal, 10 interrupted move writes");
 return 0;
}
