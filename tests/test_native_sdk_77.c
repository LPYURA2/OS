#include <styleos/native_v1.h>
#include <styleos/native_window_channel.h>
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
 printf("Native SDK 77: %u checks PASS (mock transport, no guest claim)\n",checks);
 return 0;
}
