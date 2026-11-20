#ifndef STYLEOS_DOCUMENTS_APP_GATEWAY_H
#define STYLEOS_DOCUMENTS_APP_GATEWAY_H
#include <stddef.h>
#include <stdint.h>
#include "styleos/documents_model.h"
/* Host offers explicitly user-granted opaque file tokens. The application has
 * no direct disk access, shell pointers or global app singleton. The host may
 * deny any operation; the editor retains unsaved content on refusal. */
typedef uint64_t sda_file_token;
typedef struct {
    void *context;
    int (*read_granted)(void *context,sda_file_token token,uint8_t *dst,size_t capacity,size_t *size_out);
    int (*replace_atomic_granted)(void *context,sda_file_token token,const uint8_t *src,size_t n);
} sda_host_io;
typedef enum {SDA_TXT=0,SDA_DOCX=1} sda_format;
typedef struct {
    sdm_document document;
    sda_file_token selected_token;
    sda_format selected_format;
    int has_file;
} sda_documents_app;
/* Work buffers are explicitly provided by the caller; do not put ~450 KiB
 * on small guest stacks. Scratch and staged_document must be disjoint. */
int sda_init(sda_documents_app *app);
int sda_open(sda_documents_app *app,const sda_host_io *io,
             sda_file_token token,sda_format format,uint8_t *scratch,size_t capacity,
             sdm_document *staged_document);
int sda_save(sda_documents_app *app,const sda_host_io *io,
             uint8_t *scratch,size_t capacity);
int sda_save_as(sda_documents_app *app,const sda_host_io *io,
             sda_file_token token,sda_format format,uint8_t *scratch,size_t capacity);
int sda_new(sda_documents_app *app);
#endif
