#include "styleos/documents_runtime.h"
#include "styleos/documents_edit.h"
#include <string.h>
static int prompt(const sdr_runtime *r,int save){
 if (!r->host->request_file_dialog)return SDR_RESPONSE_DENIED;
 int rc=r->host->request_file_dialog(r->host->context,save);
 return rc==0?(save?SDR_RESPONSE_SAVE_DIALOG:SDR_RESPONSE_OPEN_DIALOG):SDR_RESPONSE_DENIED;
}
int sdr_init(sdr_runtime *r,const sdr_broker *host,int w,int h,
             uint8_t *scratch,size_t scratch_bytes,sdm_document *staged){
 if(!r||!host||host->version!=SDR_ABI_VERSION||!scratch||
    scratch_bytes<SDR_SCRATCH_BYTES||!staged||!host->files.read_granted||
    !host->files.replace_atomic_granted)return SDM_ARGUMENT;
 memset(r,0,sizeof(*r));
 r->host=host;r->scratch=scratch;r->scratch_bytes=scratch_bytes;r->staged=staged;
 int rc=sda_init(&r->app);
 if(rc!=SDM_OK)return rc;
 rc=sdu_init(&r->ui,&r->app,w,h);
 if(rc==SDM_OK)r->ready=1;
 return rc;
}
int sdr_render(sdr_runtime *r,sdu_draw *out,size_t capacity,size_t *count){
 if(!r||!r->ready)return SDM_ARGUMENT;
 int rc=sdu_render(&r->ui,out,capacity,count);
 if(rc==SDM_OK&&r->host->present){
   if(r->host->present(r->host->context,out,*count)!=0)return SDR_RESPONSE_DENIED;
 }
 return rc;
}
static int handle_action(sdr_runtime *r,sdu_action action){
 switch(action){
 case SDU_EVENT_NONE:return SDM_OK;
 case SDU_EVENT_NEW:{int rc=sda_new(&r->app);if(rc==SDM_OK){sdh_clear(&r->history);r->query_bytes=0;}return rc;}
 case SDU_EVENT_OPEN:return prompt(r,0);
 case SDU_EVENT_SAVE:{
   if(!r->app.has_file)return prompt(r,1);
   int rc=sda_save(&r->app,&r->host->files,r->scratch,r->scratch_bytes);
   return rc==SDM_OK?SDM_OK:SDR_RESPONSE_DENIED;
 }
 case SDU_EVENT_SAVE_AS:return prompt(r,1);
 case SDU_EVENT_PRINT:return SDR_RESPONSE_PRINT_DIALOG;
 case SDU_EVENT_FIND:return SDR_RESPONSE_FIND_DIALOG;
 case SDU_EVENT_REPLACE:return SDR_RESPONSE_REPLACE_DIALOG;
 case SDU_EVENT_UNDO:return sdh_undo(&r->history,&r->app.document);
 case SDU_EVENT_REDO:return sdh_redo(&r->history,&r->app.document);
 case SDU_EVENT_UNAVAILABLE:return SDM_NOT_FOUND;
 }
 return SDR_RESPONSE_BAD_EVENT;
}
int sdr_dispatch(sdr_runtime *r,const sdr_event *e){
 if(!r||!e||!r->ready)return SDM_ARGUMENT;
 int rc=SDR_RESPONSE_BAD_EVENT;
 switch(e->type){
 case SDR_EVENT_TEXT:
   if(!e->text){rc=SDM_ARGUMENT;break;}
   if(e->bytes || r->app.document.anchor!=r->app.document.cursor){
     sdh_begin(&r->history,&r->app.document);
     rc=sdu_input_text(&r->ui,e->text,e->bytes);
     if(rc==SDM_OK)sdh_commit(&r->history);
   }else rc=SDM_OK;
   break;
 case SDR_EVENT_BACKSPACE:
   if(r->app.document.cursor || r->app.document.anchor!=r->app.document.cursor){
     sdh_begin(&r->history,&r->app.document);
     rc=sdu_backspace(&r->ui);
     if(rc==SDM_OK)sdh_commit(&r->history);
   }else rc=SDM_OK;
   break;
 case SDR_EVENT_POINTER:rc=handle_action(r,sdu_click(&r->ui,e->x,e->y));break;
 case SDR_EVENT_SHORTCUT:rc=handle_action(r,sdu_shortcut(&r->ui,e->key,(int)e->ctrl));break;
 case SDR_EVENT_REDRAW:rc=SDM_OK;break;
 case SDR_EVENT_OPEN_GRANTED:
   if(e->format!=SDA_TXT&&e->format!=SDA_DOCX){rc=SDM_ARGUMENT;break;}
   rc=sda_open(&r->app,&r->host->files,e->token,e->format,
               r->scratch,r->scratch_bytes,r->staged);
   if(rc!=SDM_OK)rc=SDR_RESPONSE_DENIED;
   else{sdh_clear(&r->history);r->query_bytes=0;}
   break;
 case SDR_EVENT_SAVE_AS_GRANTED:
   if(e->format!=SDA_TXT&&e->format!=SDA_DOCX){rc=SDM_ARGUMENT;break;}
   rc=sda_save_as(&r->app,&r->host->files,e->token,e->format,
                   r->scratch,r->scratch_bytes);
   if(rc!=SDM_OK)rc=SDR_RESPONSE_DENIED;
   break;
 case SDR_EVENT_NEW:
   rc=sda_new(&r->app);
   if(rc==SDM_OK){sdh_clear(&r->history);r->query_bytes=0;}
   break;
 case SDR_EVENT_UNDO:rc=sdh_undo(&r->history,&r->app.document);break;
 case SDR_EVENT_REDO:rc=sdh_redo(&r->history,&r->app.document);break;
 case SDR_EVENT_FIND:{
   if(!e->text || !e->bytes || e->bytes>256 || !sdm_valid_utf8(e->text,e->bytes)){
     rc=SDM_ARGUMENT;break;
   }
   memcpy(r->query,e->text,e->bytes);r->query[e->bytes]=0;r->query_bytes=e->bytes;
   size_t at=0;
   rc=sde_find(&r->app.document,r->query,r->query_bytes,
               r->app.document.cursor,1,&at);
   if(rc==SDM_OK)rc=sdm_select(&r->app.document,at,at+r->query_bytes);
   break;
 }
 case SDR_EVENT_FIND_NEXT:{
   if(!r->query_bytes){rc=SDM_NOT_FOUND;break;}
   size_t at=0;
   rc=sde_find(&r->app.document,r->query,r->query_bytes,
               r->app.document.cursor,1,&at);
   if(rc==SDM_OK)rc=sdm_select(&r->app.document,at,at+r->query_bytes);
   break;
 }
 case SDR_EVENT_REPLACE_ALL:{
   if(!r->query_bytes || (!e->text && e->bytes)){rc=SDM_ARGUMENT;break;}
   sdh_begin(&r->history,&r->app.document);
   size_t replacements=0;
   rc=sde_replace_all(&r->app.document,r->query,r->query_bytes,
                       e->text,e->bytes,r->staged,&replacements);
   if(rc==SDM_OK && replacements)sdh_commit(&r->history);
   break;
 }
 case SDR_EVENT_REPLACE_SELECTED:{
   sdm_document *doc=&r->app.document;
   if(!r->query_bytes || (!e->text && e->bytes) ||
      !sdm_valid_utf8(e->text,e->bytes)){rc=SDM_ARGUMENT;break;}
   size_t begin=doc->cursor<doc->anchor?doc->cursor:doc->anchor;
   size_t end=doc->cursor>doc->anchor?doc->cursor:doc->anchor;
   if(end-begin!=r->query_bytes || memcmp(doc->content+begin,r->query,r->query_bytes)!=0){
     rc=SDM_NOT_FOUND;break;
   }
   sdh_begin(&r->history,doc);
   rc=sdm_insert(doc,e->text,e->bytes);
   if(rc==SDM_OK)sdh_commit(&r->history);
   break;
 }
 default:break;
 }
 /* The host can inspect an error, not silently change the user's document. */
 r->last_error=rc<0?rc:0;
 return rc;
}

int sdr_grammar(sdr_runtime *r,srev_language language,
                srev_grammar_issue *issues,size_t capacity,size_t *total){
 if(!r||!r->ready||!issues||!total)return SDM_ARGUMENT;
 return srev_grammar_scan(r->app.document.content,language,issues,capacity,total);
}
