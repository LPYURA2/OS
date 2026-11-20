#include "styleos/documents_review.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static int utf8_next(const char *p, size_t len, size_t *i, uint32_t *cp) {
    if (*i >= len) return 0;
    const unsigned char *b = (const unsigned char *)p;
    unsigned c = b[*i]; size_t n;
    uint32_t u;
    if (c < 0x80) { n=1; u=c; }
    else if (c >= 0xc2 && c <= 0xdf) { n=2; u=c&0x1f; }
    else if (c >= 0xe0 && c <= 0xef) { n=3; u=c&0x0f; }
    else if (c >= 0xf0 && c <= 0xf4) { n=4; u=c&0x07; }
    else return -1;
    if (*i+n>len) return -1;
    for (size_t j=1;j<n;j++) {
        if ((b[*i+j]&0xc0)!=0x80) return -1;
        u=(u<<6)|(b[*i+j]&0x3f);
    }
    if ((n==2 && u<0x80) || (n==3 && u<0x800) ||
        (n==4 && u<0x10000) || u>0x10ffff ||
        (u>=0xd800 && u<=0xdfff)) return -1;
    *i+=n; *cp=u; return 1;
}

static int valid_utf8(const char *p, size_t len) {
    size_t i=0; uint32_t cp;
    while (i<len) if (utf8_next(p,len,&i,&cp)!=1) return 0;
    return 1;
}

static int is_boundary(const char *p,size_t len,size_t n) {
    return n<=len && (n==0 || n==len ||
        (((unsigned char)p[n]&0xc0)!=0x80));
}

static char *dup_bytes(const char *p,size_t len) {
    char *q = (char*)malloc(len+1);
    if (!q) return NULL;
    memcpy(q,p,len); q[len]='\0'; return q;
}

void srev_destroy(srev_document *doc) {
    if (!doc) return;
    for (size_t i=0;i<doc->count;i++) free(doc->pieces[i].bytes);
    memset(doc,0,sizeof(*doc));
}

srev_status srev_init(srev_document *doc,const char *utf8) {
    if (!doc || !utf8) return SREV_INVALID;
    size_t n=strlen(utf8);
    if (n>SREV_MAX_BYTES) return SREV_LIMIT;
    if (!valid_utf8(utf8,n)) return SREV_UTF8;
    memset(doc,0,sizeof(*doc)); doc->next_id=1;
    if (!n) return SREV_OK;
    doc->pieces[0]=(srev_piece){dup_bytes(utf8,n),n,0,SREV_NORMAL};
    if (!doc->pieces[0].bytes) return SREV_NO_MEMORY;
    doc->count=1;return SREV_OK;
}

size_t srev_visible_size(const srev_document *doc) {
    size_t n=0;
    if (doc) for(size_t i=0;i<doc->count;i++)
        if(doc->pieces[i].mark!=SREV_DELETED)n+=doc->pieces[i].len;
    return n;
}
size_t srev_original_size(const srev_document *doc) {
    size_t n=0;
    if (doc) for(size_t i=0;i<doc->count;i++)
        if(doc->pieces[i].mark!=SREV_INSERTED)n+=doc->pieces[i].len;
    return n;
}

static size_t all_size(const srev_document *doc) {
    size_t n=0;
    for(size_t i=0;i<doc->count;i++) n+=doc->pieces[i].len;
    return n;
}

static srev_status split_visible(srev_document *doc,size_t at,size_t *idx) {
    size_t pos=0;
    for(size_t i=0;i<doc->count;i++) {
        srev_piece *p=&doc->pieces[i];
        if(p->mark==SREV_DELETED)continue;
        if(at==pos){*idx=i;return SREV_OK;}
        if(at<pos+p->len){
            size_t left=at-pos,right=p->len-left;
            if(!is_boundary(p->bytes,p->len,left))return SREV_RANGE;
            if(doc->count==SREV_MAX_PIECES)return SREV_LIMIT;
            char *a=dup_bytes(p->bytes,left),*b=dup_bytes(p->bytes+left,right);
            if(!a || !b){free(a);free(b);return SREV_NO_MEMORY;}
            srev_piece tail={b,right,p->change_id,p->mark};
            free(p->bytes);p->bytes=a;p->len=left;
            memmove(&doc->pieces[i+2],&doc->pieces[i+1],
                    (doc->count-i-1)*sizeof(srev_piece));
            doc->pieces[i+1]=tail;doc->count++;
            *idx=i+1;return SREV_OK;
        }
        pos+=p->len;
    }
    if(at==pos){*idx=doc->count;return SREV_OK;}
    return SREV_RANGE;
}

