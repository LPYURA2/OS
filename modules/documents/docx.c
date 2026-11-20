#include "styleos/documents_docx.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t *dst;
    size_t pos;
    size_t cap;
    int err;
} writer;

static void bytes(writer *w, const void *buf, size_t n) {
    if (w->err || n > w->cap - w->pos) { w->err=1;return; }
    if (n) memcpy(w->dst+w->pos,buf,n);
    w->pos += n;
}
static void literal(writer *w, const char *s) { bytes(w,s,strlen(s)); }
static void u16(writer *w,uint16_t x){
    uint8_t b[2]={(uint8_t)x,(uint8_t)(x>>8)};bytes(w,b,2);
}
static void u32(writer *w,uint32_t x){
    uint8_t b[4]={(uint8_t)x,(uint8_t)(x>>8),(uint8_t)(x>>16),(uint8_t)(x>>24)};bytes(w,b,4);
}
static void number(writer *w, unsigned x) {
    char b[16];int n=snprintf(b,sizeof(b),"%u",x);
    if (n<0 || (size_t)n>=sizeof(b)){w->err=1;return;}
    bytes(w,b,(size_t)n);
}
static uint32_t crc32(const uint8_t *data,size_t n){
    uint32_t c=0xffffffffu;
    for(size_t i=0;i<n;i++){
        c^=data[i];
        for(unsigned b=0;b<8;b++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);
    }
    return ~c;
}
static void xmlchar(writer *w,unsigned char c){
    if(c=='&')literal(w,"&amp;");
    else if(c=='<')literal(w,"&lt;");
    else if(c=='>')literal(w,"&gt;");
    else if(c=='\"')literal(w,"&quot;");
    else if(c=='\'')literal(w,"&apos;");
    else bytes(w,&c,1);
}
static void paragraph_start(writer *w){literal(w,"<w:p><w:r><w:t xml:space=\"preserve\">");}
static void paragraph_end(writer *w){literal(w,"</w:t></w:r></w:p>");}
static int make_xml(const sdm_document *doc,writer *w){
    literal(w,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>");
    literal(w,"<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:body>");
    paragraph_start(w);
    for(size_t i=0;i<doc->bytes;i++){
        unsigned char c=(unsigned char)doc->content[i];
        if(c=='\n'){paragraph_end(w);paragraph_start(w);}
        else if(c=='\r'){ if(i+1<doc->bytes && doc->content[i+1]=='\n')continue;paragraph_end(w);paragraph_start(w); }
        else if(c=='\t')literal(w,"</w:t><w:tab/><w:t xml:space=\"preserve\">");
        else if(c<0x20){w->err=1;return SDM_ENCODING;}
        else xmlchar(w,c);
        if(w->err)return SDM_CAPACITY;
    }
    paragraph_end(w);
    literal(w,"<w:sectPr><w:pgSz w:w=\"");
    unsigned width=doc->page.paper==SDM_A4?11906:12240;
    unsigned height=doc->page.paper==SDM_A4?16838:15840;
    if(doc->page.orientation==SDM_LANDSCAPE){unsigned temp=width;width=height;height=temp;}
    number(w,width);literal(w,"\" w:h=\"");number(w,height);
    literal(w,doc->page.orientation==SDM_LANDSCAPE?"\" w:orient=\"landscape\"/>":"\"/>");
    literal(w,"<w:pgMar w:top=\"");number(w,doc->page.top_twips);
    literal(w,"\" w:bottom=\"");number(w,doc->page.bottom_twips);
    literal(w,"\" w:left=\"");number(w,doc->page.left_twips);
    literal(w,"\" w:right=\"");number(w,doc->page.right_twips);
    literal(w,"\" w:header=\"720\" w:footer=\"720\" w:gutter=\"0\"/></w:sectPr></w:body></w:document>");
    return w->err?SDM_CAPACITY:SDM_OK;
}
static const char ct[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
    "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
    "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
    "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
    "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
    "</Types>";
static const char rel[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
    "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
    "</Relationships>";

typedef struct {const char *name;uint32_t offset,length,crc;} part;
static void zip_local(writer *w,part *f,const char *name,const uint8_t *data,size_t n){
    if(w->err || w->pos>UINT32_MAX || n>UINT32_MAX || strlen(name)>UINT16_MAX){w->err=1;return;}
    f->name=name;f->offset=(uint32_t)w->pos;f->length=(uint32_t)n;f->crc=crc32(data,n);
    u32(w,0x04034b50);u16(w,20);u16(w,0);u16(w,0);u16(w,0);u16(w,0);
    u32(w,f->crc);u32(w,f->length);u32(w,f->length);u16(w,(uint16_t)strlen(name));u16(w,0);
    literal(w,name);bytes(w,data,n);
}
static void zip_directory(writer *w,const part *f){
    size_t namelen=strlen(f->name);
    u32(w,0x02014b50);u16(w,20);u16(w,20);u16(w,0);u16(w,0);u16(w,0);u16(w,0);
    u32(w,f->crc);u32(w,f->length);u32(w,f->length);
    u16(w,(uint16_t)namelen);u16(w,0);u16(w,0);u16(w,0);u16(w,0);
    u32(w,0);u32(w,f->offset);literal(w,f->name);
}

int sdm_docx_export(const sdm_document *doc,uint8_t *out,size_t capacity,size_t *written){
    if(!doc||!out||!written)return SDM_ARGUMENT;
    *written=0;
    if(doc->bytes>=SDM_TEXT_CAPACITY || !sdm_valid_utf8(doc->content,doc->bytes))return SDM_ENCODING;
    if(doc->content[doc->bytes]!='\0')return SDM_ENCODING;
    if(capacity>SDM_DOCX_MAX_OUTPUT)capacity=SDM_DOCX_MAX_OUTPUT;
    /* The XML buffer is caller-owned: use the tail of the output workspace. */
    const size_t xml_start=4096;
    if(xml_start>=capacity)return SDM_CAPACITY;
    writer xml={out+xml_start,0,capacity-xml_start,0};
    int status=make_xml(doc,&xml);
    if(status!=SDM_OK)return status;
    /* Don't overwrite XML while building archive: XML lives at the tail, and
     * the ZIP content will be constructed first in a separate stack-free region.
     * The in-place gap is released only after its bytes have been moved. */
    static const size_t reserve=512;
    size_t prefix_overhead=sizeof(ct)+sizeof(rel)+512;
    if(xml_start < prefix_overhead+reserve)return SDM_CAPACITY;
    writer zip={out,0,xml_start,0};
    part parts[3];
    zip_local(&zip,&parts[0],"[Content_Types].xml",(const uint8_t*)ct,strlen(ct));
    zip_local(&zip,&parts[1],"_rels/.rels",(const uint8_t*)rel,strlen(rel));
    if(zip.err)return SDM_CAPACITY;
    /* Full archive is rebuilt with source XML copied to its final position.
       After room is verified, compaction from the temporary XML tail is safe. */
    size_t header=30+strlen("word/document.xml");
    size_t final_xml=zip.pos+header;
    size_t xml_end=xml_start+xml.pos;
    if(final_xml+xml.pos>capacity || xml_end>capacity)return SDM_CAPACITY;
    /* Copy XML away from the output region that will host the central directory;
       memmove permits overlap. */
    memmove(out+final_xml,out+xml_start,xml.pos);
    /* ZIP writer now appends the local header without recopying its XML. */
    parts[2].name="word/document.xml";
    parts[2].offset=(uint32_t)zip.pos;
    parts[2].length=(uint32_t)xml.pos;
    parts[2].crc=crc32(out+final_xml,xml.pos);
    u32(&zip,0x04034b50);u16(&zip,20);u16(&zip,0);u16(&zip,0);u16(&zip,0);u16(&zip,0);
    u32(&zip,parts[2].crc);u32(&zip,parts[2].length);u32(&zip,parts[2].length);
    u16(&zip,(uint16_t)strlen(parts[2].name));u16(&zip,0);
    literal(&zip,parts[2].name);
    if(zip.err || zip.pos!=final_xml)return SDM_CAPACITY;
    zip.pos=final_xml+xml.pos;
    zip.cap=capacity;
    size_t central_start=zip.pos;
    for(size_t i=0;i<3;i++)zip_directory(&zip,&parts[i]);
    size_t central_len=zip.pos-central_start;
    if(central_start>UINT32_MAX || central_len>UINT32_MAX)return SDM_CAPACITY;
    u32(&zip,0x06054b50);u16(&zip,0);u16(&zip,0);u16(&zip,3);u16(&zip,3);
    u32(&zip,(uint32_t)central_len);u32(&zip,(uint32_t)central_start);u16(&zip,0);
    if(zip.err)return SDM_CAPACITY;
    *written=zip.pos;
    return SDM_OK;
}
