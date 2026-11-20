#ifndef STYLEOS_DOCUMENTS_UI_H
#define STYLEOS_DOCUMENTS_UI_H
#include <stddef.h>
#include <stdint.h>
#include "styleos/documents_app_gateway.h"
/* Documents paints its own client area. StyleOS window manager owns the outer
 * chrome, frame, focus and input events; no desktop symbols are imported. */
typedef enum {
 SDU_START,SDU_FILE,SDU_TEXT,SDU_INSERT,SDU_PAGE,SDU_REFERENCES,
 SDU_REVIEW,SDU_AUTOMATION,SDU_VIEW,SDU_PARAGRAPH,SDU_IMAGE,
 SDU_OPEN,SDU_SAVE_AS,SDU_PRINT,SDU_SCREEN_COUNT
} sdu_screen;
typedef enum {SDU_RECT,SDU_TEXT_DRAW,SDU_LINE} sdu_shape;
typedef struct {
 sdu_shape type;
 int x,y,w,h;
 uint32_t rgb;
 int size,emphasis;
 char text[192];
} sdu_draw;
#define SDU_COMMANDS_MAX 160u
typedef struct {
 sda_documents_app *app;
 sdu_screen screen;
 int zoom_percent;
 int width,height;
} sdu_ui;
typedef enum {
 SDU_EVENT_NONE=0, SDU_EVENT_NEW, SDU_EVENT_OPEN, SDU_EVENT_SAVE,
 SDU_EVENT_SAVE_AS, SDU_EVENT_PRINT, SDU_EVENT_FIND,
 SDU_EVENT_REPLACE, SDU_EVENT_UNDO, SDU_EVENT_REDO, SDU_EVENT_UNAVAILABLE
} sdu_action;
int sdu_init(sdu_ui *ui,sda_documents_app *app,int width,int height);
int sdu_set_screen(sdu_ui *ui,sdu_screen screen);
int sdu_render(const sdu_ui *ui,sdu_draw *out,size_t capacity,size_t *count);
/* Actions for host broker are returned, not performed from inside the renderer. */
sdu_action sdu_click(sdu_ui *ui,int x,int y);
sdu_action sdu_shortcut(sdu_ui *ui,unsigned key,int ctrl);
int sdu_input_text(sdu_ui *ui,const char *utf8,size_t bytes);
int sdu_backspace(sdu_ui *ui);
#endif
