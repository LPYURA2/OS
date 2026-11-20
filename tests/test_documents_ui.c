#include "styleos/documents_ui.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int has_text(const sdu_draw *cmd,size_t n,const char *t){
 for(size_t i=0;i<n;i++)if(cmd[i].type==SDU_TEXT_DRAW && strcmp(cmd[i].text,t)==0)return 1;
 return 0;
}
int main(void){
 sda_documents_app *a=calloc(1,sizeof(*a));
 sdu_draw *canvas=calloc(SDU_COMMANDS_MAX,sizeof(*canvas));
 assert(a&&canvas);assert(sda_init(a)==SDM_OK);
 sdu_ui ui;assert(sdu_init(&ui,a,900,565)==SDM_OK);
 size_t n=0;
 for(int view=0;view<(int)SDU_SCREEN_COUNT;view++){
   assert(sdu_set_screen(&ui,(sdu_screen)view)==SDM_OK);
   assert(sdu_render(&ui,canvas,SDU_COMMANDS_MAX,&n)==SDM_OK);
   assert(n>10 && n<=SDU_COMMANDS_MAX);
   assert(has_text(canvas,n,"Свойства"));
   assert(has_text(canvas,n,"Документы"));
 }
 assert(sdu_set_screen(&ui,SDU_REVIEW)==SDM_OK);
 assert(sdu_render(&ui,canvas,SDU_COMMANDS_MAX,&n)==SDM_OK);
 assert(has_text(canvas,n,"Комментарии") && has_text(canvas,n,"Грамматика"));
 assert(sdu_click(&ui,20,43)==SDU_EVENT_NONE && ui.screen==SDU_FILE);
 assert(sdu_click(&ui,20,82)==SDU_EVENT_NEW);
 assert(sdu_shortcut(&ui,'s',1)==SDU_EVENT_SAVE);
 assert(sdu_shortcut(&ui,'n',1)==SDU_EVENT_NEW);
 assert(sdu_shortcut(&ui,'p',1)==SDU_EVENT_PRINT);
 assert(sdu_input_text(&ui,"Привет 🟠",strlen("Привет 🟠"))==SDM_OK);
 assert(sdu_backspace(&ui)==SDM_OK);
 assert(strcmp(a->document.content,"Привет ")==0);
 assert(sdu_set_screen(&ui,SDU_PAGE)==SDM_OK);
 assert(sdu_click(&ui,100,80)==SDU_EVENT_NONE);
 assert(a->document.page.paper==SDM_LETTER);
 assert(sdu_click(&ui,300,80)==SDU_EVENT_UNAVAILABLE);
 assert(sdu_init(&ui,a,200,100)==SDM_ARGUMENT);
 free(canvas);free(a);puts("UI 14 views, 8 tabs, UTF8 edits PASS");return 0;
}