/* Independent native read-only Explorer, no kernel headers or fake rows.
 * Expanded text SDK: independent name/type/size columns.
 * Trusted launcher must authorize a real root and supply 640x400 client bounds. */
#include <styleos/explorer_v1.h>
#include <styleos/graphics_v1.h>
enum { WIDTH=640,HEIGHT=400,ROWS=8 };
struct view {
 struct sty_explorer_row rows[ROWS];
 struct sty_explorer_volume volume;
 uint64_t first,status;
 unsigned count,selected,depth;
 bool more,computer,warm;
};
static struct view v;
static uint8_t wire[STY_EXPLORER_ROW_BYTES],input[STY_INPUT_BYTES],text[STY_TEXT_WIRE_MAX];
static struct sty_rect frame[16];
static struct sty_text_run runs[STY_TEXT_RUNS];
struct theme {uint32_t body,panel,bar,selection,fg,accent;};
static const struct theme palettes[]={
 {0x303238,0x25272c,0x1f2125,0x454c57,0xe8e9eb,0x9eabbc},
 {0x46423d,0x312e2a,0x282521,0x61584d,0xede5d8,0xe4bd83}
};
static uint64_t request(unsigned op,uint64_t ordinal,size_t n){
 struct sty_reply q=sty_explorer_submit(op,ordinal);
 if(q.status!=STY_F_OK)return q.status;
 if(!q.value)return STY_F_CORRUPT;
 for(;;){
  struct sty_reply r=sty_explorer_take(q.value,n?wire:0,n);
  if(r.status!=STY_F_BUSY)return r.status;
  (void)sty_yield();
 }
}
static void load(void){
 v.count=0;v.selected=0;v.more=false;
 if(v.computer){
  v.status=request(STY_EX_VOLUME,0,STY_EXPLORER_VOLUME_BYTES);
  if(v.status==STY_F_OK&&!sty_explorer_volume_decode(wire,STY_EXPLORER_VOLUME_BYTES,&v.volume))v.status=STY_F_CORRUPT;
  return;
 }
 for(unsigned i=0;i<=ROWS;++i){
  if(v.first>UINT64_MAX-i){v.status=STY_F_INVALID;v.count=0;return;}
  uint64_t s=request(STY_EX_ROW,v.first+i,sizeof wire);
  if(s==STY_F_END){v.status=STY_F_OK;return;}
  if(s!=STY_F_OK){v.count=0;v.status=s;return;}
  struct sty_explorer_row item;
  if(!sty_explorer_decode(wire,sizeof wire,&item)){v.count=0;v.status=STY_F_CORRUPT;return;}
  if(i==ROWS){v.more=true;v.status=STY_F_OK;return;}
  v.rows[v.count++]=item;
 }
}
static void change_directory(bool up){
 if(up&&!v.depth)return;
 if(!up&&(v.selected>=v.count||v.rows[v.selected].kind!=2))return;
 uint64_t s=request(up?STY_EX_UP:STY_EX_ENTER,up?0:v.first+v.selected,0);
 if(s!=STY_F_OK){v.status=s;v.count=0;return;}
 if(up)--v.depth;else ++v.depth;
 v.first=0;load();
}
static void refresh(void){
 struct sty_reply q=sty_ex_refresh();
 if(q.status==STY_F_OK&&q.value){
  do{
   q.status=sty_explorer_take(q.value,wire,STY_EX_RECEIPT_BYTES).status;
   if(q.status==STY_F_BUSY)(void)sty_yield();
  }while(q.status==STY_F_BUSY);
  if(q.status==STY_F_OK){
   struct sty_ex_receipt at;
   if(!sty_ex_receipt_decode(wire,STY_EX_RECEIPT_BYTES,&at))q.status=STY_F_CORRUPT;
   else{v.depth=0;v.first=0;}
  }
 }
 if(q.status!=STY_F_OK){v.status=q.status;v.count=0;return;}
 load();
}
static unsigned utf(struct sty_text_run *r,unsigned at,const uint8_t *p,size_t n,unsigned limit){
 while(n&&at<limit&&at<STY_TEXT_GLYPHS){
  uint32_t cp;size_t k;
  if(!sty_utf8_next(p,n,&cp,&k))break;
  r->cp[at++]=cp;p+=k;n-=k;
 }
 return at;
}
static unsigned literal(struct sty_text_run *r,unsigned at,const char *s){
 size_t n=0;while(s[n])++n;return utf(r,at,(const uint8_t *)s,n,STY_TEXT_GLYPHS);
}
static unsigned decimal(struct sty_text_run *r,unsigned at,uint64_t x){
 uint8_t d[20];unsigned n=0;
 do{d[n++]=(uint8_t)('0'+x%10);x/=10;}while(x);
 while(n&&at<STY_TEXT_GLYPHS)r->cp[at++]=d[--n];
 return at;
}
static unsigned nr,nt;
static void rect(unsigned x,unsigned y,unsigned w,unsigned h,uint32_t rgb){
 frame[nr++]=(struct sty_rect){x,y,w,h,rgb};
}
static struct sty_text_run *line(unsigned x,unsigned y,unsigned width,uint32_t fg,uint32_t bg){
 struct sty_text_run *r=&runs[nt++];*r=(struct sty_text_run){.x=x,.y=y,.width=width,.foreground=fg,.background=bg};return r;
}
static void label(unsigned x,unsigned y,unsigned width,uint32_t fg,uint32_t bg,const char *s){
 struct sty_text_run *r=line(x,y,width,fg,bg);r->count=literal(r,0,s);
}
static const char *status(void){
 switch(v.status){
 case STY_F_OK:return v.computer?"Сведения о томе":v.count?"Enter: открыть  /  Backspace: вверх":"Пустая папка";
 case STY_F_DENIED:return "Нет разрешения";
 case STY_F_STALE:return "Данные изменились — F5";
 case STY_F_END:return "Нет доступного тома";
 case STY_F_BUSY:return "Занято";
 case STY_F_IO:return "Ошибка чтения";
 default:return "Данные недоступны";
 }
}
static bool render(void){
 const struct theme *t=&palettes[v.warm?1:0];
 nr=nt=0;
 rect(0,0,WIDTH,HEIGHT,t->body);
 rect(0,0,WIDTH,36,t->bar);rect(0,36,174,HEIGHT-36,t->panel);
 rect(174,36,WIDTH-174,40,t->panel);rect(0,370,WIDTH,30,t->bar);
 rect(4,v.computer?112:80,166,28,t->selection);
 if(!v.computer&&v.status==STY_F_OK&&v.count)
  rect(184,116+v.selected*28,446,26,t->selection);
 label(12,8,600,t->fg,t->bar,"Проводник  |  F5 обновить  /  F6 тема");
 label(12,84,150,t->fg,v.computer?t->panel:t->selection,"Каталог");
 label(12,116,150,t->fg,v.computer?t->selection:t->panel,"Мой компьютер");
 label(188,45,434,t->fg,t->panel,v.computer?"Мой компьютер":"Каталог StyleFS");
 if(!v.computer){
  rect(184,82,446,28,t->bar);
  label(188,86,234,t->fg,t->bar,"Имя");
  label(426,86,72,t->fg,t->bar,"Тип");
  label(502,86,126,t->fg,t->bar,"Размер");
 }
 if(v.computer&&v.status==STY_F_OK){
  struct sty_text_run *r=line(188,110,434,t->fg,t->body);
  r->count=decimal(r,literal(r,0,"Том: "),v.volume.volume_bytes);
  r->count=literal(r,r->count," байт");
  r=line(188,150,434,t->fg,t->body);
  r->count=decimal(r,literal(r,0,"Свободно: "),v.volume.free_bytes);
  r->count=literal(r,r->count," байт");
  label(188,190,434,t->accent,t->body,v.volume.writable?"Доступен для записи":"Только чтение");
  rect(188,226,420,8,t->panel);
  uint64_t scale=v.volume.volume_bytes/420+1;
  unsigned fill=(unsigned)(v.volume.free_bytes/scale);
  if(fill)rect(188,226,fill,8,t->accent);
 }else if(!v.computer&&v.status==STY_F_OK){
  /* Seven headings, eight rows x three columns, one footer. */
  for(unsigned i=0;i<v.count;++i){
   const struct sty_explorer_row *a=&v.rows[i];
   unsigned y=119+i*28;
   uint32_t bg=i==v.selected?t->selection:t->body;
   struct sty_text_run *r=line(188,y,234,t->fg,bg);
   r->count=utf(r,0,a->name,a->name_bytes,22);
   label(426,y,72,t->fg,bg,a->kind==2?"Папка":"Файл");
   r=line(502,y,126,t->fg,bg);
   if(a->kind==2)r->count=literal(r,0,"—");
   else {
    uint64_t n=a->bytes;unsigned unit=0;
    const char *units[]={" Б"," КиБ"," МиБ"," ГиБ"," ТиБ"," ПиБ"," ЭиБ"};
    while(n>=1024&&unit<6){n/=1024;++unit;}
    r->count=decimal(r,0,n);r->count=literal(r,r->count,units[unit]);
   }
  }
 }
 label(12,375,616,t->fg,t->bar,status());
 if(sty_window_frame(0,frame,nr)!=STY_OK)return false;
 struct sty_reply g=sty_window_generation(0);
 if(g.status!=STY_OK||!g.value||g.value>UINT32_MAX)return false;
 size_t n=sty_text_wire_size(nt);
 return sty_text_encode(text,n,(uint32_t)g.value,runs,nt)&&sty_window_text(0,text,n).status==STY_OK;
}
static void key(uint32_t k){
 if(k==0x3f){refresh();return;} /* F5 */
 if(k==0x40){v.warm=!v.warm;return;} /* F6 */
 if(k==0x0f){v.computer=!v.computer;load();return;} /* Tab */
 if(v.computer)return;
 if(v.status!=STY_F_OK)return;
 if(k==0x1c){change_directory(false);return;}
 if(k==0x0e){change_directory(true);return;}
 if(k==0x150||k==0x50){
  if(v.selected+1<v.count)++v.selected;
  else if(v.more&&v.first<=UINT64_MAX-v.count){v.first+=v.count;load();}
 }else if(k==0x148||k==0x48){
  if(v.selected)--v.selected;
  else if(v.first){v.first=v.first>=ROWS?v.first-ROWS:0;load();}
 }
}
void explorer_main(void){
 load();
 for(;;){
  if(!render())return;
  for(;;){
   uint64_t s=sty_window_input(0,input,sizeof input);
   if(s==STY_EMPTY){(void)sty_yield();continue;}
   if(s!=STY_OK)return;
   struct sty_event ev;
   if(!sty_decode_event(input,sizeof input,&ev))continue;
   if(ev.kind==STY_KEY&&ev.down){if(ev.code==0x01)return;key(ev.code);break;}
   if(ev.kind==STY_POINTER&&ev.down){
    int32_t x=(int32_t)sty_le32(ev.bytes),y=(int32_t)sty_le32(ev.bytes+4);
    if(x>=0&&x<174&&y>=80&&y<140){v.computer=y>=112;load();break;}
    if(!v.computer&&v.status==STY_F_OK&&x>=184&&x<630&&y>=116&&y<116+ROWS*28){
     unsigned i=(unsigned)(y-116)/28;
     if(i<v.count){v.selected=i;break;}
    }
   }
  }
 }
}