srev_status srev_insert(srev_document *doc,size_t at,
                        const char *utf8,uint32_t *change_id) {
    if(!doc || !utf8)return SREV_INVALID;
    size_t len=strlen(utf8);
    if(!valid_utf8(utf8,len))return SREV_UTF8;
    if(!len)return SREV_INVALID;
    if(len>SREV_MAX_BYTES || all_size(doc)+len>SREV_MAX_BYTES*2u ||
       srev_visible_size(doc)+len>SREV_MAX_BYTES)return SREV_LIMIT;
    if(doc->next_id==UINT32_MAX)return SREV_LIMIT;
    /* Reserve insertion slot before splitting a piece. */
    if(doc->count+2>SREV_MAX_PIECES)return SREV_LIMIT;
    char *copy=dup_bytes(utf8,len);
    if(!copy)return SREV_NO_MEMORY;
    size_t idx=0;
    srev_status rc=split_visible(doc,at,&idx);
    if(rc!=SREV_OK){free(copy);return rc;}
    memmove(&doc->pieces[idx+1],&doc->pieces[idx],
            (doc->count-idx)*sizeof(srev_piece));
    uint32_t id=doc->next_id++;
    doc->pieces[idx]=(srev_piece){copy,len,id,SREV_INSERTED};doc->count++;
    if(change_id)*change_id=id;
    return SREV_OK;
}

srev_status srev_delete(srev_document *doc,size_t at,size_t n,
                        uint32_t *change_id) {
    if(!doc || !n)return SREV_INVALID;
    size_t visible=srev_visible_size(doc);
    if(at>visible || n>visible-at)return SREV_RANGE;
    if(doc->next_id==UINT32_MAX || doc->count+2>SREV_MAX_PIECES)
        return SREV_LIMIT;
    size_t dummy;
    srev_status rc=split_visible(doc,at+n,&dummy);
    if(rc!=SREV_OK)return rc;
    rc=split_visible(doc,at,&dummy);
    if(rc!=SREV_OK)return rc;
    uint32_t id=doc->next_id++;
    size_t pos=0;
    for(size_t i=0;i<doc->count;) {
        srev_piece *p=&doc->pieces[i];
        if(p->mark==SREV_DELETED){i++;continue;}
        size_t end=pos+p->len;
        if(pos>=at && end<=at+n) {
            if(p->mark==SREV_INSERTED) {
                free(p->bytes);
                memmove(&doc->pieces[i],&doc->pieces[i+1],
                        (doc->count-i-1)*sizeof(srev_piece));
                doc->count--;pos=end;continue;
            }
            p->mark=SREV_DELETED;p->change_id=id;
        }
        pos=end;i++;
    }
    if(change_id)*change_id=id;
    return SREV_OK;
}

srev_status srev_resolve(srev_document *doc,uint32_t id,int accept) {
    if(!doc || !id)return SREV_INVALID;
    int found=0;
    for(size_t i=0;i<doc->count;) {
        srev_piece *p=&doc->pieces[i];
        if(p->change_id!=id){i++;continue;}
        found=1;
        int remove=(p->mark==SREV_INSERTED && !accept) ||
                   (p->mark==SREV_DELETED && accept);
        if(remove) {
            free(p->bytes);
            memmove(&doc->pieces[i],&doc->pieces[i+1],
                    (doc->count-i-1)*sizeof(srev_piece));
            doc->count--;continue;
        }
        p->mark=SREV_NORMAL;p->change_id=0;i++;
    }
    return found?SREV_OK:SREV_NOT_FOUND;
}

srev_status srev_resolve_all(srev_document *doc,int accept) {
    if(!doc || (accept!=0 && accept!=1))return SREV_INVALID;
    /* Removing pieces shifts positions; pick the next pending id each time. */
    while(1) {
        uint32_t id=0;
        for(size_t i=0;i<doc->count;i++) {
            if(doc->pieces[i].mark!=SREV_NORMAL) {
                id=doc->pieces[i].change_id;break;
            }
        }
        if(!id)return SREV_OK;
        srev_status rc=srev_resolve(doc,id,accept);
        if(rc!=SREV_OK)return rc;
    }
}

