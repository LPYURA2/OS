#include <styleos/volume_service.h>
/* Block-layer lifetime query at a serialized kernel boundary. No I/O.
 * The slot owns the grant; a revoked/reused/detached token cannot expose
 * stale cached directory data. */
static enum bs_status grant_live(const struct sv_slot *v)
{
    const struct bs_service *b=v->block;
    if(!b||!v->token)return BS_STALE;
    if(b->busy)return BS_BUSY;
    for(unsigned i=0;i<BS_GRANTS;++i){
        const struct bs_grant *g=&b->grants[i];
        if(g->token!=v->token)continue;
        if(g->owner!=v->owner||g->device>=BS_DEVICES||
           g->rights!=(BS_READ|(v->writable?BS_WRITE:0))||
           g->length!=v->sectors)return BS_STALE;
        const struct bs_device *d=&b->devices[g->device];
        if(!g->generation||d->generation!=g->generation)return BS_STALE;
        if(d->recovery)return BS_RECOVERY;
        return BS_OK;
    }
    return BS_STALE;
}
static enum sv_status block_result(enum bs_status r)
{
    switch(r){
    case BS_OK:return SV_OK;case BS_INVALID:return SV_INVALID;
    case BS_DENIED:return SV_DENIED;case BS_STALE:return SV_STALE;
    case BS_BUSY:return SV_BUSY;case BS_FULL:return SV_FULL;
    case BS_RANGE:return SV_RANGE;case BS_IO:return SV_IO;
    case BS_UNCERTAIN:return SV_UNCERTAIN;case BS_RECOVERY:return SV_RECOVERY;
    }
    return SV_IO;
}
static enum sv_status fs_result(enum sfs_result r)
{
    switch(r){
    case SFS_OK:return SV_OK;case SFS_IO:return SV_IO;
    case SFS_SIGNATURE:return SV_SIGNATURE;case SFS_CORRUPT:return SV_CORRUPT;
    case SFS_STATE:return SV_RECOVERY;case SFS_NAME:return SV_NAME;
    case SFS_NOT_FOUND:return SV_NOT_FOUND;case SFS_EXISTS:return SV_EXISTS;
    case SFS_TYPE:return SV_TYPE;case SFS_FULL:return SV_FULL;
    case SFS_RANGE:return SV_RANGE;case SFS_NOT_EMPTY:case SFS_DENIED:return SV_DENIED;
    }
    return SV_IO;
}
static bool read_sector(void *ctx,uint32_t lba,uint8_t out[512])
{
    struct sv_slot *v=ctx;
    v->last_block=bs_read_sector(v->block,v->owner,v->token,lba,out);
    return v->last_block==BS_OK;
}
static bool write_sector(void *ctx,uint32_t lba,const uint8_t in[512])
{
    struct sv_slot *v=ctx;
    v->last_block=bs_write_sector(v->block,v->owner,v->token,lba,in);
    return v->last_block==BS_OK;
}
static bool flush(void *ctx)
{
    /* Every accepted sector write is already flushed by BS; verify grant
     * remains live. No extra transport command or fictitious deferred I/O. */
    struct sv_slot *v=ctx;
    v->last_block=grant_live(v);
    return v->last_block==BS_OK;
}
static struct sv_slot *slot(struct sv_service *s,uint64_t mount)
{
    for(unsigned i=0;i<SV_SLOTS;++i)
        if(s->slots[i].active&&s->slots[i].mount==mount)return &s->slots[i];
    return 0;
}
static enum sv_status live(struct sv_service *s,uint64_t mount,struct sv_slot **out)
{
    if(!s||!mount)return SV_INVALID;
    if(s->busy)return SV_BUSY;
    struct sv_slot *v=slot(s,mount);
    if(!v)return SV_STALE;
    enum bs_status b=grant_live(v);
    if(b!=BS_OK)return block_result(b);
    if(!v->fs.mounted)return SV_RECOVERY;
    *out=v;return SV_OK;
}
static enum sv_status entry(struct sv_service *s,struct sv_ref ref,struct sv_slot **out,
                            const struct sfs_entry **e)
{
    enum sv_status r=live(s,ref.mount,out);
    if(r!=SV_OK)return r;
    if(ref.revision!=(*out)->fs.generation)return SV_STALE;
    *e=sfs_entry_at(&(*out)->fs,ref.index);
    return *e?SV_OK:SV_NOT_FOUND;
}
static struct sv_ref reference(const struct sv_slot *v,unsigned index)
{
    return (struct sv_ref){v->mount,v->fs.generation,index};
}
static enum sv_status finish(struct sv_service *s,struct sv_slot *v,enum sfs_result r)
{
    enum sv_status status=v->last_block==BS_OK?fs_result(r):block_result(v->last_block);
    s->busy=false;return status;
}
enum sv_status sv_mount(struct sv_service *s,struct bs_service *b,uint64_t gen,
                        uint64_t owner,uint64_t start,uint32_t sectors,bool writable,uint64_t *mount)
{
    if(!s||!b||!owner||!mount||!sfs_capacity_valid(sectors))return SV_INVALID;
    if(s->busy)return SV_BUSY;
    unsigned i=0;while(i<SV_SLOTS&&s->slots[i].active)++i;
    if(i==SV_SLOTS||s->serial==UINT64_MAX)return SV_FULL;
    struct sv_slot *v=&s->slots[i];
    s->busy=true;
    uint64_t token=0;
    enum bs_status r=bs_grant_range(b,gen,owner,start,sectors,BS_READ|(writable?BS_WRITE:0),&token);
    if(r!=BS_OK){s->busy=false;return block_result(r);}
    v->block=b;v->owner=owner;v->token=token;v->sectors=sectors;v->writable=writable;v->last_block=BS_OK;
    const struct sfs_io io={v,sectors,read_sector,writable?write_sector:0,writable?flush:0};
    enum sfs_result fs=sfs_mount(&v->fs,&v->work,&io);
    if(fs!=SFS_OK){
        enum sv_status why=v->last_block==BS_OK?fs_result(fs):block_result(v->last_block);
        (void)bs_revoke(b,owner,token);v->token=0;v->block=0;v->fs.mounted=false;
        s->busy=false;return why;
    }
    v->mount=++s->serial;v->active=true;*mount=v->mount;
    s->busy=false;return SV_OK;
}
enum sv_status sv_unmount(struct sv_service *s,uint64_t mount)
{
    if(!s||!mount)return SV_INVALID;
    if(s->busy)return SV_BUSY;
    struct sv_slot *v=slot(s,mount);
    if(!v)return SV_STALE;
    enum bs_status b=bs_revoke(v->block,v->owner,v->token);
    if(b!=BS_OK&&b!=BS_STALE)return block_result(b);
    for(unsigned i=0;i<SV_SCOPES;++i)
        if(s->scopes[i].mount==mount)s->scopes[i]=(struct sv_scope){0};
    v->active=false;v->fs.mounted=false;v->token=0;v->block=0;v->mount=0;
    return SV_OK;
}
enum sv_status sv_info(struct sv_service *s,uint64_t mount,struct sv_info *out)
{
    if(!out)return SV_INVALID;
    struct sv_slot *v;enum sv_status r=live(s,mount,&v);
    if(r!=SV_OK)return r;
    *out=(struct sv_info){v->mount,v->sectors,v->fs.generation,v->fs.format,v->writable};
    return SV_OK;
}
enum sv_status sv_root(struct sv_service *s,uint64_t mount,struct sv_ref *out)
{
    if(!out)return SV_INVALID;
    struct sv_slot *v;enum sv_status r=live(s,mount,&v);
    if(r!=SV_OK)return r;
    *out=reference(v,SFS_ROOT);return SV_OK;
}
enum sv_status sv_lookup(struct sv_service *s,struct sv_ref parent,const char *name,struct sv_ref *out)
{
    if(!name||!out)return SV_INVALID;
    struct sv_slot *v;const struct sfs_entry *e;
    enum sv_status r=entry(s,parent,&v,&e);if(r!=SV_OK)return r;
    if(e->type!=SFS_DIRECTORY)return SV_TYPE;
    int id=sfs_find(&v->fs,parent.index,name);
    if(id<0)return SV_NOT_FOUND;
    *out=reference(v,(unsigned)id);return SV_OK;
}
enum sv_status sv_stat(struct sv_service *s,struct sv_ref ref,struct sfs_entry *out)
{
    if(!out)return SV_INVALID;
    struct sv_slot *v;const struct sfs_entry *e;
    enum sv_status r=entry(s,ref,&v,&e);if(r!=SV_OK)return r;
    *out=*e;return SV_OK;
}
enum sv_status sv_next(struct sv_service *s,struct sv_ref parent,unsigned after,struct sv_ref *out,struct sfs_entry *meta)
{
    if(!out||!meta||after>=SFS_ENTRIES)return SV_INVALID;
    struct sv_slot *v;const struct sfs_entry *e;
    enum sv_status r=entry(s,parent,&v,&e);if(r!=SV_OK)return r;
    if(e->type!=SFS_DIRECTORY)return SV_TYPE;
    for(unsigned i=after+1;i<SFS_ENTRIES;++i){
        e=sfs_entry_at(&v->fs,i);
        if(e&&e->parent==parent.index){*out=reference(v,i);*meta=*e;return SV_OK;}
    }
    return SV_NOT_FOUND;
}
enum sv_status sv_mkdir(struct sv_service *s,struct sv_ref parent,const char *name,struct sv_ref *out)
{
    if(!name||!out)return SV_INVALID;
    struct sv_slot *v;const struct sfs_entry *e;
    enum sv_status r=entry(s,parent,&v,&e);if(r!=SV_OK)return r;
    if(!v->writable)return SV_DENIED;
    if(e->type!=SFS_DIRECTORY)return SV_TYPE;
    unsigned id=0;s->busy=true;v->last_block=BS_OK;
    enum sfs_result fs=sfs_mkdir(&v->fs,&v->work,parent.index,name,&id);
    r=finish(s,v,fs);if(r==SV_OK)*out=reference(v,id);return r;
}
enum sv_status sv_create(struct sv_service *s,struct sv_ref parent,const char *name,const uint8_t *data,size_t n,struct sv_ref *out)
{
    if(!name||!out||(n&&!data))return SV_INVALID;
    struct sv_slot *v;const struct sfs_entry *e;
    enum sv_status r=entry(s,parent,&v,&e);if(r!=SV_OK)return r;
    if(!v->writable)return SV_DENIED;
    if(e->type!=SFS_DIRECTORY)return SV_TYPE;
    unsigned id=0;s->busy=true;v->last_block=BS_OK;
    enum sfs_result fs=sfs_create(&v->fs,&v->work,parent.index,name,data,n,&id);
    r=finish(s,v,fs);if(r==SV_OK)*out=reference(v,id);return r;
}
enum sv_status sv_read_to(struct sv_service *s,struct sv_ref ref,sfs_sink sink,void *ctx)
{
    if(!sink)return SV_INVALID;
    struct sv_slot *v;const struct sfs_entry *e;
    enum sv_status r=entry(s,ref,&v,&e);if(r!=SV_OK)return r;
    if(e->type!=SFS_FILE)return SV_TYPE;
    s->busy=true;v->last_block=BS_OK;
    return finish(s,v,sfs_read_to(&v->fs,ref.index,sink,ctx));
}

