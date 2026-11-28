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

/* Stage 79: bounded local raster; no kernel state or heap. */
static bool region(const void *p,size_t n){
 return p&&n&&n<=UINTPTR_MAX-(uintptr_t)p;
}
static bool disjoint(const void *a,size_t n,const void *b,size_t m){
 return region(a,n)&&region(b,m)&&
 ((uintptr_t)a+n<=(uintptr_t)b||(uintptr_t)b+m<=(uintptr_t)a);
}
static bool shape(uint32_t w,uint32_t h,uint32_t stride,size_t cap){
 return w&&h&&w<=stride&&stride<=STY_SURFACE_PIXELS&&
 cap<=STY_SURFACE_PIXELS&&h<=cap/stride;
}
static bool surface_valid(const struct sty_surface *s){
 if(!s||!shape(s->width,s->height,s->stride,s->capacity)||
    (uintptr_t)s->pixels%_Alignof(uint32_t)||
    !disjoint(s,sizeof *s,s->pixels,s->capacity*sizeof(uint32_t)))return false;
 const struct sty_damage *d=&s->dirty;
 return (!d->w&&!d->h&&!d->x&&!d->y)||
 (d->w&&d->h&&d->x<s->width&&d->y<s->height&&
  d->w<=s->width-d->x&&d->h<=s->height-d->y);
}
bool sty_surface_begin(struct sty_surface *s,uint32_t *pixels,size_t cap,
 uint32_t w,uint32_t h,uint32_t stride,uint32_t rgb){
 if(!s||rgb>0xffffffu||!shape(w,h,stride,cap)||
    (uintptr_t)pixels%_Alignof(uint32_t)||
    !disjoint(s,sizeof *s,pixels,cap*sizeof(uint32_t)))return false;
 /* Clear padding as well; unused capacity beyond height stays untouched. */
 for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<stride;++x)
  pixels[(size_t)y*stride+x]=x<w?rgb:0;
 *s=(struct sty_surface){pixels,cap,w,h,stride,{0,0,w,h}};
 return true;
}
static void changed(struct sty_surface *s,uint32_t x,uint32_t y){
 struct sty_damage *d=&s->dirty;
 if(!d->w){*d=(struct sty_damage){x,y,1,1};return;}
 uint32_t r=d->x+d->w,b=d->y+d->h;
 if(x<d->x)d->x=x;
 if(y<d->y)d->y=y;
 if(x>=r)r=x+1;
 if(y>=b)b=y+1;
 d->w=r-d->x;d->h=b-d->y;
}
static void plot(struct sty_surface *s,uint32_t x,uint32_t y,uint32_t rgb){
 uint32_t *p=&s->pixels[(size_t)y*s->stride+x];
 if(*p!=rgb){*p=rgb;changed(s,x,y);}
}
bool sty_surface_fill(struct sty_surface *s,int32_t x,int32_t y,
 uint32_t w,uint32_t h,uint32_t rgb){
 if(!surface_valid(s)||!w||!h||rgb>0xffffffu)return false;
 int64_t l=x,t=y,r=(int64_t)x+w,b=(int64_t)y+h;
 if(l<0)l=0;
 if(t<0)t=0;
 if(r>s->width)r=s->width;
 if(b>s->height)b=s->height;
 for(int64_t yy=t;yy<b;++yy)for(int64_t xx=l;xx<r;++xx)
  plot(s,(uint32_t)xx,(uint32_t)yy,rgb);
 return true;
}
static uint32_t blend(uint32_t old,uint32_t rgb,unsigned a){
 if(a==3)return rgb;
 uint32_t result=0;
 for(unsigned shift=0;shift<24;shift+=8){
  unsigned o=(old>>shift)&255u,n=(rgb>>shift)&255u;
  result|=((o*(3u-a)+n*a)/3u)<<shift;
 }
 return result;
}
static const struct desk_font_glyph *glyph(uint32_t cp){
 unsigned i=cp>=32&&cp<127?cp-32:cp==0x401?95:
 cp>=0x410&&cp<=0x44f?96+cp-0x410:cp==0x451?160:63-32;
 return &desk_font[i];
}
bool sty_surface_text(struct sty_surface *s,int32_t x,int32_t y,
 const uint8_t *p,size_t n,uint32_t rgb){
 if(!surface_valid(s)||rgb>0xffffffu||
    (n&&(!disjoint(p,n,s,sizeof *s)||
          !disjoint(p,n,s->pixels,s->capacity*sizeof(uint32_t)))))return false;
 /* Validate the whole immutable input before writing a single pixel.
  * Text measure also checks scalar/length and accumulation overflow. */
 uint32_t width;
 if(!sty_text_measure(p,n,&width))return false;
 int64_t xx=x;
 while(n){
  uint32_t cp;size_t used;
  if(!sty_utf8_next(p,n,&cp,&used))return false;
  const struct desk_font_glyph *f=glyph(cp);
  for(unsigned row=0;row<20;++row){
   int64_t yy=(int64_t)y+row;
   if(yy<0||yy>=s->height)continue;
   uint32_t mask=f->rows[row];
   for(unsigned col=0;mask;mask>>=2,++col){
    int64_t px=xx+col;unsigned a=mask&3u;
    if(!a||px<0||px>=s->width)continue;
    uint32_t old=s->pixels[(size_t)yy*s->stride+(size_t)px];
    plot(s,(uint32_t)px,(uint32_t)yy,blend(old,rgb,a));
   }
  }
  xx+=f->step;p+=used;n-=used;
 }
 return true;
}
bool sty_surface_damage(const struct sty_surface *s,struct sty_damage *out){
 if(!surface_valid(s)||!out||!disjoint(out,sizeof *out,s,sizeof *s)||
    !disjoint(out,sizeof *out,s->pixels,s->capacity*sizeof(uint32_t)))return false;
 *out=s->dirty;return true;
}
bool sty_surface_clear_damage(struct sty_surface *s){
 if(!surface_valid(s))return false;
 s->dirty=(struct sty_damage){0};return true;
}