srev_status srev_collect_changes(const srev_document *doc,
                                 srev_change_span *out,size_t capacity,
                                 size_t *count) {
    if(!doc || !count || (capacity && !out))return SREV_INVALID;
    size_t visible=0,original=0,n=0;
    for(size_t i=0;i<doc->count;i++) {
        const srev_piece *p=&doc->pieces[i];
        if(p->mark!=SREV_NORMAL) {
            if(n<capacity) {
                out[n]=(srev_change_span){p->change_id,p->mark,
                                          visible,original,p->len};
            }
            n++;
        }
        if(p->mark!=SREV_DELETED)visible+=p->len;
        if(p->mark!=SREV_INSERTED)original+=p->len;
    }
    *count=n;
    return n>capacity?SREV_LIMIT:SREV_OK;
}

srev_status srev_render(const srev_document *doc,int original,
                        char *out,size_t cap) {
    if(!doc || !out)return SREV_INVALID;
    size_t n=original?srev_original_size(doc):srev_visible_size(doc);
    if(cap<=n)return SREV_LIMIT;
    size_t at=0;
    for(size_t i=0;i<doc->count;i++) {
        const srev_piece *p=&doc->pieces[i];
        if((original && p->mark==SREV_INSERTED) ||
           (!original && p->mark==SREV_DELETED))continue;
        memcpy(out+at,p->bytes,p->len);at+=p->len;
    }
    out[at]=0;return SREV_OK;
}

static void u32w(unsigned char *p,uint32_t n) {
    for(int i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));
}
static uint32_t u32r(const unsigned char *p) {
    uint32_t n=0;for(int i=0;i<4;i++)n|=(uint32_t)p[i]<<(8*i);return n;
}

srev_status srev_pack(const srev_document *doc,unsigned char *out,
                      size_t cap,size_t *written) {
    if(!doc || !written)return SREV_INVALID;
    size_t n=12;
    for(size_t i=0;i<doc->count;i++)n+=9+doc->pieces[i].len;
    *written=n;
    if(!out || cap<n)return SREV_LIMIT;
    memcpy(out,"STRV",4);u32w(out+4,1);u32w(out+8,(uint32_t)doc->count);
    size_t pos=12;
    for(size_t i=0;i<doc->count;i++) {
        const srev_piece *p=&doc->pieces[i];
        out[pos++]=(unsigned char)p->mark;
        u32w(out+pos,p->change_id);pos+=4;
        u32w(out+pos,(uint32_t)p->len);pos+=4;
        memcpy(out+pos,p->bytes,p->len);pos+=p->len;
    }
    return SREV_OK;
}

srev_status srev_unpack(srev_document *doc,const unsigned char *in,
                        size_t len) {
    if(!doc || !in || len<12 || memcmp(in,"STRV",4)!=0 ||
       u32r(in+4)!=1)return SREV_INVALID;
    uint32_t count=u32r(in+8);
    if(count>SREV_MAX_PIECES)return SREV_LIMIT;
    srev_document tmp={0};tmp.next_id=1;
    size_t pos=12,total=0;
    for(uint32_t i=0;i<count;i++) {
        if(len-pos<9){srev_destroy(&tmp);return SREV_INVALID;}
        unsigned mark=in[pos++];uint32_t id=u32r(in+pos);pos+=4;
        uint32_t n=u32r(in+pos);pos+=4;
        if(mark>2 || (!id && mark!=SREV_NORMAL) ||
           (id && mark==SREV_NORMAL) || !n || n>len-pos ||
           total+n>SREV_MAX_BYTES*2u ||
           !valid_utf8((const char*)in+pos,n) ||
           memchr(in+pos,0,n)!=NULL) {
            srev_destroy(&tmp);return SREV_INVALID;
        }
        char *copy=dup_bytes((const char*)in+pos,n);
        if(!copy){srev_destroy(&tmp);return SREV_NO_MEMORY;}
        tmp.pieces[i]=(srev_piece){copy,n,id,(srev_mark)mark};
        tmp.count++;total+=n;pos+=n;
        if(id>=tmp.next_id){
            if(id==UINT32_MAX){srev_destroy(&tmp);return SREV_LIMIT;}
            tmp.next_id=id+1;
        }
    }
    if(pos!=len || srev_visible_size(&tmp)>SREV_MAX_BYTES ||
       srev_original_size(&tmp)>SREV_MAX_BYTES){
        srev_destroy(&tmp);return SREV_INVALID;
    }
    /* Caller must destroy an existing document before unpacking. */
    *doc=tmp;return SREV_OK;
}

