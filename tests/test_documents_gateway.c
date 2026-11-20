#include "styleos/documents_app_gateway.h"
#include "styleos/documents_docx.h"
#include "styleos/documents_import.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

typedef struct {sda_file_token token;uint8_t bytes[SDM_DOCX_MAX_OUTPUT];size_t length;int deny,reads,writes;} fake_store;
static int read_cb(void *p,sda_file_token t,uint8_t *out,size_t cap,size_t *n){
 fake_store *s=p;s->reads++;
 if(s->deny||t!=s->token||s->length>cap)return -1;
 memcpy(out,s->bytes,s->length);*n=s->length;return 0;
}
static int write_cb(void *p,sda_file_token t,const uint8_t *data,size_t n){
 fake_store *s=p;s->writes++;
 if(s->deny||t!=s->token||n>sizeof(s->bytes))return -1;
 memcpy(s->bytes,data,n);s->length=n;return 0;
}
int main(void){
 sda_documents_app *a=calloc(1,sizeof(*a)),*other=calloc(1,sizeof(*other));
 sdm_document *staged=calloc(1,sizeof(*staged));
 uint8_t *scratch=malloc(SDM_DOCX_MAX_OUTPUT);
 assert(a&&other&&staged&&scratch);
 fake_store *store=calloc(1,sizeof(*store));assert(store);
 store->token=123; sda_host_io io={store,read_cb,write_cb};
 assert(sda_init(a)==SDM_OK&&sda_init(other)==SDM_OK);
 assert(sdm_set_text(&a->document,"Привет & <текст>\nLine\t2",strlen("Привет & <текст>\nLine\t2"))==SDM_OK);
 sdm_page page=a->document.page;
 page.paper=SDM_LETTER;page.orientation=SDM_LANDSCAPE;
 page.left_twips=1200;page.right_twips=1300;page.top_twips=900;page.bottom_twips=1000;
 assert(sdm_set_page(&a->document,page)==SDM_OK);
 assert(sda_save(a,&io,scratch,SDM_DOCX_MAX_OUTPUT)==SDM_NOT_FOUND);
 assert(sda_save_as(a,&io,124,SDA_DOCX,scratch,SDM_DOCX_MAX_OUTPUT)!=SDM_OK);
 assert(a->document.changed&&a->has_file==0);
 assert(sda_save_as(a,&io,123,SDA_DOCX,scratch,SDM_DOCX_MAX_OUTPUT)==SDM_OK);
 assert(a->has_file && !a->document.changed && a->selected_token==123);
 assert(store->length>500 && store->length<SDM_DOCX_MAX_OUTPUT);
 assert(sda_open(other,&io,123,SDA_DOCX,scratch,SDM_DOCX_MAX_OUTPUT,staged)==SDM_OK);
 assert(strcmp(other->document.content,a->document.content)==0);
 assert(other->document.bytes==a->document.bytes);
 assert(other->document.page.paper==SDM_LETTER);
 assert(other->document.page.orientation==SDM_LANDSCAPE);
 assert(other->document.page.left_twips==1200);
 assert(other->document.page.right_twips==1300);
 assert(other->document.page.top_twips==900);
 assert(other->document.page.bottom_twips==1000);
 assert(!other->document.changed&&other->selected_token==123);
 assert(sdm_select(other->document.bytes?&other->document:NULL,0,0)==SDM_OK);
 assert(sdm_insert(&other->document,"X",1)==SDM_OK);
 assert(strcmp(other->document.content,a->document.content)!=0);/* isolated models */
 store->deny=1;
 assert(sda_save(other,&io,scratch,SDM_DOCX_MAX_OUTPUT)!=SDM_OK);
 assert(other->document.changed);
 assert(sda_open(other,&io,123,SDA_DOCX,scratch,SDM_DOCX_MAX_OUTPUT,staged)!=SDM_OK);
 assert(other->document.content[0]=='X'); /* refused open did not erase edits */
 store->deny=0;
 memcpy(store->bytes,"привет",strlen("привет"));store->length=strlen("привет");
 assert(sda_open(other,&io,123,SDA_TXT,scratch,SDM_DOCX_MAX_OUTPUT,staged)==SDM_OK);
 assert(strcmp(other->document.content,"привет")==0);
 assert(other->selected_token==123 && other->selected_format==SDA_TXT);
 free(store);free(scratch);free(staged);free(other);free(a);
 puts("gateway PASS");return 0;
}