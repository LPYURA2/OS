#include <styleos/cache_pagewalk_801.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t tab[4][512];
static bool rd(void *unused,uint64_t addr,uint16_t slot,uint64_t *v){
    (void)unused;
    if(addr<0x1000 || addr>0x4000 || (addr&4095u) || slot>=512)return false;
    *v=tab[(addr>>12)-1][slot];return true;
}
int main(void){
    const uint64_t pat=UINT64_C(0x0007040600070406);
    struct cache801_translation t;
    memset(tab,0,sizeof(tab));
    tab[0][0]=0x2003;tab[1][0]=0x3003;tab[2][0]=0x4003;
    tab[3][1]=0x80000000|3u|8u|16u|128u;
    assert(cache801_walk(0x1000,0x1000,pat,40,rd,NULL,&t));
    assert(t.present && t.physical==0x80000000 && t.page_size==4096 && t.pat_index==7);
    assert(t.pat_type==0);
    assert(cache801_walk(0x1000,0x2000,pat,40,rd,NULL,&t) && !t.present);
    tab[2][0]=0x80000000|3u|128u|4096u;
    assert(cache801_walk(0x1000,0x1234,pat,40,rd,NULL,&t));
    assert(t.present && t.page_size==2097152 && t.physical==0x80001234 && t.pat_index==4);
    tab[1][0]=0x80000000|3u|128u;
    assert(cache801_walk(0x1000,0x1234,pat,40,rd,NULL,&t));
    assert(t.present && t.page_size==1073741824u && t.physical==0x80001234);
    tab[0][0]|=128u;
    assert(!cache801_walk(0x1000,0x1234,pat,40,rd,NULL,&t));
    assert(!cache801_walk(0x1000,UINT64_C(0x0000800000000000),pat,40,rd,NULL,&t));
    assert(!cache801_walk(0x1000,0x1234,pat,35,rd,NULL,&t));
    assert(!cache801_walk(0x1000,0x1234,pat,40,NULL,NULL,&t));
    puts("STYLEOS 801 pagewalk host PASS");return 0;
}
