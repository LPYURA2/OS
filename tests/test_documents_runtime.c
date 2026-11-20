#include "styleos/documents_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t scratch_a[SDR_SCRATCH_BYTES],scratch_b[SDR_SCRATCH_BYTES];
static sdm_document staged_a,staged_b;
static sdr_runtime a,b;
static struct {
 char data[16384];size_t length;int block_read,block_write;
 unsigned presented,dialogs,requests;
} store[2];
static int read_file(void *ctx,sda_file_token t,uint8_t *out,size_t cap,size_t *length){
 size_t index=(size_t)(uintptr_t)ctx;
 if(!t||store[index].block_read||store[index].length>cap)return -1;
 memcpy(out,store[index].data,store[index].length);*length=store[index].length;return 0;
}
static int save_file(void *ctx,sda_file_token t,const uint8_t *src,size_t size){
 size_t index=(size_t)(uintptr_t)ctx;
 if(!t||store[index].block_write||size>sizeof(store[index].data))return -1;
 memcpy(store[index].data,src,size);store[index].length=size;return 0;
}
static int file_dialog(void *ctx,int save){size_t i=(size_t)(uintptr_t)ctx;store[i].dialogs++;store[i].requests=(unsigned)save;return 0;}
static int present(void *ctx,const sdu_draw *draw,size_t count){
 size_t i=(size_t)(uintptr_t)ctx;assert(draw&&count>3);store[i].presented++;return 0;
}
static sdr_broker broker_a={SDR_ABI_VERSION,(void*)0,present,file_dialog,{(void*)0,read_file,save_file}};
static sdr_broker broker_b={SDR_ABI_VERSION,(void*)1,present,file_dialog,{(void*)1,read_file,save_file}};
static int event(sdr_runtime *rt,uint32_t type,const char *text,sda_file_token t,sda_format f){
 sdr_event e={0};e.type=type;e.text=text;e.bytes=text?strlen(text):0;e.token=t;e.format=f;
 return sdr_dispatch(rt,&e);
}
int main(void){
 assert(sdr_init(&a,&broker_a,1200,720,scratch_a,sizeof(scratch_a),&staged_a)==SDM_OK);
 assert(sdr_init(&b,&broker_b,1200,720,scratch_b,sizeof(scratch_b),&staged_b)==SDM_OK);
 assert(event(&a,SDR_EVENT_TEXT,"Документы, привет!",0,SDA_TXT)==SDM_OK);
 assert(event(&b,SDR_EVENT_TEXT,"Second",0,SDA_TXT)==SDM_OK);
 assert(event(&a,SDR_EVENT_SAVE_AS_GRANTED,0,7,SDA_TXT)==SDM_OK);
 assert(store[0].length==strlen("Документы, привет!"));
 assert(!memcmp(store[0].data,"Документы, привет!",store[0].length));
 assert(event(&a,SDR_EVENT_NEW,0,0,SDA_TXT)==SDM_OK);
 assert(a.app.document.bytes==0 && b.app.document.bytes==strlen("Second"));
 assert(event(&a,SDR_EVENT_OPEN_GRANTED,0,7,SDA_TXT)==SDM_OK);
 assert(a.app.document.bytes==strlen("Документы, привет!"));
 assert(event(&a,SDR_EVENT_TEXT,"!",0,SDA_TXT)==SDM_OK);
 store[0].block_write=1;
 {sdr_event e={0};e.type=SDR_EVENT_SHORTCUT;e.key='s';e.ctrl=1;
 assert(sdr_dispatch(&a,&e)==SDR_RESPONSE_DENIED);}
 assert(a.app.document.changed==1);
 store[0].block_write=0;
 {sdr_event e={0};e.type=SDR_EVENT_SHORTCUT;e.key='s';e.ctrl=1;
 assert(sdr_dispatch(&a,&e)==SDM_OK);}
 assert(a.app.document.changed==0);
 store[0].block_read=1;
 assert(event(&a,SDR_EVENT_OPEN_GRANTED,0,7,SDA_TXT)==SDR_RESPONSE_DENIED);
 assert(a.app.document.bytes>0);
 store[0].block_read=0;
 sdu_draw commands[SDU_COMMANDS_MAX];size_t n=0;
 assert(sdr_render(&a,commands,SDU_COMMANDS_MAX,&n)==SDM_OK&&n>20&&store[0].presented==1);
 {sdr_event e={0};e.type=SDR_EVENT_POINTER;e.x=40;e.y=50;
 assert(sdr_dispatch(&a,&e)==SDM_OK);
 e.x=105;e.y=83;
 assert(sdr_dispatch(&a,&e)==SDR_RESPONSE_OPEN_DIALOG&&store[0].dialogs==1);}
 assert(event(&b,SDR_EVENT_SAVE_AS_GRANTED,0,9,SDA_TXT)==SDM_OK);
 assert(store[1].length==6&&store[0].length!=store[1].length);
 assert(event(&a,SDR_EVENT_SAVE_AS_GRANTED,0,0,SDA_TXT)==SDR_RESPONSE_DENIED);
 assert(a.app.document.bytes>0);
 assert(event(&a,SDR_EVENT_OPEN_GRANTED,0,7,(sda_format)99)==SDM_ARGUMENT);
 assert(event(&b,SDR_EVENT_SAVE_AS_GRANTED,0,10,SDA_DOCX)==SDM_OK);
 assert(store[1].length>1024);
 assert(!memcmp(store[1].data,"PK",2));
 assert(event(&b,SDR_EVENT_NEW,0,0,SDA_TXT)==SDM_OK);
 assert(event(&b,SDR_EVENT_OPEN_GRANTED,0,10,SDA_DOCX)==SDM_OK);
 assert(b.app.document.bytes==6);
 puts("DOCS RUNTIME HOST PASS: scoped IO, two windows, UTF-8, deny, save, load, rendering, shortcuts");
 return 0;
}
