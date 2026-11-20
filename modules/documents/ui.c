#include "styleos/documents_ui.h"
#include <string.h>
#include <stdio.h>
#define NAVY  0x162332u
#define PANEL 0x131e2bu
#define EDGE  0x445366u
#define WARM  0xf6f2e9u
#define INK   0x17263eu
#define MAIN  0xdce2ebu
#define MUTE  0x8798aeu
#define GOLD  0xe3ad4bu
#define TINT  0x29374au

static const char *tabs[8]={"Файл","Текст","Вставка","Страница","Ссылки","Проверка","Автоматизация","Вид"};
static const sdu_screen tab_screens[8]={SDU_FILE,SDU_TEXT,SDU_INSERT,SDU_PAGE,SDU_REFERENCES,SDU_REVIEW,SDU_AUTOMATION,SDU_VIEW};
static const char *ribbon[8][8]={
 {"Новый","Открыть","Сохранить","Сохранить как","Экспорт","Печать","История",NULL},
 {"Шрифт","Размер","Полужирный","Курсив","Абзац","Список","Поиск","Заменить"},
 {"Таблица","Изображение","Фигура","Диаграмма","Формула","Символ","Колонтитул",NULL},
 {"A4","Letter","Ориентация","Поля","Колонки","Разрыв",NULL,NULL},
 {"Сноска","Сноски","Подпись","Оглавление","Перекр. ссылка","Цитата","Библиография",NULL},
 {"Орфография","Грамматика","Язык","Комментарии","Изменения","Сравнить",NULL,NULL},
 {"Шаблоны","Поля","Заготовки","Источник","Слияние","Предпросмотр",NULL,NULL},
 {"Структура","Чтение","Масштаб","Линейка",NULL,NULL,NULL,NULL}
};
static const char *screen_names[SDU_SCREEN_COUNT]={
 "Стартовый экран","Файл","Текст","Вставка","Страница","Ссылки",
 "Проверка","Автоматизация","Вид","Свойства абзаца","Свойства изображения",
 "Открыть документ","Сохранить как","Предпросмотр печати"
};
static int add(sdu_draw *out,size_t cap,size_t *n,sdu_shape type,int x,int y,int w,int h,uint32_t c,
               const char *str,int size,int emphasis){
 if(*n>=cap || *n>=SDU_COMMANDS_MAX)return SDM_CAPACITY;
 sdu_draw *d=&out[(*n)++];
 memset(d,0,sizeof(*d));d->type=type;d->x=x;d->y=y;d->w=w;d->h=h;
 d->rgb=c;d->size=size;d->emphasis=emphasis;
 if(str){size_t len=strlen(str);if(len>=sizeof(d->text))return SDM_CAPACITY;
         memcpy(d->text,str,len+1);}
 return SDM_OK;
}
int sdu_init(sdu_ui *ui,sda_documents_app *app,int w,int h){
 if(!ui||!app||w<800||h<360)return SDM_ARGUMENT;
 ui->app=app;ui->screen=SDU_TEXT;ui->zoom_percent=100;
 ui->width=w;ui->height=h;return SDM_OK;
}
int sdu_set_screen(sdu_ui *ui,sdu_screen screen){
 if(!ui || screen>=SDU_SCREEN_COUNT)return SDM_ARGUMENT;
 ui->screen=screen;return SDM_OK;
}
static size_t glyphs(const char *s){size_t n=0;while(*s){if(((*s)&0xc0)!=0x80)n++;s++;}return n;}
static int cmd(sdu_draw *out,size_t cap,size_t *n,sdu_shape type,int x,int y,int w,int h,
               uint32_t rgb,const char *text,int fs,int bold){
 return add(out,cap,n,type,x,y,w,h,rgb,text,fs,bold);
}
#define DRAW(t,x,y,w,h,color,label,sz,bold) do{if(cmd(out,cap,&count,t,x,y,w,h,color,label,sz,bold)!=SDM_OK)return SDM_CAPACITY;}while(0)
static void cut_utf8_line(const char *s,size_t start,size_t end,char *dst,size_t dstlen){
 size_t n=end-start;
 if(n>=dstlen)n=dstlen-1;
 while(n && (((unsigned char)s[start+n]&0xc0)==0x80))n--;
 memcpy(dst,s+start,n);dst[n]=0;
}
int sdu_render(const sdu_ui *ui,sdu_draw *out,size_t cap,size_t *total){
 if(!ui||!ui->app||!out||!total||cap==0)return SDM_ARGUMENT;
 if(ui->width<800 || ui->height<360)return SDM_RANGE;
 if(ui->screen>=SDU_SCREEN_COUNT)return SDM_RANGE;
 size_t count=0;int w=ui->width,h=ui->height;
 int sidebar=220,content_w=w-sidebar,body_y=96,footer=h-24;
 DRAW(SDU_RECT,0,0,w,h,NAVY,NULL,0,0);
 DRAW(SDU_RECT,0,0,w,32,PANEL,NULL,0,0);
 DRAW(SDU_TEXT_DRAW,16,7,0,0,MAIN,"Документы",15,1);
 DRAW(SDU_TEXT_DRAW,w-198,8,0,0,MUTE,ui->app->document.changed?"Не сохранено":"Сохранено",11,0);
 DRAW(SDU_LINE,0,32,w,1,EDGE,NULL,0,0);
 int x=18;
 for(int t=0;t<8;t++){
   int tabw=(t==6)?116:(t==3||t==5)?100:82;
   uint32_t color=ui->screen==tab_screens[t]?MAIN:MUTE;
   DRAW(SDU_TEXT_DRAW,x,43,0,0,color,tabs[t],13,ui->screen==tab_screens[t]);
   if(ui->screen==tab_screens[t])DRAW(SDU_RECT,x,67,tabw-10,3,GOLD,NULL,0,0);
   x+=tabw;
 }
 DRAW(SDU_LINE,0,72,w,1,EDGE,NULL,0,0);
 if(ui->screen>=SDU_FILE && ui->screen<=SDU_VIEW){
    int tab=(int)ui->screen-1; x=18;
    for(int a=0;a<8 && ribbon[tab][a];a++){
        int tw=(int)glyphs(ribbon[tab][a])*8+20;
        if(tw<70)tw=70;
        if(x+tw> w-10)break;
        int ready=(tab==0 && a<4) || (tab==3 && a<2) ||
                  (tab==1 && (a==6 || a==7));
        DRAW(SDU_TEXT_DRAW,x,80,0,0,ready?MAIN:MUTE,ribbon[tab][a],12,0);
        x+=tw;
    }
 }else DRAW(SDU_TEXT_DRAW,20,80,0,0,MAIN,screen_names[ui->screen],12,0);
 DRAW(SDU_LINE,0,body_y,w,1,EDGE,NULL,0,0);
 DRAW(SDU_RECT,content_w,body_y,sidebar,footer-body_y,PANEL,NULL,0,0);
 DRAW(SDU_LINE,content_w,body_y,1,footer-body_y,EDGE,NULL,0,0);
 DRAW(SDU_TEXT_DRAW,content_w+14,body_y+13,0,0,MAIN,"Свойства",15,1);
 DRAW(SDU_LINE,content_w+14,body_y+41,sidebar-26,1,EDGE,NULL,0,0);
 DRAW(SDU_TEXT_DRAW,content_w+14,body_y+55,0,0,GOLD,ui->app->has_file?"Открытый файл":"Без имени",13,0);
 DRAW(SDU_TEXT_DRAW,content_w+14,body_y+83,0,0,MUTE,ui->app->document.changed?"Есть изменения":"Без изменений",12,0);
 DRAW(SDU_TEXT_DRAW,content_w+14,body_y+110,0,0,MUTE,ui->app->document.page.paper==SDM_A4?"Формат: A4":"Формат: Letter",12,0);
 if(ui->screen==SDU_REVIEW){
    DRAW(SDU_LINE,content_w+14,body_y+145,sidebar-26,1,EDGE,NULL,0,0);
    DRAW(SDU_TEXT_DRAW,content_w+14,body_y+155,0,0,MAIN,"Комментарии",12,1);
    unsigned comments=0;for(size_t i=0;i<SDM_MAX_COMMENTS;i++)if(ui->app->document.comments[i].active)comments++;
    char info[64];(void)snprintf(info,sizeof(info),"Всего: %u",comments);
    DRAW(SDU_TEXT_DRAW,content_w+14,body_y+182,0,0,MUTE,info,12,0);
 }
 int sheet_w=content_w-176; if(sheet_w>460)sheet_w=460;
 int sheet_x=(content_w-sheet_w)/2;
 int sheet_y=body_y+33;
 int sheet_h=footer-sheet_y-15;
 DRAW(SDU_RECT,sheet_x-1,sheet_y-1,sheet_w+2,sheet_h+2,0x58606bu,NULL,0,0);
 DRAW(SDU_RECT,sheet_x,sheet_y,sheet_w,sheet_h,WARM,NULL,0,0);
 DRAW(SDU_LINE,sheet_x,sheet_y-18,sheet_w,1,EDGE,NULL,0,0);
 for(int i=0;i<=sheet_w;i+=16){
    int tickh=i%64==0?8:4;
    DRAW(SDU_LINE,sheet_x+i,sheet_y-18,1,tickh,MUTE,NULL,0,0);
 }
 const sdm_document *d=&ui->app->document;
 if(d->bytes){
   size_t start=0;int row=0,max_rows=(sheet_h-48)/22;
   while(start<=d->bytes && row<max_rows){
       size_t end=start;
       while(end<d->bytes && d->content[end]!='\n' && end-start<72)end++;
       char line[192];cut_utf8_line(d->content,start,end,line,sizeof(line));
       /* The selected span belongs to the app canvas, not the shell.
        * Glyph width remains approximate until a font metric service exists. */
       if(d->anchor!=d->cursor){
          size_t a=d->anchor<d->cursor?d->anchor:d->cursor;
          size_t b=d->anchor>d->cursor?d->anchor:d->cursor;
          if(a<end && b>start){
            size_t lo=a>start?a:start,hi=b<end?b:end;
            char prefix[192],selected[192];
            cut_utf8_line(d->content,start,lo,prefix,sizeof(prefix));
            cut_utf8_line(d->content,lo,hi,selected,sizeof(selected));
            int px=sheet_x+35+(int)glyphs(prefix)*8;
            int pw=(int)glyphs(selected)*8;
            if(pw>0)DRAW(SDU_RECT,px,sheet_y+27+row*22,pw,20,0xf1d38fu,NULL,0,0);
          }
       }
       DRAW(SDU_TEXT_DRAW,sheet_x+35,sheet_y+30+row*22,0,0,INK,line,13,0);
       row++;
       if(end>=d->bytes)break;
       if(d->content[end]=='\n')start=end+1;
       else{
           size_t next=end;
           while(next>start && (((unsigned char)d->content[next]&0xc0)==0x80))next--;
           start=next>start?next:end;
       }
   }
 }else DRAW(SDU_TEXT_DRAW,sheet_x+40,sheet_y+42,0,0,0x929ba7u,"Начните вводить текст…",14,0);
 DRAW(SDU_RECT,0,footer,w,24,PANEL,NULL,0,0);
 DRAW(SDU_LINE,0,footer,w,1,EDGE,NULL,0,0);
 DRAW(SDU_TEXT_DRAW,16,footer+5,0,0,MUTE,"Режим редактирования",11,0);
 char zoom[32];(void)snprintf(zoom,sizeof(zoom),"%d%%",ui->zoom_percent);
 DRAW(SDU_TEXT_DRAW,w-60,footer+5,0,0,MAIN,zoom,11,0);
 *total=count;return SDM_OK;
}
static int tab_at(int x){
 int cursor=18;
 for(int t=0;t<8;t++){int n=(t==6)?116:(t==3||t==5)?100:82;
 if(x>=cursor && x<cursor+n)return t;
 cursor+=n;
 }return -1;
}
sdu_action sdu_click(sdu_ui *ui,int x,int y){
 if(!ui||x<0||x>=ui->width||y<0||y>=ui->height)return SDU_EVENT_NONE;
 if(y>=35 && y<72){int t=tab_at(x);if(t>=0)ui->screen=tab_screens[t];return SDU_EVENT_NONE;}
 if(y>=73 && y<96){
  if(ui->screen==SDU_FILE){
   if(x>=18 && x<88)return SDU_EVENT_NEW;
   if(x>=88 && x<170)return SDU_EVENT_OPEN;
   if(x>=170 && x<278)return SDU_EVENT_SAVE;
   if(x>=278 && x<394)return SDU_EVENT_SAVE_AS;
  }
  if(ui->screen==SDU_TEXT){
   int left=18;
   for(int a=0;a<8 && ribbon[1][a];a++){
     int tw=(int)glyphs(ribbon[1][a])*8+20;
     if(tw<70)tw=70;
     if(x>=left && x<left+tw){
       if(a==6)return SDU_EVENT_FIND;
       if(a==7)return SDU_EVENT_REPLACE;
       return SDU_EVENT_UNAVAILABLE;
     }
     left+=tw;
   }
  }
  if(ui->screen==SDU_PAGE){
   sdm_page page=ui->app->document.page;
   if(x>=18 && x<88)page.paper=SDM_A4;
   else if(x>=88 && x<170)page.paper=SDM_LETTER;
   else return SDU_EVENT_UNAVAILABLE;
   (void)sdm_set_page(&ui->app->document,page);return SDU_EVENT_NONE;
  }
  return SDU_EVENT_UNAVAILABLE;
 }
 return SDU_EVENT_NONE;
}
sdu_action sdu_shortcut(sdu_ui *ui,unsigned key,int ctrl){
 if(!ui||!ctrl)return SDU_EVENT_NONE;
 if(key>='A'&&key<='Z')key=key-'A'+'a';
 if(key=='n')return SDU_EVENT_NEW;
 if(key=='o')return SDU_EVENT_OPEN;
 if(key=='s')return SDU_EVENT_SAVE;
 if(key=='p')return SDU_EVENT_PRINT;
 if(key=='f')return SDU_EVENT_FIND;
 if(key=='h')return SDU_EVENT_REPLACE;
 if(key=='z')return SDU_EVENT_UNDO;
 if(key=='y')return SDU_EVENT_REDO;
 if(key=='a'){(void)sdm_select(&ui->app->document,0,ui->app->document.bytes);return SDU_EVENT_NONE;}
 return SDU_EVENT_NONE;
}
int sdu_input_text(sdu_ui *ui,const char *text,size_t n){
 if(!ui||!ui->app)return SDM_ARGUMENT;
 return sdm_insert(&ui->app->document,text,n);
}
int sdu_backspace(sdu_ui *ui){
 if(!ui||!ui->app)return SDM_ARGUMENT;
 sdm_document *d=&ui->app->document;
 if(d->cursor!=d->anchor)return sdm_remove(d);
 if(!d->cursor)return SDM_OK;
 size_t last=d->cursor-1;
 while(last && (((unsigned char)d->content[last]&0xc0)==0x80))last--;
 int rc=sdm_select(d,last,d->cursor);
 return rc==SDM_OK?sdm_remove(d):rc;
}
