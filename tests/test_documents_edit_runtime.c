#include "styleos/documents_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static sdr_runtime runtime;
static uint8_t scratch[SDR_SCRATCH_BYTES];
static sdm_document stage;
static char file[16384];static size_t file_bytes;
static int read_file(void *unused,sda_file_token tok,uint8_t *out,size_t cap,size_t *n){
 (void)unused;if(tok!=5 || cap<file_bytes)return -1;
 memcpy(out,file,file_bytes);*n=file_bytes;return 0;
}
static int write_file(void *unused,sda_file_token tok,const uint8_t *src,size_t n){
 (void)unused;if(tok!=5 || n>sizeof(file))return -1;
 memcpy(file,src,n);file_bytes=n;return 0;
}
static int dialog(void *ctx,int mode){(void)ctx;(void)mode;return 0;}
static int event(uint32_t type,const char *value){
 sdr_event e={0};e.type=type;e.text=value;e.bytes=value?strlen(value):0;
 return sdr_dispatch(&runtime,&e);
}
static int ctrl(unsigned k){sdr_event e={0};e.type=SDR_EVENT_SHORTCUT;e.ctrl=1;e.key=k;return sdr_dispatch(&runtime,&e);}
int main(void){
 sdr_broker broker={SDR_ABI_VERSION,0,0,dialog,{0,read_file,write_file}};
 assert(sdr_init(&runtime,&broker,1200,760,scratch,sizeof(scratch),&stage)==SDM_OK);
 assert(event(SDR_EVENT_TEXT,"мир мир и мир")==SDM_OK);
 assert(ctrl('f')==SDR_RESPONSE_FIND_DIALOG);
 assert(ctrl('h')==SDR_RESPONSE_REPLACE_DIALOG);
 assert(event(SDR_EVENT_FIND,"мир")==SDM_OK);
 assert(runtime.app.document.anchor==0 && runtime.app.document.cursor==strlen("мир"));
 assert(event(SDR_EVENT_FIND_NEXT,NULL)==SDM_OK);
 assert(runtime.app.document.anchor==strlen("мир "));
 assert(event(SDR_EVENT_REPLACE_SELECTED,"дом")==SDM_OK);
 assert(!strcmp(runtime.app.document.content,"мир дом и мир"));
 assert(ctrl('z')==SDM_OK);
 assert(!strcmp(runtime.app.document.content,"мир мир и мир"));
 assert(ctrl('y')==SDM_OK);
 assert(!strcmp(runtime.app.document.content,"мир дом и мир"));
 assert(event(SDR_EVENT_FIND,"мир")==SDM_OK);
 assert(event(SDR_EVENT_REPLACE_ALL,"лес")==SDM_OK);
 assert(!strcmp(runtime.app.document.content,"лес дом и лес"));
 assert(event(SDR_EVENT_UNDO,NULL)==SDM_OK);
 assert(!strcmp(runtime.app.document.content,"мир дом и мир"));
 assert(event(SDR_EVENT_REDO,NULL)==SDM_OK);
 assert(!strcmp(runtime.app.document.content,"лес дом и лес"));
 assert(ctrl('a')==SDM_OK);
 assert(runtime.app.document.anchor==0 && runtime.app.document.cursor==runtime.app.document.bytes);
 assert(event(SDR_EVENT_TEXT,"Новый документ")==SDM_OK);
 assert(!strcmp(runtime.app.document.content,"Новый документ"));
 sdr_event e={0};e.type=SDR_EVENT_SAVE_AS_GRANTED;e.token=5;e.format=SDA_TXT;
 assert(sdr_dispatch(&runtime,&e)==SDM_OK);
 assert(file_bytes==strlen("Новый документ") && !memcmp(file,"Новый документ",file_bytes));
 assert(event(SDR_EVENT_NEW,NULL)==SDM_OK);
 assert(event(SDR_EVENT_UNDO,NULL)==SDM_NOT_FOUND); /* separate document history */
 e=(sdr_event){0};e.type=SDR_EVENT_OPEN_GRANTED;e.token=5;e.format=SDA_TXT;
 assert(sdr_dispatch(&runtime,&e)==SDM_OK);
 assert(!strcmp(runtime.app.document.content,"Новый документ"));
 assert(event(SDR_EVENT_FIND_NEXT,NULL)==SDM_NOT_FOUND);
 puts("DOCUMENTS EDIT RUNTIME: find, replace one/all, undo/redo, select all, save/open PASS");
 return 0;
}
