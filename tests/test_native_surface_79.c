#include <styleos/graphics_v1.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned checks;
#define C(x) do{assert(x);++checks;}while(0)
struct sty_reply sty_call(uint64_t a,uint64_t b,uint64_t c,uint64_t d){
 (void)a;(void)b;(void)c;(void)d;assert(0);return (struct sty_reply){0,0};
}
static uint32_t pixels[32*24+2],old[32*24+2];
static struct sty_surface s;
static void damage(uint32_t x,uint32_t y,uint32_t w,uint32_t h){
 struct sty_damage d={0};
 C(sty_surface_damage(&s,&d)&&d.x==x&&d.y==y&&d.w==w&&d.h==h);
}
int main(void){
 for(unsigned i=0;i<sizeof pixels/sizeof *pixels;++i)pixels[i]=0xdeadbeef;
 C(!sty_surface_begin(&s,pixels+1,10,30,24,32,0));
 C(!sty_surface_begin(&s,pixels+1,768,33,24,32,0));
 C(sty_surface_begin(&s,pixels+1,768,30,24,32,0x112233));
 damage(0,0,30,24);
 C(pixels[0]==0xdeadbeef&&pixels[769]==0xdeadbeef);
 for(unsigned y=0;y<24;++y)for(unsigned x=0;x<32;++x)
  C(pixels[1+y*32+x]==(x<30?0x112233u:0u));
 C(sty_surface_clear_damage(&s));damage(0,0,0,0);
 C(sty_surface_fill(&s,-4,-2,7,5,0xabcdef));damage(0,0,3,3);
 C(sty_surface_fill(&s,28,22,UINT32_MAX,UINT32_MAX,0x556677));damage(0,0,30,24);
 C(sty_surface_clear_damage(&s));
 C(sty_surface_fill(&s,28,22,2,2,0x556677));damage(0,0,0,0);
 C(sty_surface_fill(&s,INT32_MAX,INT32_MAX,UINT32_MAX,UINT32_MAX,0));damage(0,0,0,0);
 memcpy(old,pixels,sizeof old);
 const uint8_t bad[]={'A',0xed,0xa0,0x80};
 C(!sty_surface_text(&s,0,0,bad,sizeof bad,0xffffff));
 C(!memcmp(old,pixels,sizeof old));damage(0,0,0,0);
 C(!sty_surface_text(&s,0,0,(uint8_t *)(pixels+1),1,0xffffff));
 C(!sty_surface_damage(&s,(struct sty_damage *)(pixels+1)));
 C(sty_surface_fill(&s,0,0,30,24,0));
 C(sty_surface_clear_damage(&s));
 const uint8_t text[]={0xd0,0x90,0xd1,0x91};
 C(sty_surface_text(&s,-2,1,text,sizeof text,0xffffff));
 struct sty_damage d;
 C(sty_surface_damage(&s,&d)&&d.w&&d.h&&d.x+d.w<=30&&d.y+d.h<=21);
 unsigned ink=0,soft=0;
 for(unsigned y=0;y<24;++y)for(unsigned x=0;x<32;++x){
  uint32_t p=pixels[1+y*32+x];
  if(p){++ink;C(x<30&&x>=d.x&&x<d.x+d.w&&y>=d.y&&y<d.y+d.h);}
  if(p&&p!=0xffffff)++soft;
 }
 C(ink>0&&soft>0);
 C(sty_surface_clear_damage(&s));damage(0,0,0,0);
 memcpy(old,pixels,sizeof old);
 C(sty_surface_text(&s,INT32_MAX,INT32_MIN,text,sizeof text,0xffffff));
 C(!memcmp(old,pixels,sizeof old));damage(0,0,0,0);
 C(pixels[0]==0xdeadbeef&&pixels[769]==0xdeadbeef);
 printf("Native surface 79: %u checks PASS (local raster only)\n",checks);
 return 0;
}
