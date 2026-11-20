#include "styleos/documents_runtime.h"
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
 case SDU_EVENT_NEW:return sda_new(&r->app);
 case SDU_EVENT_OPEN:return prompt(r,0);
 case SDU_EVENT_SAVE:{
   if(!r->app.has_file)return prompt(r,1);
   int rc=sda_save(&r->app,&r->host->files,r->scratch,r->scratch_bytes);
   return rc==SDM_OK?SDM_OK:SDR_RESPONSE_DENIED;
 }
 case SDU_EVENT_SAVE_AS:return prompt(r,1);
 case SDU_EVENT_PRINT:return SDR_RESPONSE_PRINT_DIALOG;
 case SDU_EVENT_UNAVAILABLE:return SDM_NOT_FOUND;
 }
 return SDR_RESPONSE_BAD_EVENT;
}
int sdr_dispatch(sdr_runtime *r,const sdr_event *e){
 if(!r||!e||!r->ready)return SDM_ARGUMENT;
 int rc=SDR_RESPONSE_BAD_EVENT;
 switch(e->type){
 case SDR_EVENT_TEXT:
   rc=e->text?sdu_input_text(&r->ui,e->text,e->bytes):SDM_ARGUMENT;break;
 case SDR_EVENT_BACKSPACE:rc=sdu_backspace(&r->ui);break;
 case SDR_EVENT_POINTER:rc=handle_action(r,sdu_click(&r->ui,e->x,e->y));break;
 case SDR_EVENT_SHORTCUT:rc=handle_action(r,sdu_shortcut(&r->ui,e->key,(int)e->ctrl));break;
 case SDR_EVENT_REDRAW:rc=SDM_OK;break;
 case SDR_EVENT_OPEN_GRANTED:
   if(e->format!=SDA_TXT&&e->format!=SDA_DOCX){rc=SDM_ARGUMENT;break;}
   rc=sda_open(&r->app,&r->host->files,e->token,e->format,
               r->scratch,r->scratch_bytes,r->staged);
   if(rc!=SDM_OK)rc=SDR_RESPONSE_DENIED;
   break;
 case SDR_EVENT_SAVE_AS_GRANTED:
   if(e->format!=SDA_TXT&&e->format!=SDA_DOCX){rc=SDM_ARGUMENT;break;}
   rc=sda_save_as(&r->app,&r->host->files,e->token,e->format,
                   r->scratch,r->scratch_bytes);
   if(rc!=SDM_OK)rc=SDR_RESPONSE_DENIED;
   break;
 case SDR_EVENT_NEW:rc=sda_new(&r->app);break;
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
