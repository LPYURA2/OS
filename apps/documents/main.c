/* Experimental freestanding app entry, NOT the undocumented 744 system ABI.
 * Host must create process, grant safe memory and give a valid broker pointer.
 * No direct shell or hardware access. */
#include "styleos/documents_runtime.h"
#define APP_MAX_DRAW 160
static sdr_runtime instance;
static sdm_document staging;
static uint8_t scratch[SDR_SCRATCH_BYTES];
static sdu_draw display_list[APP_MAX_DRAW];
/* Return a version, or 0 if ABI negotiation fails. The host never calls
 * this function until a matching system ABI is implemented and verified. */
__attribute__((visibility("default")))
uint32_t styleos_documents_app_entry_v1(uint32_t version,const sdr_broker *broker,
                                         int width,int height){
 if(version!=SDR_ABI_VERSION||!broker)return 0;
 return sdr_init(&instance,broker,width,height,scratch,sizeof(scratch),&staging)==SDM_OK ? SDR_ABI_VERSION : 0;
}
__attribute__((visibility("default")))
int styleos_documents_app_event_v1(const sdr_event *event){
 return sdr_dispatch(&instance,event);
}
__attribute__((visibility("default")))
int styleos_documents_app_draw_v1(void){
 size_t n=0;return sdr_render(&instance,display_list,APP_MAX_DRAW,&n);
}

__attribute__((visibility("default")))
int styleos_documents_app_grammar_v1(srev_language language,
                                      srev_grammar_issue *issues,size_t cap,size_t *total){
 return sdr_grammar(&instance,language,issues,cap,total);
}
