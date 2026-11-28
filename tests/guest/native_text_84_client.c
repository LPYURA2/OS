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
 /* Frame and aligned patch: wire crosses a page boundary without
  * touching the input receipt, end guard or release flag. */
 struct sty_rect *frame=(struct sty_rect *)(uintptr_t)0x12000000u;
 uint8_t *packet=(uint8_t *)(uintptr_t)0x12001e00u;
 uint32_t *pixels=(uint32_t *)(uintptr_t)0x12000400u;
 *frame=(struct sty_rect){0,0,160,120,0x224466};
 if(sty_window_frame(0,frame,1)!=STY_OK)fail();
 struct sty_reply r=sty_window_generation(0);
 if(r.status!=STY_OK||r.value!=2)fail();
 struct sty_text_run *line=(struct sty_text_run *)(uintptr_t)0x12000800u;
 line->x=8;line->y=8;line->width=120;line->foreground=0xffffff;line->background=0x263345;line->count=6;
 line->cp[0]=0x421;line->cp[1]=0x442;line->cp[2]=0x438;line->cp[3]=0x43b;line->cp[4]=0x44c;line->cp[5]='!';
 for(unsigned i=6;i<STY_TEXT_GLYPHS;++i)line->cp[i]=0;
 uint8_t *text=(uint8_t *)(uintptr_t)0x12001f80u;
 size_t n=sty_text_wire_size(1);
 if(!sty_text_encode(text,n,2,line,1))fail();
 r=sty_window_text(0,(const uint8_t *)(uintptr_t)0x12002f00u,n);
 if(r.status!=STY_INVALID||r.value)fail();
 r=sty_window_text(0,text,n);
 if(r.status!=STY_OK||r.value!=3)fail();
 r=sty_window_text(0,text,n);
 if(r.status!=STY_STALE||r.value)fail();
 sty_put32(text+4,3);
 r=sty_window_text(0,text,n);
 if(r.status!=STY_OK||r.value!=3)fail();
 text[0]=2;
 r=sty_window_text(0,text,n);
 if(r.status!=STY_INVALID||r.value)fail();
 for(unsigned i=0;i<256;++i)pixels[i]=0x91c725;
 if(!sty_patch_encode(packet,STY_PATCH_BYTES,3,32,32,16,16,pixels))fail();
 r=sty_window_patch(0,(const uint8_t *)(uintptr_t)0x12002f00u,STY_PATCH_BYTES);
 if(r.status!=STY_INVALID||r.value)fail();
 r=sty_window_patch(0,packet,STY_PATCH_BYTES);
 if(r.status!=STY_OK||r.value!=4)fail();
 r=sty_window_patch(0,packet,STY_PATCH_BYTES);
 if(r.status!=STY_STALE||r.value)fail();
 sty_put32(packet+4,4);
 r=sty_window_patch(0,packet,STY_PATCH_BYTES);
 if(r.status!=STY_OK||r.value!=4)fail();
 packet[0]=2;
 r=sty_window_patch(0,packet,STY_PATCH_BYTES);
 if(r.status!=STY_INVALID||r.value)fail();
 r=sty_window_generation(0);
 if(r.status!=STY_OK||r.value!=4||*guard!=0x11223344u)fail();
 /* Packet crosses into the third data page; restore wait flag and marker. */
 *(volatile uint8_t *)(uintptr_t)0x12002008u=0;
 *marker=0x66778899u;
 sdk77_wait();
 fail();
}
