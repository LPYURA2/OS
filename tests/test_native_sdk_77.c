#include <styleos/native_v1.h>
#include <styleos/native_window_channel.h>
#include <styleos/native_file_abi.h>
#include <styleos/stylefs_paged_object64.h>
#include <styleos/process_trap_847.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
_Static_assert(STY_INPUT_BYTES==NWC_INPUT_BYTES&&STY_FRAME_RECTS==NWC_FRAME_RECTS,"wire size");
_Static_assert((int)STY_OK==(int)NWC_OK&&(int)STY_INVALID==(int)NWC_INVALID&&
 (int)STY_NOT_READY==(int)NWC_NOT_READY&&(int)STY_DENIED==(int)NWC_DENIED&&
 (int)STY_MISSING==(int)NWC_MISSING&&(int)STY_FULL==(int)NWC_FULL&&
 (int)STY_EMPTY==(int)NWC_EMPTY&&(int)STY_STALE==(int)NWC_STALE&&
 (int)STY_OVERFLOW==(int)NWC_OVERFLOW,"status parity");
_Static_assert(STY_INPUT==NWC_CALL_INPUT&&STY_FRAME==NWC_CALL_FRAME&&STY_PIXEL==NWC_CALL_PIXEL&&
 STY_TILE==NWC_CALL_TILE&&(int)STY_YIELD==(int)S847_CALL_YIELD&&(int)STY_EXIT==(int)S847_CALL_EXIT,"syscall parity");
_Static_assert((int)STY_KEY==(int)NWC_KEY&&(int)STY_TEXT==(int)NWC_TEXT&&
 (int)STY_POINTER==(int)NWC_POINTER&&(int)STY_DROP==(int)NWC_DEEP_DROP,"event parity");
_Static_assert(STY_PATCH==NWC_CALL_PATCH&&STY_FRAME_GEN==NWC_CALL_FRAME_GEN&&STY_PATCH_BYTES==NWC_PATCH_BYTES,"patch parity");
_Static_assert(STY_TEXT_LAYER==NWC_CALL_TEXT&&STY_TEXT_WIRE_MAX==NWC_TEXT_WIRE_MAX&&
 STY_TEXT_RUN_BYTES==NWC_TEXT_RUN_BYTES&&STY_TEXT_HEADER==NWC_TEXT_HEADER,"text wire parity");
_Static_assert(STY_FILE_READ==NF91_READ&&STY_FILE_WRITE==NF91_WRITE&&STY_FILE_TAKE==NF91_TAKE&&
 STY_FILE_CANCEL==NF91_CANCEL&&STY_FILE_MAX==NF91_MAX&&STY_F_UNAVAILABLE==NF91_UNAVAILABLE,"file op parity");
_Static_assert((int)STY_F_OK==(int)PO64_OK&&(int)STY_F_INVALID==(int)PO64_INVALID&&
 (int)STY_F_DENIED==(int)PO64_DENIED&&(int)STY_F_STALE==(int)PO64_STALE&&
 (int)STY_F_BUSY==(int)PO64_BUSY&&(int)STY_F_FULL==(int)PO64_FULL&&
 (int)STY_F_IO==(int)PO64_IO&&(int)STY_F_CORRUPT==(int)PO64_CORRUPT&&
 (int)STY_F_CANCELLED==(int)PO64_CANCELLED&&(int)STY_F_UNCERTAIN==(int)PO64_UNCERTAIN&&
 (int)STY_F_END==(int)PO64_END,"file status parity");