/* Scope checks use the real validated directory graph, not path prefixes
 * or a caller-supplied entry number alone. Bound traversal even if an
 * internal invariant is violated. Raw sv_* operations remain trusted-only. */
static struct sv_scope *scope_find(struct sv_service *s,uint64_t token)
{
    if(!token)return 0;
    for(unsigned i=0;i<SV_SCOPES;++i)
        if(s->scopes[i].token==token)return &s->scopes[i];
    return 0;
}
static enum sv_status scope_live(struct sv_service *s,uint64_t owner,uint64_t token,unsigned right,
                                 struct sv_scope **out,struct sv_slot **v)
{
    if(!s||!owner||!token)return SV_INVALID;
    if(s->busy)return SV_BUSY;
    struct sv_scope *q=scope_find(s,token);
    if(!q)return SV_STALE;
    if(q->owner!=owner||!(q->rights&right))return SV_DENIED;
    enum sv_status r=live(s,q->mount,v);
    if(r!=SV_OK)return r;
    if(q->revision!=(*v)->fs.generation)return SV_STALE;
    if(right==SV_SCOPE_WRITE&&!(*v)->writable)return SV_DENIED;
    const struct sfs_entry *anchor=sfs_entry_at(&(*v)->fs,q->root);
    if(!anchor||anchor->type!=SFS_DIRECTORY)return SV_STALE;
    *out=q;return SV_OK;
}
static enum sv_status scope_entry(struct sv_service *s,uint64_t owner,uint64_t token,unsigned right,
                                  struct sv_ref ref,struct sv_scope **q,struct sv_slot **v)
{
    enum sv_status r=scope_live(s,owner,token,right,q,v);
    if(r!=SV_OK)return r;
    if(ref.mount!=(*q)->mount)return SV_DENIED;
    if(ref.revision!=(*q)->revision)return SV_STALE;
    return sfs_scope_contains(&(*v)->fs,(*q)->root,ref.index)?SV_OK:SV_DENIED;
}
enum sv_status sv_scope_grant(struct sv_service *s,uint64_t owner,struct sv_ref profile,unsigned rights,uint64_t *token)
{
    if(!owner||!token||!(rights&SV_SCOPE_READ)||(rights&~(SV_SCOPE_READ|SV_SCOPE_WRITE)))return SV_INVALID;
    struct sv_slot *v;const struct sfs_entry *e;
    enum sv_status r=entry(s,profile,&v,&e);
    if(r!=SV_OK)return r;
    if(!sfs_scope_profile(&v->fs,profile.index))return SV_DENIED;
    if((rights&SV_SCOPE_WRITE)&&!v->writable)return SV_DENIED;
    unsigned i=0;while(i<SV_SCOPES&&s->scopes[i].token)++i;
    if(i==SV_SCOPES||s->serial==UINT64_MAX)return SV_FULL;
    s->scopes[i]=(struct sv_scope){++s->serial,owner,profile.mount,profile.revision,profile.index,rights};
    *token=s->serial;return SV_OK;
}
enum sv_status sv_scope_revoke(struct sv_service *s,uint64_t owner,uint64_t token)
{
    if(!s||!owner||!token)return SV_INVALID;
    if(s->busy)return SV_BUSY;
    struct sv_scope *q=scope_find(s,token);
    if(!q)return SV_STALE;
    if(q->owner!=owner)return SV_DENIED;
    *q=(struct sv_scope){0};return SV_OK;
}
enum sv_status sv_scope_revoke_owner(struct sv_service *s,uint64_t owner)
{
    if(!s||!owner)return SV_INVALID;
    if(s->busy)return SV_BUSY;
    for(unsigned i=0;i<SV_SCOPES;++i)
        if(s->scopes[i].owner==owner)s->scopes[i]=(struct sv_scope){0};
    return SV_OK;
}
enum sv_status sv_scope_root(struct sv_service *s,uint64_t owner,uint64_t token,struct sv_ref *out)
{
    if(!out)return SV_INVALID;
    struct sv_scope *q;struct sv_slot *v;
    enum sv_status r=scope_live(s,owner,token,SV_SCOPE_READ,&q,&v);
    if(r!=SV_OK)return r;
    *out=reference(v,q->root);return SV_OK;
}
enum sv_status sv_scope_lookup(struct sv_service *s,uint64_t owner,uint64_t token,struct sv_ref parent,const char *name,struct sv_ref *out)
{
    struct sv_scope *q;struct sv_slot *v;
    enum sv_status r=scope_entry(s,owner,token,SV_SCOPE_READ,parent,&q,&v);
    return r==SV_OK?sv_lookup(s,parent,name,out):r;
}
enum sv_status sv_scope_next(struct sv_service *s,uint64_t owner,uint64_t token,struct sv_ref parent,unsigned after,struct sv_ref *out,struct sfs_entry *meta)
{
    struct sv_scope *q;struct sv_slot *v;
    enum sv_status r=scope_entry(s,owner,token,SV_SCOPE_READ,parent,&q,&v);
    return r==SV_OK?sv_next(s,parent,after,out,meta):r;
}
enum sv_status sv_scope_stat(struct sv_service *s,uint64_t owner,uint64_t token,struct sv_ref ref,struct sfs_entry *out)
{
    struct sv_scope *q;struct sv_slot *v;
    enum sv_status r=scope_entry(s,owner,token,SV_SCOPE_READ,ref,&q,&v);
    return r==SV_OK?sv_stat(s,ref,out):r;
}
enum sv_status sv_scope_read_to(struct sv_service *s,uint64_t owner,uint64_t token,struct sv_ref ref,sfs_sink sink,void *ctx)
{
    struct sv_scope *q;struct sv_slot *v;
    enum sv_status r=scope_entry(s,owner,token,SV_SCOPE_READ,ref,&q,&v);
    return r==SV_OK?sv_read_to(s,ref,sink,ctx):r;
}
static enum sv_status scope_mutated(struct sv_scope *q,struct sv_slot *v,enum sv_status r)
{
    if(r==SV_OK)q->revision=v->fs.generation;
    return r;
}
enum sv_status sv_scope_mkdir(struct sv_service *s,uint64_t owner,uint64_t token,struct sv_ref parent,const char *name,struct sv_ref *out)
{
    struct sv_scope *q;struct sv_slot *v;
    enum sv_status r=scope_entry(s,owner,token,SV_SCOPE_WRITE,parent,&q,&v);
    if(r!=SV_OK)return r;
    r=sv_mkdir(s,parent,name,out);return scope_mutated(q,v,r);
}
enum sv_status sv_scope_create(struct sv_service *s,uint64_t owner,uint64_t token,struct sv_ref parent,const char *name,const uint8_t *data,size_t n,struct sv_ref *out)
{
    struct sv_scope *q;struct sv_slot *v;
    enum sv_status r=scope_entry(s,owner,token,SV_SCOPE_WRITE,parent,&q,&v);
    if(r!=SV_OK)return r;
    r=sv_create(s,parent,name,data,n,out);return scope_mutated(q,v,r);
}
enum sv_status sv_scope_write(struct sv_service *s,uint64_t owner,uint64_t token,struct sv_ref ref,const uint8_t *data,size_t n,struct sv_ref *out)
{
    if(!out||(n&&!data))return SV_INVALID;
    struct sv_scope *q;struct sv_slot *v;
    enum sv_status r=scope_entry(s,owner,token,SV_SCOPE_WRITE,ref,&q,&v);
    if(r!=SV_OK)return r;
    if(ref.index==q->root)return SV_DENIED;
    s->busy=true;v->last_block=BS_OK;
    r=finish(s,v,sfs_write(&v->fs,&v->work,ref.index,data,n));
    if(r==SV_OK)*out=reference(v,ref.index);
    return scope_mutated(q,v,r);
}
static enum sv_status scope_transfer(struct sv_service *s,uint64_t owner,uint64_t token,struct sv_ref source,
                                     struct sv_ref parent,const char *name,struct sv_ref *out,bool copy)
{
    if(!name||!out)return SV_INVALID;
    struct sv_scope *q;struct sv_slot *v;
    enum sv_status r=scope_entry(s,owner,token,SV_SCOPE_WRITE,source,&q,&v);
    if(r!=SV_OK)return r;
    if(source.index==q->root)return SV_DENIED;
    r=scope_entry(s,owner,token,SV_SCOPE_WRITE,parent,&q,&v);
    if(r!=SV_OK)return r;
    unsigned id=source.index;s->busy=true;v->last_block=BS_OK;
    enum sfs_result fs=copy?sfs_copy(&v->fs,&v->work,source.index,parent.index,name,&id):
        sfs_move(&v->fs,&v->work,source.index,parent.index,name);
    r=finish(s,v,fs);
    if(r==SV_OK)*out=reference(v,id);
    return scope_mutated(q,v,r);
}
enum sv_status sv_scope_move(struct sv_service *s,uint64_t owner,uint64_t token,struct sv_ref source,struct sv_ref parent,const char *name,struct sv_ref *out)
{return scope_transfer(s,owner,token,source,parent,name,out,false);}
enum sv_status sv_scope_copy(struct sv_service *s,uint64_t owner,uint64_t token,struct sv_ref source,struct sv_ref parent,const char *name,struct sv_ref *out)
{return scope_transfer(s,owner,token,source,parent,name,out,true);}
enum sv_status sv_scope_remove(struct sv_service *s,uint64_t owner,uint64_t token,struct sv_ref ref)
{
    struct sv_scope *q;struct sv_slot *v;
    enum sv_status r=scope_entry(s,owner,token,SV_SCOPE_WRITE,ref,&q,&v);
    if(r!=SV_OK)return r;
    if(ref.index==q->root)return SV_DENIED;
    s->busy=true;v->last_block=BS_OK;
    r=finish(s,v,sfs_remove(&v->fs,&v->work,ref.index));
    return scope_mutated(q,v,r);
}
