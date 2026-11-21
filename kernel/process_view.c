#include <styleos/process_view.h>
static unsigned lower(unsigned c) {return c>=65 && c<=90 ? c+32 : c;}
static int valid_search(const char *s)
{
    if (!s) return 1;
    for (unsigned i=0;i<PI_MAX_SEARCH;++i) if(!s[i]) return 1;
    return 0;
}
static int has(const char *text, unsigned len, const char *search)
{
    if (!search || !*search) return 1;
    for (unsigned p=0;p<len && text[p];++p) {
        unsigned n=0;
        while(n<PI_MAX_SEARCH && search[n] && p+n<len && text[p+n] &&
              lower((unsigned char)text[p+n])==lower((unsigned char)search[n])) ++n;
        if (!search[n]) return 1;
    }
    return 0;
}
static int matches(const struct pi_row *r,const char *search)
{
    if (has(r->name,PR_LABEL_BYTES,search)) return 1;
    char reverse[11], digits[11]; unsigned n=0; uint32_t pid=r->pid;
    do {reverse[n++]=(char)('0'+pid%10);pid/=10;} while(pid && n<10);
    for (unsigned i=0;i<n;++i) digits[i]=reverse[n-i-1];
    digits[n]=0;
    return has(digits,n+1,search);
}
static int cmp64(uint64_t a,uint64_t b) {return a<b?-1:a>b?1:0;}
static int cmpname(const char *a,const char *b)
{
    for(unsigned i=0;i<PR_LABEL_BYTES;++i) {
        const unsigned x=lower((unsigned char)a[i]),y=lower((unsigned char)b[i]);
        if(x!=y)return x<y?-1:1;
        if(!x)return 0;
    }
    return 0;
}
static int compare(const struct pi_row *a,const struct pi_row *b,
                   const struct pi_query *q)
{
    if(a->group!=b->group) return a->group<b->group?-1:1;
    int n=0;
    switch(q->by){
    case PI_BY_NAME: n=cmpname(a->name,b->name);break;
    case PI_BY_CPU_TICKS: n=cmp64(a->cpu_ticks,b->cpu_ticks);break;
    case PI_BY_MEMORY: n=cmp64(a->resident_bytes,b->resident_bytes);break;
    case PI_BY_STATE: n=cmp64(a->state,b->state);break;
    default: n=cmp64(a->pid,b->pid);break;
    }
    if(q->descending)n=-n;
    return n?n:cmp64(a->pid,b->pid);
}
enum pi_status pi_view(const struct pr_table *p,const struct ws_service *w,
                       const struct pi_query *q,struct pi_row *out,
                       size_t cap,struct pi_view_info *info)
{
    if(!p||!w||!q||!info||(cap&&!out)||
       q->by<PI_BY_PID||q->by>PI_BY_STATE||!valid_search(q->search))
        return PI_ARGUMENT;
    struct pi_row work[PR_MAX_PROCESSES];
    struct pi_snapshot_info snapshot={0};
    enum pi_status s=pi_collect(p,w,work,PR_MAX_PROCESSES,&snapshot);
    if(s!=PI_OK)return s;
    size_t total=0;
    for(size_t i=0;i<snapshot.total;++i)if(matches(&work[i],q->search)){
        if(total!=i)work[total]=work[i];
        ++total;
    }
    for(size_t i=1;i<total;++i){
        struct pi_row item=work[i];size_t j=i;
        while(j && compare(&item,&work[j-1],q)<0){
            work[j]=work[j-1];--j;
        }
        work[j]=item;
    }
    size_t n=total<cap?total:cap;
    for(size_t i=0;i<n;++i)out[i]=work[i];
    info->source=snapshot;
    info->matched=total;
    return total>cap?PI_TRUNCATED:PI_OK;
}
