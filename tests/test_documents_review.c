#include "styleos/documents_review.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void is_text(const srev_document *d,const char *s,int original) {
    char b[SREV_MAX_BYTES+1];
    assert(srev_render(d,original,b,sizeof b)==SREV_OK);
    assert(strcmp(b,s)==0);
}
static int has(const srev_grammar_issue *v,size_t n,srev_grammar_rule rule) {
    for(size_t i=0;i<n;i++)if(v[i].rule==rule)return 1;
    return 0;
}
static void test_grammar(void) {
    srev_grammar_issue issues[SREV_MAX_ISSUES];size_t n;
    assert(srev_grammar_scan("Это это  важно ,да",SREV_RU,issues,128,&n)==SREV_OK);
    assert(n==4);
    assert(has(issues,n,SREV_GRAMMAR_DUPLICATE_WORD));
    assert(has(issues,n,SREV_GRAMMAR_DOUBLE_SPACE));
    assert(has(issues,n,SREV_GRAMMAR_SPACE_BEFORE_PUNCTUATION));
    assert(has(issues,n,SREV_GRAMMAR_NO_SPACE_AFTER_COMMA));
    assert(srev_grammar_scan("The the  plan,works",SREV_EN,issues,128,&n)==SREV_OK);
    assert(n==3);
    assert(srev_grammar_scan("Цена 1,500, пункт 2",SREV_RU,issues,128,&n)==SREV_OK);
    assert(n==0);
    assert(srev_grammar_scan("https://styleos.org",SREV_BOTH,issues,128,&n)==SREV_OK);
    assert(n==0);
    assert(srev_grammar_scan("a\xff",SREV_BOTH,issues,128,&n)==SREV_UTF8);
    assert(srev_grammar_scan("word",SREV_BOTH,issues,0,&n)==SREV_OK && n==0);
    assert(srev_grammar_scan("слово слово",SREV_RU,issues,0,&n)==SREV_LIMIT && n==1);
    puts("grammar 8 checks PASS");
}
static void test_review(void) {
    srev_document d={0};uint32_t ins,del;
    assert(srev_init(&d,"Привет мир")==SREV_OK);
    assert(srev_insert(&d,strlen("Привет")," новый",&ins)==SREV_OK);
    is_text(&d,"Привет новый мир",0);
    is_text(&d,"Привет мир",1);
    assert(srev_delete(&d,strlen("Привет новый "),strlen("мир"),&del)==SREV_OK);
    is_text(&d,"Привет новый ",0);
    is_text(&d,"Привет мир",1);
    assert(srev_resolve(&d,del,0)==SREV_OK);
    is_text(&d,"Привет новый мир",0);
    assert(srev_resolve(&d,ins,1)==SREV_OK);
    is_text(&d,"Привет новый мир",1);
    assert(srev_resolve(&d,ins,0)==SREV_NOT_FOUND);
    assert(srev_insert(&d,1,"X",NULL)==SREV_RANGE); /* split inside UTF-8 */
    assert(srev_insert(&d,0,"Начало: ",&ins)==SREV_OK);
    is_text(&d,"Начало: Привет новый мир",0);
    assert(srev_resolve(&d,ins,0)==SREV_OK);
    is_text(&d,"Привет новый мир",0);
    srev_destroy(&d);
    puts("tracked changes insert/delete/accept/reject UTF-8 PASS");
}
static void test_persistence(void) {
    srev_document d={0},opened={0};uint32_t id1,id2;
    unsigned char buffer[32768];size_t n;
    assert(srev_init(&d,"Первая строка")==SREV_OK);
    assert(srev_insert(&d,strlen("Первая")," новая",&id1)==SREV_OK);
    assert(srev_delete(&d,0,strlen("Первая"),&id2)==SREV_OK);
    assert(srev_pack(&d,buffer,sizeof buffer,&n)==SREV_OK);
    assert(srev_unpack(&opened,buffer,n)==SREV_OK);
    is_text(&opened," новая строка",0);
    is_text(&opened,"Первая строка",1);
    assert(srev_resolve(&opened,id2,0)==SREV_OK);
    assert(srev_resolve(&opened,id1,0)==SREV_OK);
    is_text(&opened,"Первая строка",0);
    srev_destroy(&opened);
    buffer[0]='X';
    assert(srev_unpack(&opened,buffer,n)==SREV_INVALID);
    buffer[0]='S';
    assert(srev_unpack(&opened,buffer,n-1)==SREV_INVALID);
    srev_destroy(&d);
    puts("review private serialization roundtrip + corruption PASS");
}
static void test_edges(void) {
    srev_document d={0};uint32_t id;
    assert(srev_init(&d,"")==SREV_OK);
    assert(srev_insert(&d,0,"abc",&id)==SREV_OK);
    is_text(&d,"abc",0);is_text(&d,"",1);
    assert(srev_delete(&d,0,3,NULL)==SREV_OK);
    is_text(&d,"",0);is_text(&d,"",1);
    assert(srev_resolve(&d,id,0)==SREV_NOT_FOUND);
    assert(srev_delete(&d,0,1,NULL)==SREV_RANGE);
    srev_destroy(&d);
    assert(srev_init(&d,"Текст")==SREV_OK);
    assert(srev_delete(&d,0,strlen("Текст"),&id)==SREV_OK);
    is_text(&d,"",0);is_text(&d,"Текст",1);
    assert(srev_resolve(&d,id,1)==SREV_OK);
    is_text(&d,"",0);is_text(&d,"",1);
    srev_destroy(&d);
    puts("empty, pending insert deletion, accepted deletion PASS");
}

