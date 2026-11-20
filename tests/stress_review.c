#include "styleos/documents_review.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned rng=0x1259beefu;
static unsigned next_random(void) { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
int main(void) {
    for (int run=0;run<40;run++) {
        srev_document d={0};char expected[1024]="начало";
        assert(srev_init(&d,expected)==SREV_OK);
        /* UTF-8 initial text; subsequent edits occur on ASCII-only prefix. */
        for(int j=0;j<48;j++) {
            size_t ascii=0;
            while(expected[ascii] && (unsigned char)expected[ascii]<128)ascii++;
            unsigned op=next_random()%2;
            size_t off=ascii?(next_random()%(ascii+1)):0;
            srev_status rc;
            if(op==0 || !ascii) {
                rc=srev_insert(&d,off,"x",NULL);
                if(rc==SREV_LIMIT)break;
                assert(rc==SREV_OK);
                memmove(expected+off+1,expected+off,strlen(expected+off)+1);
                expected[off]='x';
            } else {
                rc=srev_delete(&d,off<ascii?off:ascii-1,1,NULL);
                if(rc==SREV_LIMIT)break;
                assert(rc==SREV_OK);
                if(off==ascii)off--;
                memmove(expected+off,expected+off+1,strlen(expected+off+1)+1);
            }
            char actual[1024];
            assert(srev_render(&d,0,actual,sizeof actual)==SREV_OK);
            if(strcmp(actual,expected)) {
                fprintf(stderr,"run %d step %d got '%s' expected '%s'\n",run,j,actual,expected);
                abort();
            }
            unsigned char binary[32768];size_t n=0;
            assert(srev_pack(&d,binary,sizeof binary,&n)==SREV_OK);
            srev_document opened={0};
            assert(srev_unpack(&opened,binary,n)==SREV_OK);
            assert(srev_render(&opened,0,actual,sizeof actual)==SREV_OK);
            assert(strcmp(actual,expected)==0);
            srev_destroy(&opened);
        }
        srev_destroy(&d);
    }
    puts("40 deterministic edit/reload scenarios PASS");
    return 0;
}
