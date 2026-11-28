#include <styleos/graphics_v1.h>
/* Reuse existing approved font source; no new font asset or kernel code.
 * DejaVu-derived StyleOS Desktop Sans, docs/licenses/desktop-font.txt. */
#include "../../kernel/desktop_font.inc"
static bool valid_canvas(const struct sty_canvas *c){
 return c&&c->width&&c->height&&c->width<=8192&&c->height<=8192&&
        c->count&&c->count<=STY_FRAME_RECTS&&
        !c->rect[0].x&&!c->rect[0].y&&c->rect[0].w==c->width&&c->rect[0].h==c->height;
}
bool sty_canvas_begin(struct sty_canvas *c,uint32_t w,uint32_t h,uint32_t rgb){
 if(!c||!w||!h||w>8192||h>8192||rgb>0xffffffu)return false;
 c->width=w;c->height=h;c->count=1;
 for(unsigned i=0;i<STY_FRAME_RECTS;++i)c->rect[i]=(struct sty_rect){0};
 c->rect[0]=(struct sty_rect){0,0,w,h,rgb};
 return true;
}
enum sty_draw_result sty_canvas_rect(struct sty_canvas *c,int32_t x,int32_t y,
 uint32_t w,uint32_t h,uint32_t rgb){
 if(!valid_canvas(c)||rgb>0xffffffu||!w||!h)return STY_DRAW_INVALID;
 int64_t x0=x,y0=y,x1=(int64_t)x+w,y1=(int64_t)y+h;
 if(x0<0)x0=0;
 if(y0<0)y0=0;
 if(x1>c->width)x1=c->width;
 if(y1>c->height)y1=c->height;
 if(x1<=x0||y1<=y0)return STY_DRAW_OK;
 if(c->count==STY_FRAME_RECTS)return STY_DRAW_FULL;
 c->rect[c->count++]=(struct sty_rect){(uint32_t)x0,(uint32_t)y0,
   (uint32_t)(x1-x0),(uint32_t)(y1-y0),rgb};
 return STY_DRAW_OK;
}
uint64_t sty_canvas_publish(uint32_t window,const struct sty_canvas *c){
 if(!valid_canvas(c))return STY_INVALID;
 for(unsigned i=0;i<c->count;++i){
  const struct sty_rect *r=&c->rect[i];
  if(!r->w||!r->h||r->x>=c->width||r->y>=c->height||
     r->w>c->width-r->x||r->h>c->height-r->y||r->rgb>0xffffffu)return STY_INVALID;
 }
 return sty_window_frame(window,c->rect,c->count);
}
static bool scalar(uint32_t cp){return cp<=0x10ffffu&&(cp<0xd800u||cp>0xdfffu);}
bool sty_utf8_next(const uint8_t *p,size_t n,uint32_t *cp,size_t *used){
 if(!p||!n||!cp||!used)return false;
 uint32_t v=p[0],min=0;size_t k=1;
 if(v<0x80u){}
 else if(v>=0xc2u&&v<=0xdfu){v&=31u;k=2;min=0x80u;}
 else if(v>=0xe0u&&v<=0xefu){v&=15u;k=3;min=0x800u;}
 else if(v>=0xf0u&&v<=0xf4u){v&=7u;k=4;min=0x10000u;}
 else return false;
 if(n<k)return false;
 for(size_t i=1;i<k;++i){
  if((p[i]&0xc0u)!=0x80u)return false;
  v=(v<<6)|(p[i]&63u);
 }
 if(v<min||!scalar(v))return false;
 *cp=v;*used=k;return true;
}
bool sty_glyph_advance(uint32_t cp,uint32_t *advance){
 if(!advance||!scalar(cp))return false;
 const size_t n=sizeof desk_font/sizeof desk_font[0];
 uint32_t fallback=0;
 for(size_t i=0;i<n;++i){
  if(desk_font[i].cp==cp){*advance=desk_font[i].step;return true;}
  if(desk_font[i].cp==63)fallback=desk_font[i].step;
 }
 if(!fallback)return false;
 *advance=fallback;return true;
}
bool sty_text_measure(const uint8_t *p,size_t n,uint32_t *width){
 if(!width||(!p&&n))return false;
 uint32_t total=0;
 while(n){
  uint32_t cp,advance;size_t k;
  if(!sty_utf8_next(p,n,&cp,&k)||cp<32||cp==127||
     !sty_glyph_advance(cp,&advance)||advance>UINT32_MAX-total)return false;
  total+=advance;p+=k;n-=k;
 }
 *width=total;return true;
}