static void test_review_ui_spans(void) {
    srev_document d={0};uint32_t insert_id=0,delete_id=0;
    srev_change_span changes[8];size_t n=0;
    assert(srev_init(&d,"Привет, мир")==SREV_OK);
    assert(srev_insert(&d,strlen("Привет, "),"новый ",&insert_id)==SREV_OK);
    assert(srev_delete(&d,strlen("Привет, новый "),strlen("мир"),&delete_id)==SREV_OK);
    assert(srev_collect_changes(&d,changes,8,&n)==SREV_OK);
    assert(n==2);
    assert(changes[0].change_id==insert_id && changes[0].mark==SREV_INSERTED);
    assert(changes[0].visible_byte_offset==strlen("Привет, "));
    assert(changes[0].original_byte_offset==strlen("Привет, "));
    assert(changes[0].bytes==strlen("новый "));
    assert(changes[1].change_id==delete_id && changes[1].mark==SREV_DELETED);
    assert(changes[1].visible_byte_offset==strlen("Привет, новый "));
    assert(changes[1].original_byte_offset==strlen("Привет, "));
    assert(changes[1].bytes==strlen("мир"));
    assert(srev_collect_changes(&d,changes,1,&n)==SREV_LIMIT && n==2);
    assert(srev_collect_changes(&d,NULL,0,&n)==SREV_LIMIT && n==2);
    assert(srev_resolve_all(&d,0)==SREV_OK);
    is_text(&d,"Привет, мир",0);
    assert(srev_collect_changes(&d,changes,8,&n)==SREV_OK && n==0);
    srev_destroy(&d);

    assert(srev_init(&d,"Привет, мир")==SREV_OK);
    assert(srev_insert(&d,strlen("Привет, "),"новый ",&insert_id)==SREV_OK);
    assert(srev_delete(&d,strlen("Привет, новый "),strlen("мир"),&delete_id)==SREV_OK);
    assert(srev_resolve_all(&d,1)==SREV_OK);
    is_text(&d,"Привет, новый ",0);
    is_text(&d,"Привет, новый ",1);
    assert(srev_resolve_all(&d,1)==SREV_OK);
    assert(srev_resolve_all(&d,4)==SREV_INVALID);
    srev_destroy(&d);
    puts("review UI spans, accept all / reject all PASS");
}
static void test_reject_embedded_null(void) {
    srev_document a={0},b={0};unsigned char packed[32768];size_t n=0;
    assert(srev_init(&a,"abc")==SREV_OK);
    assert(srev_pack(&a,packed,sizeof packed,&n)==SREV_OK);
    assert(n==24);
    packed[21]=0;
    assert(srev_unpack(&b,packed,n)==SREV_INVALID);
    srev_destroy(&a);srev_destroy(&b);
    puts("reject embedded NUL in serialized text PASS");
}
int main(void) {
    test_grammar();test_review();test_persistence();test_edges();
    test_review_ui_spans();test_reject_embedded_null();
    puts("StyleOS Documents review engine: ALL PASS");return 0;
}
