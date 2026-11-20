#include "styleos/documents_import.h"
#include <stdint.h>
#include <string.h>

#define MAX_ARCHIVE (458752u)
#define LOCAL_MAGIC (0x04034b50u)
#define CENTRAL_MAGIC (0x02014b50u)
#define END_MAGIC (0x06054b50u)
static unsigned rd16(const uint8_t *s){return (unsigned)s[0]|((unsigned)s[1]<<8);}
static uint32_t rd32(const uint8_t *s){return (uint32_t)rd16(s)|((uint32_t)rd16(s+2)<<16);}
static uint32_t crc32_local(const uint8_t *s,size_t n){
    uint32_t c=0xffffffffu;
    for(size_t i=0;i<n;i++){
        c^=s[i];
        for(int k=0;k<8;k++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);
    }
    return ~c;
}
static const uint8_t *find_bytes(const uint8_t *p,size_t n,const char *lit){
    size_t len=strlen(lit);
    if(len>n)return NULL;
    for(size_t i=0;i<=n-len;i++)if(memcmp(p+i,lit,len)==0)return p+i;
    return NULL;
}
static int putc_doc(sdm_document *d,unsigned char c){
    if(d->bytes>=SDM_TEXT_CAPACITY-1)return SDM_CAPACITY;
    d->content[d->bytes++]=(char)c;
    d->content[d->bytes]=0;
    return SDM_OK;
}
static int cp_doc(sdm_document *d,uint32_t cp){
    if(cp==0 || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))return SDM_ENCODING;
    if(cp<0x80)return putc_doc(d,(unsigned char)cp);
    if(cp<0x800)return putc_doc(d,(unsigned char)(0xc0|(cp>>6)))==SDM_OK?
        putc_doc(d,(unsigned char)(0x80|(cp&63))):SDM_CAPACITY;
    if(cp<0x10000){
        if(putc_doc(d,(unsigned char)(0xe0|(cp>>12)))!=SDM_OK)return SDM_CAPACITY;
        if(putc_doc(d,(unsigned char)(0x80|((cp>>6)&63)))!=SDM_OK)return SDM_CAPACITY;
        return putc_doc(d,(unsigned char)(0x80|(cp&63)));
    }
    if(putc_doc(d,(unsigned char)(0xf0|(cp>>18)))!=SDM_OK)return SDM_CAPACITY;
    if(putc_doc(d,(unsigned char)(0x80|((cp>>12)&63)))!=SDM_OK)return SDM_CAPACITY;
    if(putc_doc(d,(unsigned char)(0x80|((cp>>6)&63)))!=SDM_OK)return SDM_CAPACITY;
    return putc_doc(d,(unsigned char)(0x80|(cp&63)));
}
static int xml_text(sdm_document *d,const uint8_t *p,size_t n){
    for(size_t i=0;i<n;i++){
        if(p[i]!='&'){
            if(p[i]==0 || p[i]=='<' || p[i]=='>')return SDM_ENCODING;
            if(putc_doc(d,p[i])!=SDM_OK)return SDM_CAPACITY;
            continue;
        }
        const uint8_t *e=find_bytes(p+i,n-i,";");
        if(!e || (size_t)(e-(p+i))>12)return SDM_ENCODING;
        size_t el=(size_t)(e-(p+i))+1;
        uint32_t cp=0;
        if(el==5 && memcmp(p+i,"&amp;",5)==0)cp='&';
        else if(el==4 && memcmp(p+i,"&lt;",4)==0)cp='<';
        else if(el==4 && memcmp(p+i,"&gt;",4)==0)cp='>';
        else if(el==6 && memcmp(p+i,"&quot;",6)==0)cp='"';
        else if(el==6 && memcmp(p+i,"&apos;",6)==0)cp='\'';
        else if(el>=4 && p[i+1]=='#'){
            size_t k=i+2;int base=10;
            if(k<n && (p[k]=='x'||p[k]=='X')){base=16;k++;}
            if(k>=i+el-1)return SDM_ENCODING;
            for(;k<i+el-1;k++){
                unsigned v;
                if(p[k]>='0'&&p[k]<='9')v=p[k]-'0';
                else if(base==16&&p[k]>='a'&&p[k]<='f')v=p[k]-'a'+10;
                else if(base==16&&p[k]>='A'&&p[k]<='F')v=p[k]-'A'+10;
                else return SDM_ENCODING;
                if(cp>(0x10ffffu-v)/(unsigned)base)return SDM_ENCODING;
                cp=cp*(unsigned)base+v;
            }
        }else return SDM_ENCODING;
        if(cp_doc(d,cp)!=SDM_OK)return SDM_ENCODING;
        i+=el-1;
    }
    return SDM_OK;
}
static int uint_attr(const uint8_t *tag,size_t n,const char *attr,unsigned *value){
    const uint8_t *p=find_bytes(tag,n,attr);
    if(!p)return -1;
    size_t i=(size_t)(p-tag)+strlen(attr);unsigned v=0;int digits=0;
    while(i<n && tag[i]>='0' && tag[i]<='9'){
        if(v>6500)return -1;
        v=v*10u+(unsigned)(tag[i]-'0');
        i++;digits++;
    }
    if(!digits || i>=n || tag[i]!='"' || v>65535u)return -1;
    *value=v;return 0;
}
static void page_from_xml(const uint8_t *xml,size_t n,sdm_document *d){
    const uint8_t *p=find_bytes(xml,n,"<w:pgSz ");
    if(!p)return;
    size_t remaining=n-(size_t)(p-xml);
    const uint8_t *e=find_bytes(p,remaining,">");
    if(!e)return;
    size_t taglen=(size_t)(e-p);
    unsigned w,h;
    if(uint_attr(p,taglen,"w:w=\"",&w) || uint_attr(p,taglen,"w:h=\"",&h))return;
    sdm_page page=d->page;
    if((w==11906&&h==16838)||(w==16838&&h==11906))page.paper=SDM_A4;
    else if((w==12240&&h==15840)||(w==15840&&h==12240))page.paper=SDM_LETTER;
    else return;
    page.orientation=w>h?SDM_LANDSCAPE:SDM_PORTRAIT;
    p=find_bytes(xml,n,"<w:pgMar ");
    if(p){
        remaining=n-(size_t)(p-xml);e=find_bytes(p,remaining,">");
        if(e){
            taglen=(size_t)(e-p);
            unsigned top,bottom,left,right;
            if(!uint_attr(p,taglen,"w:top=\"",&top) && !uint_attr(p,taglen,"w:bottom=\"",&bottom) &&
               !uint_attr(p,taglen,"w:left=\"",&left) && !uint_attr(p,taglen,"w:right=\"",&right)){
                page.top_twips=(uint16_t)top;page.bottom_twips=(uint16_t)bottom;
                page.left_twips=(uint16_t)left;page.right_twips=(uint16_t)right;
            }
        }
    }
    (void)sdm_set_page(d,page); /* ignore invalid external layout instead of corrupting text */
}
static int text_from_xml(const uint8_t *xml,size_t n,sdm_document *doc){
    size_t i=0;int paragraphs=0,seen=0;
    while(i<n){
        const uint8_t *open=find_bytes(xml+i,n-i,"<");
        if(!open)break;
        i=(size_t)(open-xml);
        const uint8_t *close=find_bytes(xml+i,n-i,">");
        if(!close)return SDM_ENCODING;
        size_t end=(size_t)(close-xml)+1;
        size_t taglen=end-i;
        if(taglen>=5 && memcmp(xml+i,"<w:p",4)==0 &&
           (xml[i+4]=='>' || xml[i+4]==' ')){
            if(seen && putc_doc(doc,'\n')!=SDM_OK)return SDM_CAPACITY;
            seen=1;paragraphs++;
            if(paragraphs>8192)return SDM_CAPACITY;
        } else if(taglen>=5 && memcmp(xml+i,"<w:t",4)==0 &&
                  (xml[i+4]=='>' || xml[i+4]==' ')){
            if(!seen)return SDM_ENCODING;
            const uint8_t *tend=find_bytes(xml+end,n-end,"</w:t>");
            if(!tend)return SDM_ENCODING;
            int rc=xml_text(doc,xml+end,(size_t)(tend-(xml+end)));
            if(rc!=SDM_OK)return rc;
            end=(size_t)(tend-xml)+strlen("</w:t>");
        }else if(taglen>=6 && memcmp(xml+i,"<w:tab",6)==0){
            if(putc_doc(doc,'\t')!=SDM_OK)return SDM_CAPACITY;
        }else if(taglen>=5 && memcmp(xml+i,"<w:br",5)==0){
            if(putc_doc(doc,'\n')!=SDM_OK)return SDM_CAPACITY;
        }
        i=end;
    }
    if(!seen || !sdm_valid_utf8(doc->content,doc->bytes))return SDM_ENCODING;
    page_from_xml(xml,n,doc);
    doc->cursor=doc->anchor=doc->bytes;
    doc->changed=0;
    return SDM_OK;
}
int sdm_docx_import_stored(const uint8_t *zip,size_t bytes,sdm_document *out){
    if(!zip||!out)return SDM_ARGUMENT;
    if(bytes>MAX_ARCHIVE || bytes<30)return SDM_CAPACITY;
    sdm_init(out);
    const uint8_t *xml=NULL;size_t xml_len=0;size_t p=0;unsigned entries=0;
    while(p+4<=bytes){
        uint32_t magic=rd32(zip+p);
        if(magic==CENTRAL_MAGIC || magic==END_MAGIC)break;
        if(magic!=LOCAL_MAGIC || bytes-p<30)return SDM_ENCODING;
        unsigned flags=rd16(zip+p+6),method=rd16(zip+p+8);
        uint32_t crc=rd32(zip+p+14),size=rd32(zip+p+18),uncompressed=rd32(zip+p+22);
        size_t fn=rd16(zip+p+26),extra=rd16(zip+p+28);
        if(flags!=0||method!=0||size!=uncompressed||fn==0||fn>256)return SDM_ENCODING;
        if(fn>bytes-p-30 || extra>bytes-p-30-fn)return SDM_ENCODING;
        const uint8_t *name=zip+p+30;
        size_t start=p+30+fn+extra;
        if(size>bytes-start)return SDM_ENCODING;
        if(crc32_local(zip+start,size)!=crc)return SDM_ENCODING;
        if(fn==strlen("word/document.xml") && memcmp(name,"word/document.xml",fn)==0){
            if(xml)return SDM_ENCODING;
            xml=zip+start;xml_len=size;
        }
        p=start+size;
        if(++entries>48)return SDM_CAPACITY;
    }
    /* Reject truncated/misleading ZIP directories. A local header alone is not
       a committed document; ensure the central directory and EOCD agree. */
    if(bytes<22 || rd32(zip+bytes-22)!=END_MAGIC || rd16(zip+bytes-2)!=0)
        return SDM_ENCODING;
    const uint8_t *end=zip+bytes-22;
    if(rd16(end+4)!=0 || rd16(end+6)!=0 || rd16(end+8)!=entries ||
       rd16(end+10)!=entries)return SDM_ENCODING;
    size_t central=rd32(end+16), central_len=rd32(end+12);
    if(central!=p || central_len!=bytes-22-p)return SDM_ENCODING;
    size_t at=central;unsigned checked=0;
    while(at<bytes-22){
        if(bytes-22-at<46 || rd32(zip+at)!=CENTRAL_MAGIC)return SDM_ENCODING;
        size_t fl=rd16(zip+at+28),ex=rd16(zip+at+30),comment=rd16(zip+at+32);
        if(fl==0 || fl>256 || fl+ex+comment>bytes-22-at-46)return SDM_ENCODING;
        uint32_t local=rd32(zip+at+42);
        if(local>=p || bytes-local<30 || rd32(zip+local)!=LOCAL_MAGIC)return SDM_ENCODING;
        at+=46+fl+ex+comment;checked++;
    }
    if(at!=bytes-22 || checked!=entries)return SDM_ENCODING;
    if(!xml || xml_len>400000 || !entries)return SDM_NOT_FOUND;
    return text_from_xml(xml,xml_len,out);
}
