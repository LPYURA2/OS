#include "styleos/documents_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    sdm_document d;sdm_init(&d);
    assert(d.page.paper==SDM_A4 && d.page.top_twips==1440);
    assert(sdm_set_text(&d,"Привет мир",strlen("Привет мир"))==SDM_OK);
    size_t initial=d.bytes;
    assert(sdm_select(&d,1,2)==SDM_RANGE); /* inside a Russian character */
    assert(sdm_select(&d,0,strlen("Привет"))==SDM_OK);
    uint32_t id=0;
    assert(sdm_add_comment(&d,"Проверить",strlen("Проверить"),&id)==SDM_OK);
    assert(id==1&&d.comments[0].from==0);
    assert(sdm_select(&d,0,0)==SDM_OK);
    assert(sdm_insert(&d,"Заметка: ",strlen("Заметка: "))==SDM_OK);
    assert(d.bytes==initial+strlen("Заметка: "));
    assert(d.comments[0].from==strlen("Заметка: "));
    assert(sdm_resolve_comment(&d,id,1)==SDM_OK&&d.comments[0].resolved);
    assert(sdm_select(&d,d.comments[0].from,d.comments[0].to)==SDM_OK);
    char selected[64];size_t written;
    assert(sdm_selected_text(&d,selected,sizeof(selected),&written)==SDM_OK);
    assert(strcmp(selected,"Привет")==0);
    assert(sdm_insert(&d,"Добрый",strlen("Добрый"))==SDM_OK);
    assert(!d.comments[0].active); /* editing selected anchor invalidates */
    assert(strstr(d.content,"Добрый мир"));
    assert(sdm_set_page(&d,(sdm_page){SDM_LETTER,SDM_LANDSCAPE,720,720,720,720})==SDM_OK);
    assert(sdm_set_page(&d,(sdm_page){SDM_A4,SDM_PORTRAIT,65000,65000,65000,65000})==SDM_RANGE);
    assert(sdm_valid_utf8("\xf0\x9f\x8d\xb5",4));
    assert(!sdm_valid_utf8("\xed\xa0\x80",3));
    assert(!sdm_valid_utf8("a\0b",3));
    char corrupt[]={'X',(char)0xff,'Y',0};
    size_t prior=d.bytes;
    assert(sdm_insert(&d,corrupt,3)==SDM_ENCODING && d.bytes==prior);
    assert(sdm_insert(&d,d.content,1)==SDM_ARGUMENT && d.bytes==prior);
    assert(sdm_select(&d,0,d.bytes)==SDM_OK);
    assert(sdm_set_text(&d,d.content,1)==SDM_ARGUMENT);
    assert(sdm_remove(&d)==SDM_OK && d.bytes==0);
    puts("StyleOS documents model: tests PASS");
    return 0;
}
