#include "styleos/documents_model.h"
#include <string.h>

/* Strict UTF-8: no overlong sequences, surrogates or out-of-range codepoints. */
int sdm_valid_utf8(const char *s, size_t n) {
    size_t i=0;
    if(!s && n) return 0;
    while(i<n) {
        unsigned char a=(unsigned char)s[i],b=0,c=0,d=0;
        size_t w=0;
        if(a==0) return 0;
        if(a<0x80) {++i;continue;}
        if(a>=0xc2 && a<=0xdf) w=2;
        else if(a>=0xe0 && a<=0xef) w=3;
        else if(a>=0xf0 && a<=0xf4) w=4;
        else return 0;
        if(w>n-i) return 0;
        b=(unsigned char)s[i+1];
        if(b<0x80 || b>0xbf) return 0;
        if(w>2) {
            c=(unsigned char)s[i+2];
            if(c<0x80 || c>0xbf) return 0;
            if(a==0xe0 && b<0xa0) return 0;
            if(a==0xed && b>=0xa0) return 0;
        }
        if(w>3) {
            d=(unsigned char)s[i+3];
            if(d<0x80 || d>0xbf) return 0;
            if(a==0xf0 && b<0x90) return 0;
            if(a==0xf4 && b>=0x90) return 0;
        }
        i+=w;
    }
    return 1;
}

static int boundary(const sdm_document *doc,size_t offset) {
    return offset<=doc->bytes && (offset==doc->bytes ||
        (((unsigned char)doc->content[offset]&0xc0)!=0x80));
}
static void ordered(const sdm_document *doc,size_t *a,size_t *b) {
    if(doc->anchor<doc->cursor){*a=doc->anchor;*b=doc->cursor;}
    else {*a=doc->cursor;*b=doc->anchor;}
}

void sdm_init(sdm_document *doc) {
    if(!doc)return;
    memset(doc,0,sizeof(*doc));
    doc->page.paper=SDM_A4;
    doc->page.orientation=SDM_PORTRAIT;
    doc->page.top_twips=1440;
    doc->page.bottom_twips=1440;
    doc->page.left_twips=1440;
    doc->page.right_twips=1440;
    doc->next_id=1;
}

int sdm_set_text(sdm_document *doc,const char *text,size_t n){
    if(!doc || (!text && n))return SDM_ARGUMENT;
    if(n>=SDM_TEXT_CAPACITY)return SDM_CAPACITY;
    if(n && (uintptr_t)text >= (uintptr_t)doc->content &&
       (uintptr_t)text < (uintptr_t)(doc->content+SDM_TEXT_CAPACITY))return SDM_ARGUMENT;
    if(!sdm_valid_utf8(text,n))return SDM_ENCODING;
    if(n)memcpy(doc->content,text,n);
    doc->content[n]=0;doc->bytes=n;doc->cursor=n;doc->anchor=n;
    memset(doc->comments,0,sizeof(doc->comments));
    doc->changed=1;
    return SDM_OK;
}

int sdm_select(sdm_document *doc,size_t anchor,size_t cursor){
    if(!doc)return SDM_ARGUMENT;
    if(!boundary(doc,anchor)||!boundary(doc,cursor))return SDM_RANGE;
    doc->anchor=anchor;doc->cursor=cursor;
    return SDM_OK;
}

/* Changes to a selected region invalidate comments that overlap an edited
 * anchor, rather than silently attaching them to unrelated prose. */
static void shift_comments(sdm_document *doc,size_t a,size_t b,size_t inserted){
    size_t erased=b-a;
    for(size_t i=0;i<SDM_MAX_COMMENTS;i++) {
        sdm_comment *c=&doc->comments[i];
        if(!c->active)continue;
        if(c->to<=a)continue;
        if(c->from>=b) {
            if(inserted>=erased){size_t delta=inserted-erased;c->from+=delta;c->to+=delta;}
            else {size_t delta=erased-inserted;c->from-=delta;c->to-=delta;}
        } else if(a==b && c->from<a && a<c->to) {
            c->to+=inserted;
        } else {
            c->active=0;
        }
    }
}

