#include "styleos/documents_docx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sdm_document doc;
static unsigned char buffer[SDM_DOCX_MAX_OUTPUT];

int main(int argc,char **argv){
    size_t size=0;
    sdm_init(&doc);
    const char *sample="Проверка StyleOS & <Макет>\nВторая строка с текстом.\n";
    if(sdm_set_text(&doc,sample,strlen(sample))!=SDM_OK)return 1;
    sdm_page page=doc.page;page.orientation=SDM_LANDSCAPE;
    if(sdm_set_page(&doc,page)!=SDM_OK)return 2;
    if(sdm_docx_export(&doc,buffer,sizeof(buffer),&size)!=SDM_OK)return 3;
    if(size<300 || memcmp(buffer,"PK\003\004",4)!=0)return 4;
    if(sdm_docx_export(&doc,buffer,32,&size)!=SDM_CAPACITY)return 5;
    if(argc>1){FILE *f=fopen(argv[1],"wb");if(!f)return 6;
        if(sdm_docx_export(&doc,buffer,sizeof(buffer),&size)!=SDM_OK)return 7;
        if(fwrite(buffer,1,size,f)!=size)return 8;
        if(fclose(f))return 9;
    }
    puts("C11 DOCX export: PASS");return 0;
}
