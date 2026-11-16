#include "styleos/storage_demo.h"

#define FDT_BEGIN_NODE 1u
#define FDT_END_NODE 2u
#define FDT_PROP 3u
#define FDT_NOP 4u
#define FDT_END 9u
#define FDT_DEPTH_MAX 32u
#define MMIO_START UINT64_C(0x0a000000)
#define MMIO_END UINT64_C(0x0a004000)

static uint32_t be32(const unsigned char *p)
{
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
           ((uint32_t)p[2]<<8)|p[3];
}
static uint64_t be64(const unsigned char *p)
{
    return ((uint64_t)be32(p)<<32)|be32(p+4);
}
static int same(const unsigned char *p,size_t n,const char *s)
{
    size_t i=0;while(s[i]){if(i>=n || p[i]!=(unsigned char)s[i])return 0;++i;}
    return i==n;
}
static int string(const unsigned char *p,size_t n,const char *s)
{
    return n && p[n-1]==0 && same(p,n-1,s);
}
static int word_list_has(const unsigned char *p,size_t n,const char *s)
{
    if(!n || p[n-1])return -1;
    size_t at=0;int found=0;
    while(at<n){size_t end=at;while(end<n && p[end])++end;
        if(end==n || end==at)return -1;
        if(same(p+at,end-at,s))found=1;
        at=end+1;
    }
    return found;
}
static int span(size_t start,size_t bytes,size_t total)
{
    return start<=total && bytes<=total-start;
}
static int overlaps(size_t a,size_t an,size_t b,size_t bn)
{
    return a<b+bn && b<a+an;
}
static enum storage_phase phase(const unsigned char *p,size_t n)
{
    const char *const names[]={"", "init-a", "read-a", "update-b", "read-b",
                               "prepare-b", "recover-a"};
    for(unsigned i=1;i<sizeof names/sizeof names[0];++i)
        if(same(p,n,names[i]))return (enum storage_phase)i;
    return STORAGE_PHASE_NONE;
}
static enum storage_fdt_error bootargs(const unsigned char *p,size_t n,
                                      enum storage_phase *out)
{
    static const char key[]="styleos.storage=";
    if(!n || p[n-1])return SF_PHASE;
    unsigned found=0;enum storage_phase selected=STORAGE_PHASE_NONE;
    size_t at=0;
    while(at<n-1){
        while(at<n-1 && (p[at]==' ' || p[at]=='\t'))++at;
        if(at==n-1)break;
        size_t end=at;while(end<n-1 && p[end]!=' ' && p[end]!='\t'){
            if(!p[end])return SF_PHASE;
            ++end;
        }
        size_t k=0;while(key[k] && at+k<end && p[at+k]==(unsigned char)key[k])++k;
        if(!key[k]){
            if(++found!=1)return SF_PHASE;
            selected=phase(p+at+k,end-at-k);if(!selected)return SF_PHASE;
        }
        at=end;
    }
    if(found!=1)return SF_PHASE;
    *out=selected;return SF_OK;
}
struct node {
    unsigned chosen,compatible,compatible_seen,reg_seen,reg_valid,status_seen,disabled,child_seen;
    uint64_t base,bytes;
};
enum storage_fdt_error storage_fdt_parse(const void *source,size_t available,
                                        struct storage_boot *out)
{
    if(!source || !out || available<40 || available>BOOT_DTB_MAX ||
       (uintptr_t)out%_Alignof(struct storage_boot) ||
       (uintptr_t)source>UINTPTR_MAX-available ||
       (uintptr_t)out>UINTPTR_MAX-sizeof *out)return SF_ARGUMENT;
    uintptr_t a=(uintptr_t)source,b=(uintptr_t)out;
    if(a<b+sizeof *out && b<a+available)return SF_ARGUMENT;
    const unsigned char *data=source;
    if(be32(data)!=UINT32_C(0xd00dfeed))return SF_HEADER;
    size_t total=be32(data+4),structure=be32(data+8),strings=be32(data+12);
    size_t reserve=be32(data+16),strings_n=be32(data+32),structure_n=be32(data+36);
    if(total!=available || total<40 || structure<40 || strings<40 || reserve<40 ||
       structure%4 || reserve%8 || be32(data+20)<17 || be32(data+24)>17 ||
       !span(structure,structure_n,total) || !span(strings,strings_n,total) ||
       !span(reserve,16,total) || overlaps(structure,structure_n,strings,strings_n))return SF_HEADER;
    struct storage_boot result={0};struct node nodes[FDT_DEPTH_MAX]={0};
    unsigned depth=0,root_seen=0,root_address=0,root_size=0,chosen_seen=0,args_seen=0;
    size_t cursor=structure,limit=structure+structure_n;
    int ended=0;
    while(cursor<limit){
        if(!span(cursor,4,limit))return SF_BOUNDS;
        uint32_t token=be32(data+cursor);cursor+=4;
        if(token==FDT_BEGIN_NODE){
            if(depth==FDT_DEPTH_MAX)return SF_LIMIT;
            size_t start=cursor;while(cursor<limit && data[cursor])++cursor;
            if(cursor==limit)return SF_BOUNDS;
            size_t name_n=cursor-start;++cursor;
            if(cursor>SIZE_MAX-3)return SF_BOUNDS;
            cursor=(cursor+3)&~(size_t)3;if(cursor>limit)return SF_BOUNDS;
            if(depth==0){if(root_seen || name_n)return SF_FORMAT;root_seen=1;}
            else{
                if(!root_address || !root_size)return SF_PROFILE;
                nodes[depth-1].child_seen=1;
            }
            nodes[depth]=(struct node){0};
            if(depth==1 && same(data+start,name_n,"chosen")){
                if(chosen_seen++)return SF_PHASE;
                nodes[depth].chosen=1;
            }
            ++depth;
        }else if(token==FDT_END_NODE){
            if(!depth)return SF_FORMAT;
            struct node *n=&nodes[depth-1];
            if(n->compatible){
                if(depth!=2 || !n->reg_seen || !n->reg_valid)return SF_PROFILE;
                if(!n->disabled){
                    if(result.count==STORAGE_MMIO_MAX)return SF_LIMIT;
                    if(n->bytes!=UINT64_C(0x200) || n->base<MMIO_START ||
                       n->base>=MMIO_END || (n->base-MMIO_START)%UINT64_C(0x200))return SF_PROFILE;
                    for(size_t i=0;i<result.count;++i)
                        if(result.mmio[i].base==n->base)return SF_PROFILE;
                    result.mmio[result.count++]=(struct storage_mmio){n->base,n->bytes};
                }
            }
            --depth;
        }else if(token==FDT_PROP){
            if(!depth || !span(cursor,8,limit))return SF_FORMAT;
            uint32_t bytes=be32(data+cursor),nameoff=be32(data+cursor+4);cursor+=8;
            if(!span(cursor,bytes,limit) || nameoff>=strings_n)return SF_BOUNDS;
            const unsigned char *p=data+cursor,*name=data+strings+nameoff;
            size_t name_n=0;while(name_n<strings_n-nameoff && name[name_n])++name_n;
            if(name_n==strings_n-nameoff)return SF_BOUNDS;
            if(nodes[depth-1].child_seen)return SF_FORMAT;
            if(depth==1 && same(name,name_n,"#address-cells")){
                if(root_address++ || bytes!=4 || be32(p)!=2)return SF_PROFILE;
            }else if(depth==1 && same(name,name_n,"#size-cells")){
                if(root_size++ || bytes!=4 || be32(p)!=2)return SF_PROFILE;
            }else if(same(name,name_n,"compatible")){
                if(nodes[depth-1].compatible_seen++)return SF_FORMAT;
                int has=word_list_has(p,bytes,"virtio,mmio");if(has<0)return SF_FORMAT;
                if(has)nodes[depth-1].compatible=1;
            }else if(same(name,name_n,"reg")){
                if(nodes[depth-1].reg_seen++)return SF_FORMAT;
                if(depth==2 && bytes==16){
                    nodes[depth-1].reg_valid=1;
                    nodes[depth-1].base=be64(p);nodes[depth-1].bytes=be64(p+8);
                }
            }else if(same(name,name_n,"status")){
                if(nodes[depth-1].status_seen++)return SF_FORMAT;
                if(!string(p,bytes,"okay") && !string(p,bytes,"ok")){
                    if(!string(p,bytes,"disabled"))return SF_PROFILE;
                    nodes[depth-1].disabled=1;
                }
            }else if(nodes[depth-1].chosen && same(name,name_n,"bootargs")){
                if(args_seen++)return SF_PHASE;
                enum storage_fdt_error e=bootargs(p,bytes,&result.phase);if(e)return e;
            }
            cursor+=bytes;if(cursor>SIZE_MAX-3)return SF_BOUNDS;
            cursor=(cursor+3)&~(size_t)3;if(cursor>limit)return SF_BOUNDS;
        }else if(token==FDT_NOP){
            continue;
        }else if(token==FDT_END){
            if(depth || !root_seen || ended)return SF_FORMAT;
            ended=1;break;
        }else return SF_FORMAT;
    }
    if(!ended || !root_address || !root_size || !chosen_seen || !args_seen ||
       !result.phase || !result.count)return SF_PROFILE;
    while(cursor<limit){if(!span(cursor,4,limit) || be32(data+cursor)!=FDT_NOP)return SF_FORMAT;cursor+=4;}
    *out=result;return SF_OK;
}
const char *storage_phase_name(enum storage_phase p)
{
    static const char *const names[]={"none","init-a","read-a","update-b","read-b","prepare-b","recover-a"};
    return (unsigned)p<sizeof names/sizeof names[0]?names[p]:"unknown";
}
const char *storage_fdt_error_name(enum storage_fdt_error e)
{
    static const char *const names[]={"ok","argument","header","bounds","format","profile","phase","limit"};
    return (unsigned)e<sizeof names/sizeof names[0]?names[e]:"unknown";
}