static uint32_t fold(uint32_t cp) {
    if(cp>='A' && cp<='Z')return cp+32;
    if(cp>=0x410 && cp<=0x42f)return cp+32;
    if(cp==0x401)return 0x451;
    return cp;
}
static int letter(uint32_t cp,srev_language lang) {
    int en=(cp>='A' && cp<='Z') || (cp>='a' && cp<='z');
    int ru=(cp>=0x410 && cp<=0x44f) || cp==0x401 || cp==0x451;
    return ((lang&SREV_EN) && en) || ((lang&SREV_RU) && ru);
}
static int words_equal(const char *a,size_t na,const char *b,size_t nb) {
    size_t ia=0,ib=0;uint32_t ca,cb;
    while(ia<na && ib<nb) {
        if(utf8_next(a,na,&ia,&ca)!=1 || utf8_next(b,nb,&ib,&cb)!=1)
            return 0;
        if(fold(ca)!=fold(cb))return 0;
    }
    return ia==na && ib==nb;
}
static void add_issue(srev_grammar_issue *out,size_t cap,size_t *count,
                      srev_grammar_rule rule,size_t begin,size_t end) {
    if(*count<cap)out[*count]=(srev_grammar_issue){rule,begin,end};
    (*count)++;
}

srev_status srev_grammar_scan(const char *s,srev_language lang,
                              srev_grammar_issue *out,size_t cap,
                              size_t *count) {
    if(!s || !count || (cap && !out) ||
       (lang!=SREV_RU && lang!=SREV_EN && lang!=SREV_BOTH))
        return SREV_INVALID;
    size_t len=strlen(s),i=0;
    if(len>SREV_MAX_BYTES)return SREV_LIMIT;
    if(!valid_utf8(s,len))return SREV_UTF8;
    *count=0;
    size_t word_start=0,word_len=0,prev_start=0,prev_len=0;
    int separated_only_space=0;
    while(i<len) {
        size_t at=i;uint32_t cp;
        (void)utf8_next(s,len,&i,&cp);
        if(letter(cp,lang)) {
            word_start=at;
            for(;;) {
                if(i>=len)break;
                size_t test=i;uint32_t next;
                (void)utf8_next(s,len,&test,&next);
                if(!letter(next,lang))break;
                i=test;
            }
            word_len=i-word_start;
            if(prev_len && separated_only_space &&
               words_equal(s+word_start,word_len,s+prev_start,prev_len))
                add_issue(out,cap,count,SREV_GRAMMAR_DUPLICATE_WORD,
                          word_start,i);
            prev_start=word_start;prev_len=word_len;separated_only_space=0;
            continue;
        }
        if(cp==' ') {
            size_t start=at;
            while(i<len && s[i]==' ')i++;
            if(i-start>1)
                add_issue(out,cap,count,SREV_GRAMMAR_DOUBLE_SPACE,start,i);
            if(prev_len)separated_only_space=1;
            if(i<len && (s[i]==',' || s[i]=='.' || s[i]==':' ||
                         s[i]==';' || s[i]=='!' || s[i]=='?'))
                add_issue(out,cap,count,SREV_GRAMMAR_SPACE_BEFORE_PUNCTUATION,
                          start,i);
            continue;
        }
        if(cp==',' || cp==';') {
            if(i<len && s[i]!=' ' && s[i]!='\n' && s[i]!='\r' &&
               s[i]!='\t' && s[i]!=',' && s[i]!=';' &&
               s[i]!='\"' && s[i]!=')') {
                /* Numeric grouping is not judged as grammar. */
                if(!(at>0 && s[at-1]>='0' && s[at-1]<='9' &&
                     s[i]>='0' && s[i]<='9'))
                    add_issue(out,cap,count,SREV_GRAMMAR_NO_SPACE_AFTER_COMMA,
                              at,i);
            }
        }
        if(cp=='\n' || cp=='\r' || cp=='.' || cp==',' || cp==';' ||
           cp==':' || cp=='!' || cp=='?' || cp=='(' || cp==')'){
            prev_len=0;separated_only_space=0;
        } else if(cp!='\t') {
            prev_len=0;separated_only_space=0;
        }
    }
    return (*count>cap)?SREV_LIMIT:SREV_OK;
}
