#include "styleos/documents_import.h"
#include "styleos/documents_docx.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t state=0x9cba7361u;
static uint32_t rnd(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(void){
    sdm_document *source=calloc(1,sizeof(*source));
    sdm_document *out=calloc(1,sizeof(*out));
    uint8_t *zip=calloc(1,SDM_DOCX_MAX_OUTPUT);
    uint8_t *scratch=calloc(1,SDM_DOCX_MAX_OUTPUT);
    assert(source&&out&&zip&&scratch);
    sdm_init(source);
    const char *text="Итог — проверка. <&> \"текст\"\nТаблица\t123\n";
    assert(sdm_set_text(source,text,strlen(text))==SDM_OK);
    size_t n=0;
    assert(sdm_docx_export(source,zip,SDM_DOCX_MAX_OUTPUT,&n)==SDM_OK);
    assert(sdm_docx_import_stored(zip,n,out)==SDM_OK);
    assert(strcmp(source->content,out->content)==0);
    for(unsigned k=0;k<650;k++){
        size_t len=(size_t)(rnd()%2048u);
        for(size_t i=0;i<len;i++)scratch[i]=(uint8_t)rnd();
        assert(sdm_docx_import_stored(scratch,len,out)!=SDM_OK);
    }
    for(unsigned k=0;k<550;k++){
        memcpy(scratch,zip,n);
        size_t loc=(size_t)(rnd()%n);
        scratch[loc]^=(uint8_t)(1u<<(rnd()%8));
        int rc=sdm_docx_import_stored(scratch,n,out);
        if(rc==SDM_OK){
            assert(out->bytes<SDM_TEXT_CAPACITY);
            assert(sdm_valid_utf8(out->content,out->bytes));
        }
    }
    free(scratch);free(zip);free(out);free(source);
    puts("import boundary/fuzz PASS (1200 cases)");
    return 0;
}