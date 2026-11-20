#include "styleos/documents_app_gateway.h"
#include "styleos/documents_docx.h"
#include "styleos/documents_import.h"
#include <string.h>
#define SDA_IO_DENIED (-30)
static int valid_format(sda_format fmt){return fmt==SDA_TXT||fmt==SDA_DOCX;}
int sda_init(sda_documents_app *app){
    if(!app)return SDM_ARGUMENT;
    memset(app,0,sizeof(*app));sdm_init(&app->document);
    return SDM_OK;
}
int sda_new(sda_documents_app *app){return sda_init(app);}
int sda_open(sda_documents_app *app,const sda_host_io *io,
             sda_file_token token,sda_format format,uint8_t *scratch,size_t capacity,
             sdm_document *staged_document){
    if(!app||!io||!io->read_granted||!scratch||!staged_document||
       !token||!valid_format(format)||staged_document==&app->document)return SDM_ARGUMENT;
    size_t required=format==SDA_DOCX?SDM_DOCX_MAX_OUTPUT:SDM_TEXT_CAPACITY;
    if(capacity<required)return SDM_CAPACITY;
    size_t n=0;
    int rc=io->read_granted(io->context,token,scratch,capacity,&n);
    if(rc!=0)return SDA_IO_DENIED;
    if(n>capacity)return SDM_CAPACITY;
    if(format==SDA_DOCX)rc=sdm_docx_import_stored(scratch,n,staged_document);
    else {sdm_init(staged_document);rc=sdm_set_text(staged_document,(char*)scratch,n);}
    if(rc!=SDM_OK)return rc;
    staged_document->changed=0;
    app->document=*staged_document;
    app->selected_token=token;app->selected_format=format;app->has_file=1;
    return SDM_OK;
}
static int save_to(sda_documents_app *app,const sda_host_io *io,
                   sda_file_token token,sda_format fmt,uint8_t *scratch,size_t cap){
    if(!app||!io||!io->replace_atomic_granted||!token||!valid_format(fmt)||!scratch)
        return SDM_ARGUMENT;
    size_t n=app->document.bytes;
    const uint8_t *source=(const uint8_t*)app->document.content;
    if(fmt==SDA_DOCX){
        int rc=sdm_docx_export(&app->document,scratch,cap,&n);
        if(rc!=SDM_OK)return rc;
        source=scratch;
    }else if(!sdm_valid_utf8(app->document.content,n))return SDM_ENCODING;
    if(io->replace_atomic_granted(io->context,token,source,n)!=0)return SDA_IO_DENIED;
    return SDM_OK;
}
int sda_save(sda_documents_app *app,const sda_host_io *io,
             uint8_t *scratch,size_t cap){
    if(!app)return SDM_ARGUMENT;
    if(!app->has_file)return SDM_NOT_FOUND;
    int rc=save_to(app,io,app->selected_token,app->selected_format,scratch,cap);
    if(rc==SDM_OK)app->document.changed=0;
    return rc;
}
int sda_save_as(sda_documents_app *app,const sda_host_io *io,
             sda_file_token token,sda_format format,uint8_t *scratch,size_t cap){
    int rc=save_to(app,io,token,format,scratch,cap);
    if(rc!=SDM_OK)return rc;
    app->selected_token=token;app->selected_format=format;app->has_file=1;
    app->document.changed=0;
    return SDM_OK;
}
