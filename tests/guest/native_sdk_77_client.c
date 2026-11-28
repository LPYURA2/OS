/* Independently compiled SDK acceptance client; not a system application. */
#include <styleos/native_v1.h>
extern void sdk77_wait(void);
static void fail(void){__asm__ volatile("ud2");__builtin_unreachable();}
void sdk77_main(void){
 volatile uint32_t *guard=(volatile uint32_t *)(uintptr_t)0x12002ffcu;
 volatile uint32_t *marker=(volatile uint32_t *)(uintptr_t)0x12002000u;
 uint8_t *wire=(uint8_t *)(uintptr_t)0x12000fe0u;
 struct sty_event ev;
 *guard=0x11223344u;
 if(sty_window_input(0,(uint8_t *)(uintptr_t)0x12002fe0u,64)!=STY_INVALID)fail();
 if(*guard!=0x11223344u||sty_window_input(0,wire,64)!=STY_OK)fail();
 if(!sty_decode_event(wire,64,&ev)||ev.sequence!=1||ev.kind!=STY_TEXT||
    ev.size!=2||ev.bytes[0]!=0x5a||!ev.bytes[1]||ev.code||ev.modifiers||ev.down)fail();
 *marker=0x66778899u;
 sdk77_wait();
 fail();
}
