#include "styleos/documents_history.h"
#include <string.h>
static void push(sdm_document *slots,unsigned *count,const sdm_document *value){
 if(*count==SDH_STEPS){
   memmove(slots,slots+1,(SDH_STEPS-1)*sizeof(*slots));(*count)--;
 }
 slots[(*count)++]=*value;
}
void sdh_clear(sdh_history *h){if(h){h->undo_count=0;h->redo_count=0;h->pending_valid=0;}}
void sdh_begin(sdh_history *h,const sdm_document *doc){if(h&&doc){h->pending=*doc;h->pending_valid=1;}}
void sdh_commit(sdh_history *h){
 if(!h || !h->pending_valid)return;
 h->pending_valid=0;
 push(h->undo,&h->undo_count,&h->pending);
 h->redo_count=0;
}
int sdh_undo(sdh_history *h,sdm_document *doc){
 if(!h||!doc)return SDM_ARGUMENT;
 if(!h->undo_count)return SDM_NOT_FOUND;
 h->pending_valid=0;
 push(h->redo,&h->redo_count,doc);
 *doc=h->undo[--h->undo_count];doc->changed=1;
 return SDM_OK;
}
int sdh_redo(sdh_history *h,sdm_document *doc){
 if(!h||!doc)return SDM_ARGUMENT;
 if(!h->redo_count)return SDM_NOT_FOUND;
 h->pending_valid=0;
 push(h->undo,&h->undo_count,doc);
 *doc=h->redo[--h->redo_count];doc->changed=1;
 return SDM_OK;
}
