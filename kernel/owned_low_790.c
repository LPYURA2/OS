#include <styleos/owned_low_790.h>

#define PAGE UINT64_C(4096)
#define LIMIT (UINT64_C(1)<<47)
#define PE_W UINT32_C(0x80000000)
#define PE_X UINT32_C(0x20000000)
#define PE_R UINT32_C(0x40000000)

static uint16_t le16(const unsigned char *p)
{ return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1]<<8)); }
static uint32_t le32(const unsigned char *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static bool valid_span(uint64_t off,uint64_t len,uint64_t cap)
{ return off<=cap && len<=cap-off; }
static bool disjoint(uint64_t a,uint64_t b,uint64_t c,uint64_t d)
{ return b<=c || d<=a; }

static bool append_mapping(struct pg_plan *p,uint64_t at,uint64_t size,unsigned flags)
{
    if(!size || ((at | size)&(PAGE-1u)) || at>=LIMIT || size>LIMIT-at)
        return false;
    if(p->count && p->maps[p->count-1u].va +
            p->maps[p->count-1u].bytes == at &&
       p->maps[p->count-1u].flags==flags) {
        p->maps[p->count-1u].bytes+=size;
        return true;
    }
    if(p->count>=PG_MAX_MAPS)return false;
    p->maps[p->count++]=(struct pg_mapping){at,at,size,flags};
    return true;
}
static bool expected(struct pg_plan *p,uint64_t at,unsigned flags)
{
    struct pg_lookup found={0};
    return pg_lookup(p,at,&found)==PG_OK && found.pa==at && found.flags==flags;
}

bool s790_prepare_owned_low(const struct s790_low_request *req,
                             struct s790_low_result *out)
{
    if(out)*out=(struct s790_low_result){0};
    if(!req || !out || !req->loaded_image || !req->owned_tables ||
       req->image.start>=req->image.end ||
       (req->image.start|req->image.end|req->owned_tables_pa)&(PAGE-1u) ||
       req->image.end>=LIMIT ||
       (uint64_t)(uintptr_t)req->loaded_image!=req->image.start ||
       (uint64_t)(uintptr_t)req->owned_tables!=req->owned_tables_pa ||
       req->image.end-req->image.start>64u*1024u*1024u ||
       req->boot_stack.start>=req->boot_stack.end ||
       req->boot_stack.end>LIMIT ||
       req->boot_stack.end>UINT64_MAX-(PAGE-1u) ||
       req->required_exec<req->image.start || req->required_exec>=req->image.end ||
       req->required_write<req->image.start || req->required_write>=req->image.end ||
       req->required_rsp<req->boot_stack.start ||
       req->required_rsp>=req->boot_stack.end)
        return false;
    const uint64_t stack_start=req->boot_stack.start&~(PAGE-1u);
    const uint64_t stack_end=(req->boot_stack.end+PAGE-1u)&~(PAGE-1u);
    if(stack_start>=stack_end || stack_end>=LIMIT ||
       !disjoint(stack_start,stack_end,req->image.start,req->image.end) ||
       req->owned_tables_pa<req->image.start ||
       PG_TABLE_BYTES>req->image.end-req->owned_tables_pa)
        return false;

    const unsigned char *p=(const unsigned char*)req->loaded_image;
    const uint64_t size=req->image.end-req->image.start;
    if(size<256u || le16(p)!=0x5a4du || !valid_span(0x3cu,4u,size))return false;
    uint64_t pe=le32(p+0x3cu);
    if(!valid_span(pe,24u,size) || le32(p+pe)!=0x00004550u)return false;
    uint32_t count=le16(p+pe+6u),opt=le16(p+pe+20u);
    if(!count || count>32u || opt<64u ||
       !valid_span(pe+24u,opt,size) || le16(p+pe+24u)!=0x020bu ||
       le32(p+pe+24u+56u)!=size)return false;
    uint32_t headers=le32(p+pe+24u+60u);
    uint64_t sec=pe+24u+opt;
    if(!headers || headers>size || !valid_span(sec,(uint64_t)count*40u,size) ||
       sec+(uint64_t)count*40u>headers)return false;
    struct s790_low_result tmp={0};
    tmp.plan=(struct pg_plan){.arch=PG_X86,.pa_bits=48u,
        .tables=req->owned_tables,.tables_pa=req->owned_tables_pa,
        .capacity=PG_TABLE_PAGES,.probe=UINT64_C(0x400000001000)};
    for(uint64_t offset=0;offset<size;offset+=PAGE){
        unsigned role=offset<headers?1u:0u; /* bit 1: RO; bit 2: EXEC; bit 4: WRITE */
        for(uint32_t i=0;i<count;++i){
            const unsigned char *s=p+sec+(uint64_t)i*40u;
            uint64_t virtual_start=le32(s+12u),bytes=le32(s+8u);
            uint32_t flags=le32(s+36u);
            if(!bytes)continue;
            if(!virtual_start || virtual_start>=size || bytes>size-virtual_start ||
               !(flags&(PE_X|PE_W|PE_R)))return false;
            if(offset<virtual_start+bytes && offset+PAGE>virtual_start){
                if(flags&PE_X)role|=2u;
                if(flags&PE_W)role|=4u;
                if(!(flags&(PE_X|PE_W)))role|=1u;
            }
        }
        if(!role || (role&6u)==6u)return false;
        unsigned rights=(role&2u)?PG_EXEC:((role&4u)?PG_WRITE:0u);
        if(!append_mapping(&tmp.plan,req->image.start+offset,PAGE,rights))return false;
        if(rights&PG_EXEC)++tmp.code_pages;
        else if(rights&PG_WRITE)++tmp.writable_pages;
        else ++tmp.readonly_pages;
    }
    if(!append_mapping(&tmp.plan,stack_start,stack_end-stack_start,PG_WRITE))
        return false;
    tmp.stack_pages=(uint32_t)((stack_end-stack_start)/PAGE);
    tmp.built=pg_build(&tmp.plan);
    if(tmp.built.error!=PG_OK || tmp.built.root!=req->owned_tables_pa ||
       tmp.built.used<4u || tmp.built.used>PG_TABLE_PAGES ||
       tmp.plan.tables[0][128u] ||
       (tmp.plan.tables[0][0]&UINT64_C(0x3))!=3u ||
       ((tmp.plan.tables[0][0]&UINT64_C(0x000ffffffffff000))==
          (req->image.start&~(PAGE-1u))) ||
       !expected(&tmp.plan,req->required_exec&~(PAGE-1u),PG_EXEC) ||
       !expected(&tmp.plan,req->required_write&~(PAGE-1u),PG_WRITE) ||
       !expected(&tmp.plan,req->required_rsp&~(PAGE-1u),PG_WRITE))
        return false;
    for(size_t i=0;i<tmp.plan.count;++i){
        const struct pg_mapping *m=&tmp.plan.maps[i];
        for(uint64_t at=m->va;at<m->va+m->bytes;at+=PAGE)
            if(!expected(&tmp.plan,at,m->flags))return false;
    }
    tmp.new_low_slot=tmp.plan.tables[0][0];
    tmp.ready=true;
    *out=tmp;
    return true;
}

bool s790_audit_owned_low(const struct s790_low_result *low,
                           uint64_t current_code,uint64_t current_write,
                           uint64_t boot_rsp)
{
    if(!low || !low->ready || low->built.error!=PG_OK ||
       low->new_low_slot==0 || low->plan.tables[0][0]==0 ||
       ((low->plan.tables[0][0]^low->new_low_slot)&~UINT64_C(0x20)) ||
       low->plan.tables[0][128u] || low->built.used>PG_TABLE_PAGES ||
       low->built.used<4u)return false;
    /* Verify *all* live image and stack leaves, not just a representative. */
    for(size_t i=0;i<low->plan.count;++i){
        const struct pg_mapping *m=&low->plan.maps[i];
        for(uint64_t at=m->va;at<m->va+m->bytes;at+=PAGE)
            if(!expected((struct pg_plan*)&low->plan,at,m->flags))return false;
    }
    return expected((struct pg_plan*)&low->plan,current_code&~(PAGE-1u),PG_EXEC) &&
           expected((struct pg_plan*)&low->plan,current_write&~(PAGE-1u),PG_WRITE) &&
           expected((struct pg_plan*)&low->plan,boot_rsp&~(PAGE-1u),PG_WRITE);
}
