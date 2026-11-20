#include "styleos/documents_edit.h"
#include "styleos/documents_history.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static sdm_document doc,stage,backup;
static sdh_history history;
static void text(const char *s){sdm_init(&doc);assert(sdm_set_text(&doc,s,strlen(s))==SDM_OK);}
static void find_at(const char *query,size_t offset){
 size_t result=(size_t)-1;
 assert(sde_find(&doc,query,strlen(query),0,0,&result)==SDM_OK);
 assert(result==offset);
}
int main(void){
 text("кошка и кошка в лесу");
 find_at("кошка",0);
 size_t pos=999;
 assert(sde_find(&doc,"кошка",strlen("кошка"),strlen("кошка"),0,&pos)==SDM_OK);
 assert(pos==strlen("кошка и "));
 assert(sde_find(&doc,"КОШКА",strlen("КОШКА"),0,0,&pos)==SDM_NOT_FOUND);
 assert(pos==strlen("кошка и "));
 assert(sde_find(&doc,"кошка",strlen("кошка"),doc.bytes,1,&pos)==SDM_OK&&pos==0);
 assert(sde_find(&doc,"кошка",strlen("кошка"),1,0,&pos)==SDM_RANGE);
 assert(sde_find(&doc,"",0,0,0,&pos)==SDM_ARGUMENT);
 assert(sde_find(&doc,"\xff",1,0,0,&pos)==SDM_ARGUMENT);
 uint32_t id=0;
 assert(sdm_select(&doc,strlen("кошка и "),strlen("кошка и кошка"))==SDM_OK);
 assert(sdm_add_comment(&doc,"Замечание",strlen("Замечание"),&id)==SDM_OK);
 assert(sdm_select(&doc,0,0)==SDM_OK);
 sdh_begin(&history,&doc);
 size_t replaced=999;
 assert(sde_replace_all(&doc,"кошка",strlen("кошка"),"лис",strlen("лис"),&stage,&replaced)==SDM_OK);
 assert(replaced==2);
 assert(strcmp(doc.content,"лис и лис в лесу")==0);
 assert(!doc.comments[0].active); /* formerly selected token replaced */
 sdh_commit(&history);
 assert(sdh_undo(&history,&doc)==SDM_OK);
 assert(strcmp(doc.content,"кошка и кошка в лесу")==0);
 assert(doc.comments[0].active&&doc.comments[0].id==id);
 assert(sdh_redo(&history,&doc)==SDM_OK);
 assert(strcmp(doc.content,"лис и лис в лесу")==0);
 assert(!doc.comments[0].active);
 assert(sdh_redo(&history,&doc)==SDM_NOT_FOUND);
 assert(sdh_undo(&history,&doc)==SDM_OK);
 assert(sdm_select(&doc,0,0)==SDM_OK);
 sdh_begin(&history,&doc);assert(sdm_insert(&doc,"!",1)==SDM_OK);sdh_commit(&history);
 assert(sdh_redo(&history,&doc)==SDM_NOT_FOUND); /* new edit drops redo */
 text("НАЧАЛО конец");
 assert(sdm_select(&doc,strlen("НАЧАЛО "),strlen("НАЧАЛО конец"))==SDM_OK);
 assert(sdm_add_comment(&doc,"Comment",7,&id)==SDM_OK);
 assert(sdm_select(&doc,0,0)==SDM_OK);
 assert(sde_replace_all(&doc,"НАЧАЛО",strlen("НАЧАЛО"),"А",strlen("А"),&stage,&replaced)==SDM_OK);
 assert(replaced==1 && doc.comments[0].active);
 assert(doc.comments[0].from==strlen("А "));
 assert(doc.comments[0].to==strlen("А конец"));
 backup=doc;
 assert(sde_replace_all(&doc,"А",strlen("А"),"\xff",1,&stage,&replaced)==SDM_ARGUMENT);
 assert(!memcmp(&backup,&doc,sizeof(doc)));
 assert(sde_replace_all(&doc,"А",strlen("А"),stage.content,1,&stage,&replaced)==SDM_ARGUMENT);
 assert(!memcmp(&backup,&doc,sizeof(doc)));
 assert(sde_replace_all(&doc,"не существует",strlen("не существует"),"есть",strlen("есть"),&stage,&replaced)==SDM_OK);
 assert(replaced==0&&!memcmp(&backup,&doc,sizeof(doc)));
 text("aa aa aa");
 char many[1025];memset(many,'x',1024);many[1024]=0;
 backup=doc;
 assert(sde_replace_all(&doc,"a",1,many,1024,&stage,&replaced)==SDM_OK);
 assert(replaced==6 && doc.bytes==6146);
 assert(sde_replace_all(&doc,"x",1,many,1024,&stage,&replaced)==SDM_CAPACITY);
 assert(doc.bytes==6146);
 text("\xf0\x9f\x8d\xb5 \xf0\x9f\x8d\xb5");
 assert(sde_replace_all(&doc,"\xf0\x9f\x8d\xb5",4,"чай",strlen("чай"),&stage,&replaced)==SDM_OK);
 assert(replaced==2&&strcmp(doc.content,"чай чай")==0);
 /* bounded memory: at most four undo snapshots, isolated redo */
 sdh_clear(&history);text("");
 for(int i=0;i<10;i++){
  sdh_begin(&history,&doc);assert(sdm_insert(&doc,"a",1)==SDM_OK);sdh_commit(&history);
 }
 assert(history.undo_count==SDH_STEPS);
 for(int i=0;i<4;i++)assert(sdh_undo(&history,&doc)==SDM_OK);
 assert(doc.bytes==6 && sdh_undo(&history,&doc)==SDM_NOT_FOUND);
 puts("DOCUMENTS EDIT: UTF-8 search/replace, transactional capacity, comments, undo/redo PASS");
 return 0;
}
