#include <styleos/graphics_v1.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned checks,calls;
#define C(x) do{assert(x);++checks;}while(0)
struct sty_reply sty_call(uint64_t op,uint64_t arg,uint64_t address,uint64_t length){
 (void)arg;assert(op==STY_FRAME&&address&&length);++calls;
 return (struct sty_reply){STY_OK,0};
}
static void bad(const uint8_t *p,size_t n){
 uint32_t cp=0xdead;size_t used=99;
 C(!sty_utf8_next(p,n,&cp,&used)&&cp==0xdead&&used==99);
}
int main(void){
 struct sty_canvas c={0},old;
 C(!sty_canvas_begin(&c,0,120,0));
 C(sty_canvas_begin(&c,160,120,0x010203));
 C(c.count==1&&c.rect[0].w==160&&c.rect[0].h==120);
 C(sty_canvas_rect(&c,-10,-5,20,15,0xabcdef)==STY_DRAW_OK);
 C(c.count==2&&c.rect[1].x==0&&c.rect[1].y==0&&c.rect[1].w==10&&c.rect[1].h==10);
 C(sty_canvas_rect(&c,150,110,UINT32_MAX,UINT32_MAX,0)==STY_DRAW_OK);
 C(c.count==3&&c.rect[2].w==10&&c.rect[2].h==10);
 C(sty_canvas_rect(&c,INT32_MAX,INT32_MAX,UINT32_MAX,UINT32_MAX,0)==STY_DRAW_OK&&c.count==3);
 C(sty_canvas_rect(&c,INT32_MIN,INT32_MIN,1,1,0)==STY_DRAW_OK&&c.count==3);
 memcpy(&old,&c,sizeof c);C(sty_canvas_rect(&c,0,0,1,1,0x1000000)==STY_DRAW_INVALID&&!memcmp(&c,&old,sizeof c));
 for(unsigned i=3;i<STY_FRAME_RECTS;++i)C(sty_canvas_rect(&c,1,1,1,1,0)==STY_DRAW_OK);
 memcpy(&old,&c,sizeof c);C(sty_canvas_rect(&c,0,0,1,1,0)==STY_DRAW_FULL&&!memcmp(&c,&old,sizeof c));
 C(sty_canvas_publish(0,&c)==STY_OK&&calls==1);
 c.rect[5].w=UINT32_MAX;C(sty_canvas_publish(0,&c)==STY_INVALID&&calls==1);
 uint8_t ru[]={0xd0,0x90,0xd1,0x91};
 uint32_t cp=0,a=0,b=0,w=0;size_t used=0;
 C(sty_utf8_next(ru,4,&cp,&used)&&cp==0x410&&used==2);
 C(sty_utf8_next(ru+2,2,&cp,&used)&&cp==0x451&&used==2);
 C(sty_glyph_advance(0x410,&a)&&sty_glyph_advance(0x451,&b));
 C(sty_text_measure(ru,4,&w)&&w==a+b);
 C(sty_glyph_advance('?',&a)&&sty_glyph_advance(0x1f642,&b)&&a==b);
 const uint8_t emoji[]={0xf0,0x9f,0x99,0x82};
 C(sty_utf8_next(emoji,4,&cp,&used)&&cp==0x1f642&&used==4);
 for(size_t i=0;i<4;++i)bad(emoji,i);
 const uint8_t invalid[][4]={{0xc0,0x80},{0xe0,0x80,0x80},{0xed,0xa0,0x80},{0xf4,0x90,0x80,0x80},{0xf5,0x80,0x80,0x80},{0x80},{0xe2,0x41,0x80}};
 for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];++i)bad(invalid[i],4);
 for(unsigned i=0;i<128;++i){uint8_t x=(uint8_t)i;C(sty_utf8_next(&x,1,&cp,&used)&&cp==i&&used==1);}
 w=123;C(!sty_text_measure(invalid[0],2,&w)&&w==123);
 C(!sty_text_measure((const uint8_t *)"\n",1,&w)&&w==123);
 C(sty_text_measure(NULL,0,&w)&&w==0);
 printf("Native graphics 78: %u checks PASS (host only)\n",checks);
 return 0;
}