int sdm_insert(sdm_document *doc,const char *text,size_t n){
    size_t a,b,newbytes;
    if(!doc || (!text&&n))return SDM_ARGUMENT;
    if(!boundary(doc,doc->cursor)||!boundary(doc,doc->anchor))return SDM_RANGE;
    /* Reject aliasing before UTF-8 checks, which could read mutated input. */
    if(n && ((uintptr_t)text >= (uintptr_t)doc->content &&
       (uintptr_t)text < (uintptr_t)(doc->content+SDM_TEXT_CAPACITY)))return SDM_ARGUMENT;
    if(!sdm_valid_utf8(text,n))return SDM_ENCODING;
    ordered(doc,&a,&b);
    if(n>SDM_TEXT_CAPACITY-1-(doc->bytes-(b-a)))return SDM_CAPACITY;
    newbytes=doc->bytes-(b-a)+n;
    memmove(doc->content+a+n,doc->content+b,doc->bytes-b+1);
    if(n)memcpy(doc->content+a,text,n);
    shift_comments(doc,a,b,n);
    doc->bytes=newbytes;doc->anchor=a+n;doc->cursor=a+n;
    if(a!=b || n)doc->changed=1;
    return SDM_OK;
}

int sdm_remove(sdm_document *doc){return sdm_insert(doc,"",0);}

int sdm_set_page(sdm_document *doc,sdm_page page){
    unsigned w,h;
    if(!doc || page.paper>SDM_LETTER || page.orientation>SDM_LANDSCAPE)return SDM_ARGUMENT;
    w=(page.paper==SDM_A4)?11906u:12240u;
    h=(page.paper==SDM_A4)?16838u:15840u;
    if(page.orientation==SDM_LANDSCAPE){unsigned tmp=w;w=h;h=tmp;}
    if((unsigned)page.left_twips+page.right_twips>=w ||
       (unsigned)page.top_twips+page.bottom_twips>=h)return SDM_RANGE;
    doc->page=page;doc->changed=1;
    return SDM_OK;
}

int sdm_add_comment(sdm_document *doc,const char *text,size_t n,uint32_t *id_out){
    size_t a,b;
    if(!doc || !id_out || (!text && n))return SDM_ARGUMENT;
    if(!boundary(doc,doc->anchor)||!boundary(doc,doc->cursor))return SDM_RANGE;
    ordered(doc,&a,&b);
    if(a==b)return SDM_RANGE;
    if(n==0 || n>=SDM_COMMENT_CAPACITY)return SDM_CAPACITY;
    if(!sdm_valid_utf8(text,n))return SDM_ENCODING;
    for(size_t i=0;i<SDM_MAX_COMMENTS;i++) {
        sdm_comment *c=&doc->comments[i];
        if(c->active)continue;
        if(!doc->next_id) return SDM_CAPACITY;
        memset(c,0,sizeof(*c));
        c->active=1;c->from=a;c->to=b;c->id=doc->next_id++;
        memcpy(c->body,text,n);c->body[n]=0;
        *id_out=c->id;doc->changed=1;
        return SDM_OK;
    }
    return SDM_CAPACITY;
}

int sdm_resolve_comment(sdm_document *doc,uint32_t id,int resolved){
    if(!doc || !id)return SDM_ARGUMENT;
    for(size_t i=0;i<SDM_MAX_COMMENTS;i++){
        sdm_comment *c=&doc->comments[i];
        if(c->active && c->id==id){
            c->resolved=!!resolved;doc->changed=1;return SDM_OK;
        }
    }
    return SDM_NOT_FOUND;
}

int sdm_selected_text(const sdm_document *doc,char *out,size_t capacity,size_t *written){
    size_t a,b;
    if(!doc||!out||!written)return SDM_ARGUMENT;
    if(!boundary(doc,doc->anchor)||!boundary(doc,doc->cursor))return SDM_RANGE;
    ordered(doc,&a,&b);
    if(b-a>=capacity)return SDM_CAPACITY;
    memcpy(out,doc->content+a,b-a);out[b-a]=0;*written=b-a;
    return SDM_OK;
}