static uint64_t args[4];static unsigned calls,checks;
#define C(x) do{assert(x);++checks;}while(0)
struct sty_reply sty_call(uint64_t op,uint64_t arg,uint64_t addr,uint64_t len){
 args[0]=op;args[1]=arg;args[2]=addr;args[3]=len;++calls;
 return (struct sty_reply){STY_EMPTY,0};
}
static void put(uint8_t *w,unsigned off,uint32_t v){
 for(unsigned i=0;i<4;++i)w[off+i]=(uint8_t)(v>>(8*i));
}
int main(void){
 uint8_t wire[64]={0};struct sty_event e={0},old;
 put(wire,0,1);put(wire,4,7);put(wire,8,9);put(wire,12,STY_TEXT);put(wire,28,2);
 wire[32]=0xd0;wire[33]=0x90;
 C(sty_decode_event(wire,64,&e)&&e.sequence==7&&e.window==9&&e.kind==STY_TEXT&&e.size==2&&e.bytes[0]==0xd0&&e.bytes[1]==0x90);
 memcpy(&old,&e,sizeof e);
 for(size_t n=0;n<64;++n){C(!sty_decode_event(wire,n,&e));C(!memcmp(&e,&old,sizeof e));}
 for(unsigned off=34;off<64;++off){wire[off]=1;C(!sty_decode_event(wire,64,&e));C(!memcmp(&e,&old,sizeof e));wire[off]=0;}
 put(wire,0,2);C(!sty_decode_event(wire,64,&e));put(wire,0,1);
 put(wire,12,5);C(!sty_decode_event(wire,64,&e));put(wire,12,STY_TEXT);
 put(wire,28,33);C(!sty_decode_event(wire,64,&e));put(wire,28,2);
 put(wire,24,2);C(!sty_decode_event(wire,64,&e));put(wire,24,0);
 C(!memcmp(&e,&old,sizeof e));
 C(sty_window_input(0,wire,63)==STY_INVALID&&calls==0);
 C(sty_window_input(9,wire,64)==STY_EMPTY&&calls==1&&args[0]==STY_INPUT&&args[1]==9&&args[2]==(uintptr_t)wire&&args[3]==64);
 struct sty_rect rect={0,0,160,120,0xabcdef};
 C(sty_window_frame(0,&rect,0)==STY_INVALID&&sty_window_frame(0,&rect,33)==STY_INVALID&&calls==1);
 C(sty_window_frame(0,&rect,1)==STY_EMPTY&&calls==2&&args[0]==STY_FRAME&&args[2]==(uintptr_t)&rect&&args[3]==1);
 uint32_t px=0;C(sty_window_tile(0,&px,17,1)==STY_INVALID&&calls==2);
 C(sty_window_tile(0,&px,1,1)==STY_EMPTY&&calls==3&&args[0]==STY_TILE&&args[3]==0x10001);
 C(sty_window_pixel(0,0x1000000)==STY_INVALID&&calls==3);
 C(sty_window_pixel(0,0xffffff)==STY_EMPTY&&calls==4&&args[0]==STY_PIXEL&&args[2]==0xffffff);
 C(sty_yield()==STY_EMPTY&&calls==5&&args[0]==STY_YIELD&&args[1]==0&&args[2]==0&&args[3]==0);
 uint8_t patch[STY_PATCH_BYTES];memset(patch,0x5a,sizeof patch);
 C(!sty_patch_encode(patch,sizeof patch,5,1,0,1,1,&px)&&patch[0]==0x5a);
 px=0x112233;
 C(sty_patch_encode(patch,sizeof patch,5,16,32,1,1,&px));
 C(sty_le32(patch)==1&&sty_le32(patch+4)==5&&sty_le32(patch+8)==16&&sty_le32(patch+12)==32&&sty_le32(patch+24)==px&&sty_le32(patch+28)==0&&patch[1047]==0);
 C(sty_window_patch(0,patch,1047).status==STY_INVALID&&calls==5);
 C(sty_window_patch(7,patch,sizeof patch).status==STY_EMPTY&&calls==6&&args[0]==STY_PATCH&&args[1]==7&&args[2]==(uintptr_t)patch&&args[3]==STY_PATCH_BYTES);
 C(sty_window_generation(7).status==STY_EMPTY&&calls==7&&args[0]==STY_FRAME_GEN&&args[1]==7&&args[2]==0&&args[3]==0);
 uint8_t text[STY_TEXT_HEADER+STY_TEXT_RUN_BYTES];memset(text,0x5a,sizeof text);
 struct sty_text_run line={.x=4,.y=6,.width=90,.foreground=0xffffff,.count=1,.cp={0x421}};
 C(sty_text_wire_size(0)==12&&sty_text_wire_size(8)==2252&&!sty_text_wire_size(9));
 line.cp[0]=0xd800;
 C(!sty_text_encode(text,sizeof text,5,&line,1)&&text[0]==0x5a);
 line.cp[0]=0x421;
 C(sty_text_encode(text,sizeof text,5,&line,1));
 C(sty_le32(text)==1&&sty_le32(text+4)==5&&sty_le32(text+8)==1&&sty_le32(text+36)==0x421&&text[291]==0);
 C(sty_window_text(0,text,291).status==STY_INVALID&&calls==7);
 C(sty_window_text(7,text,sizeof text).status==STY_EMPTY&&calls==8&&args[0]==STY_TEXT_LAYER&&args[1]==7&&args[2]==(uintptr_t)text&&args[3]==sizeof text);
 C(sty_text_encode(text,12,6,0,0)&&sty_le32(text+8)==0);
 C(sty_file_read(0,1).status==STY_F_INVALID&&sty_file_read(1,0).status==STY_F_INVALID&&calls==8);
 C(sty_file_read(UINT64_C(0x100000001),4096).status==STY_EMPTY&&calls==9&&args[0]==NF91_READ&&args[1]==UINT64_C(0x100000001)&&args[2]==0&&args[3]==4096);
 C(sty_file_write(1,0,1).status==STY_F_INVALID&&sty_file_write(1,wire,0).status==STY_F_INVALID&&calls==9);
 C(sty_file_write(1,wire,64).status==STY_EMPTY&&calls==10&&args[0]==NF91_WRITE&&args[2]==(uintptr_t)wire&&args[3]==64);
 C(sty_file_write(1,0,0).status==STY_EMPTY&&calls==11&&args[2]==0&&args[3]==0);
 C(sty_file_take(1,0,1).status==STY_F_INVALID&&calls==11);
 C(sty_file_take(1,wire,64).status==STY_EMPTY&&calls==12&&args[0]==NF91_TAKE&&args[1]==1&&args[2]==(uintptr_t)wire&&args[3]==64);
 C(sty_file_cancel(1).status==STY_EMPTY&&calls==13&&args[0]==NF91_CANCEL&&args[2]==0&&args[3]==0);
 enum sty_file_status status=STY_F_INVALID;size_t bytes=999;
 C(!sty_file_completion((struct sty_reply){STY_F_BUSY,0},&status,&bytes)&&bytes==999&&status==STY_F_INVALID);
 C(!sty_file_completion((struct sty_reply){0,((uint64_t)4097<<32)},&status,&bytes)&&bytes==999);
 C(!sty_file_completion((struct sty_reply){0,((uint64_t)1<<32)|STY_F_STALE},&status,&bytes)&&bytes==999);
 C(sty_file_completion((struct sty_reply){0,((uint64_t)513<<32)},&status,&bytes)&&bytes==513&&status==STY_F_OK);
 C(sty_file_completion((struct sty_reply){0,STY_F_UNCERTAIN},&status,&bytes)&&!bytes&&status==STY_F_UNCERTAIN);
 printf("Native SDK 77: %u checks PASS (mock transport, no guest claim)\n",checks);
 return 0;
}
