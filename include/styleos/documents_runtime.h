#ifndef STYLEOS_DOCUMENTS_RUNTIME_H
#define STYLEOS_DOCUMENTS_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
#include "styleos/documents_ui.h"
#include "styleos/documents_review.h"
#include "styleos/documents_history.h"
/* Preview app-owned process contract. NO 744 loader ABI claim. A host must grant
 * each file token; no raw paths, device handles, shell global or IPC access. */
#define SDR_ABI_VERSION 1u
#define SDR_SCRATCH_BYTES 458752u
#define SDR_EVENT_TEXT 1u
#define SDR_EVENT_BACKSPACE 2u
#define SDR_EVENT_POINTER 3u
#define SDR_EVENT_SHORTCUT 4u
#define SDR_EVENT_REDRAW 5u
#define SDR_EVENT_OPEN_GRANTED 6u
#define SDR_EVENT_SAVE_AS_GRANTED 7u
#define SDR_EVENT_NEW 8u
#define SDR_EVENT_FIND 9u
#define SDR_EVENT_FIND_NEXT 10u
#define SDR_EVENT_REPLACE_ALL 11u
#define SDR_EVENT_REPLACE_SELECTED 12u
#define SDR_EVENT_UNDO 13u
#define SDR_EVENT_REDO 14u

typedef struct {
 uint32_t type;
 uint32_t key;
 int32_t x,y;
 uint32_t ctrl;
 const char *text;
 size_t bytes;
 sda_file_token token;
 sda_format format;
} sdr_event;
typedef enum {
 SDR_RESPONSE_OK=0, SDR_RESPONSE_OPEN_DIALOG=1, SDR_RESPONSE_SAVE_DIALOG=2,
 SDR_RESPONSE_PRINT_DIALOG=3, SDR_RESPONSE_FIND_DIALOG=4,
 SDR_RESPONSE_REPLACE_DIALOG=5, SDR_RESPONSE_DENIED=-50, SDR_RESPONSE_BAD_EVENT=-51
} sdr_response;
typedef struct {
 uint32_t version;
 void *context;
 /* window manager: only application client area, no chrome manipulation */
 int (*present)(void *context,const sdu_draw *items,size_t count);
 /* only the broker handles file picker; callback never returns a pathname */
 int (*request_file_dialog)(void *context,int is_save);
 sda_host_io files;
} sdr_broker;
typedef struct {
 sda_documents_app app;
 sdu_ui ui;
 const sdr_broker *host;
 uint8_t *scratch;
 size_t scratch_bytes;
 sdm_document *staged;
 sdh_history history;
 char query[257];
 size_t query_bytes;
 int ready;
 int last_error;
} sdr_runtime;
int sdr_init(sdr_runtime *runtime,const sdr_broker *host,int width,int height,
             uint8_t *scratch,size_t scratch_bytes,sdm_document *staged);
/* Caller passes output buffer; app does not draw into the host framebuffer. */
int sdr_render(sdr_runtime *runtime,sdu_draw *output,size_t max,size_t *count);
int sdr_dispatch(sdr_runtime *runtime,const sdr_event *event);
/* Advisory grammar results only; no changes to the document. */
int sdr_grammar(sdr_runtime *runtime,srev_language language,
                srev_grammar_issue *issues,size_t capacity,size_t *total);
#endif
