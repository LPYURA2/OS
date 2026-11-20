/* StyleOS Documents standalone text editing operations.
 * All offsets are UTF-8 byte boundaries; replace-all is transactional. */
#include "styleos/documents_edit.h"
#include <string.h>
#include <stdint.h>

#define NEEDLE_CAP 256u
#define REPLACEMENT_CAP 1024u

static int intersects(const void *p,size_t n,const void *base,size_t span){
 uintptr_t a=(uintptr_t)p,b=(uintptr_t)base;
 if(!n || !span)return 0;
 return a<=b ? b-a<n : a-b<span;
}
static int good_document(const sdm_document *doc){
 return doc && doc->bytes<SDM_TEXT_CAPACITY && doc->content[doc->bytes]==0 &&
        sdm_valid_utf8(doc->content,doc->bytes);
}
static int boundary(const sdm_document *doc,size_t offset){
 return offset<=doc->bytes && (offset==doc->bytes ||
    (((unsigned char)doc->content[offset]&0xc0)!=0x80));
}
static int match_at(const sdm_document *doc,size_t start,const char *needle,size_t len){
 return start<=doc->bytes && len<=doc->bytes-start && boundary(doc,start) &&
        boundary(doc,start+len) && memcmp(doc->content+start,needle,len)==0;
}
/* Explicitly byte-exact: no pretend Unicode case folding or stemming. */
int sde_find(const sdm_document *doc,const char *needle,size_t len,
             size_t start,int wrap,size_t *found){
 if(!good_document(doc)||!needle||!found||!len||len>NEEDLE_CAP||
    !sdm_valid_utf8(needle,len))return SDM_ARGUMENT;
 if(!boundary(doc,start))return SDM_RANGE;
 for(size_t i=start;i+len<=doc->bytes;i++)if(match_at(doc,i,needle,len)){
   *found=i;return SDM_OK;
 }
 if(wrap)for(size_t i=0;i<start && i+len<=doc->bytes;i++)
   if(match_at(doc,i,needle,len)){*found=i;return SDM_OK;}
 return SDM_NOT_FOUND;
}

/* Scan from left to right; do not recursively replace newly inserted text.
 * All content, comments, cursor and dirty status are updated atomically. */
int sde_replace_all(sdm_document *doc,const char *needle,size_t needle_len,
        const char *replacement,size_t replacement_len,sdm_document *staged,
        size_t *replaced){
 if(!good_document(doc)||!needle||(!replacement&&replacement_len)||!staged||
    staged==doc||!replaced||!needle_len||needle_len>NEEDLE_CAP||
    replacement_len>REPLACEMENT_CAP||
    !sdm_valid_utf8(needle,needle_len)||
    !sdm_valid_utf8(replacement,replacement_len))return SDM_ARGUMENT;
 if(intersects(staged,sizeof(*staged),doc,sizeof(*doc)) ||
    intersects(needle,needle_len,doc,sizeof(*doc)) ||
    intersects(needle,needle_len,staged,sizeof(*staged)) ||
    intersects(replacement,replacement_len,doc,sizeof(*doc)) ||
    intersects(replacement,replacement_len,staged,sizeof(*staged)))return SDM_ARGUMENT;
 /* Pass 1: length check. Failure never changes the destination. */
 size_t old=0,output_bytes=0,matches=0;
 while(old<doc->bytes){
   if(match_at(doc,old,needle,needle_len)){
     if(output_bytes>SDM_TEXT_CAPACITY-1-replacement_len)return SDM_CAPACITY;
     output_bytes+=replacement_len;old+=needle_len;matches++;
   }else{
     if(output_bytes>=SDM_TEXT_CAPACITY-1)return SDM_CAPACITY;
     output_bytes++;old++;
   }
 }
 if(matches==0){*replaced=0;return SDM_OK;}
 /* Pass 2: stage all bytes, preserving the original until success. */
 *staged=*doc;old=0;size_t out=0;
 while(old<doc->bytes){
   if(match_at(doc,old,needle,needle_len)){
     if(replacement_len)memcpy(staged->content+out,replacement,replacement_len);
     out+=replacement_len;old+=needle_len;
   }else staged->content[out++]=doc->content[old++];
 }
 if(out!=output_bytes)return SDM_ARGUMENT;
 staged->content[out]=0;staged->bytes=out;
 /* The cursor collapses to the end of the last replacement. */
 old=0;out=0;size_t last=0;
 while(old<doc->bytes){
   if(match_at(doc,old,needle,needle_len)){
     old+=needle_len;out+=replacement_len;last=out;
   }else{old++;out++;}
 }
 staged->cursor=last;staged->anchor=last;
 for(size_t i=0;i<SDM_MAX_COMMENTS;i++){
   const sdm_comment *original=&doc->comments[i];
   sdm_comment *dst=&staged->comments[i];
   if(!original->active)continue;
   /* A replacement intersecting a comment invalidates that anchor; edits
    * strictly before the comment shift its position, not its content. */
   int overlap=0;int64_t shift=0;
   old=0;
   while(old<doc->bytes){
     if(match_at(doc,old,needle,needle_len)){
       if(old<original->to && old+needle_len>original->from)overlap=1;
       if(old+needle_len<=original->from)
         shift+=(int64_t)replacement_len-(int64_t)needle_len;
       old+=needle_len;
     }else old++;
   }
   if(overlap)dst->active=0;
   else{
     int64_t from=(int64_t)original->from+shift;
     int64_t to=(int64_t)original->to+shift;
     if(from<0 || to<from || (uint64_t)to>output_bytes)return SDM_RANGE;
     dst->from=(size_t)from;dst->to=(size_t)to;
   }
 }
 staged->changed=1;
 *doc=*staged;
 *replaced=matches;
 return SDM_OK;
}
