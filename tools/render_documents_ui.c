/* Host-only preview bridge. The StyleOS window manager must later implement
   these draw commands; this is not an emulator or an installed guest app. */
#include "styleos/documents_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 sda_documents_app *app=calloc(1,sizeof(*app));
 sdu_draw *commands=calloc(SDU_COMMANDS_MAX,sizeof(*commands));
 if(!app||!commands)return 2;
 if(sda_init(app)!=SDM_OK)return 3;
 const char *content="План первой сборки\n\nПроверка StyleOS\n• Рабочий стол и Проводник\n• Создание и сохранение документа\n\nРаздел: Документы";
 if(sdm_set_text(&app->document,content,strlen(content))!=SDM_OK)return 4;
 sdu_ui ui;if(sdu_init(&ui,app,900,565)!=SDM_OK)return 5;
 if(argc>1 && strcmp(argv[1],"review")==0)ui.screen=SDU_REVIEW;
 if(argc>1 && strcmp(argv[1],"page")==0)ui.screen=SDU_PAGE;
 size_t n;if(sdu_render(&ui,commands,SDU_COMMANDS_MAX,&n)!=SDM_OK)return 6;
 for(size_t i=0;i<n;i++){
  sdu_draw *c=&commands[i];
  printf("%d\t%d\t%d\t%d\t%d\t%06x\t%d\t%d\t%s\n",c->type,c->x,c->y,c->w,c->h,c->rgb,c->size,c->emphasis,c->text);
 }
 free(commands);free(app);return 0;
}
