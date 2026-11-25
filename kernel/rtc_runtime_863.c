#include <styleos/rtc_runtime_863.h>
static bool valid(struct rtc_runtime_863 *r)
{
    struct rtc_runtime_observation_863 o={0};
    return r->ops->observe(r->ctx,&o) &&
        o.root==r->root && o.epoch==r->epoch &&
        o.cs && !(o.cs&3u) && !(o.rflags&0x200u) &&
        o.q35_tcg && o.single_cpu && o.post_ebs &&
        o.exclusive_cmos && o.known_shadow;
}
static bool guard(struct rtc_runtime_863 *r)
{
    if(r->quarantined || r->owner.quarantined) return false;
    if(!valid(r)){
        r->quarantined=true;
        r->owner.quarantined=true;
        return false;
    }
    return true;
}
static bool index_write(void *ctx,uint8_t index)
{
    struct rtc_runtime_863 *r=ctx;
    if(!r->busy || !r->bound || !r->owner.held || !guard(r))return false;
    bool ok=r->ops->index(r->ctx,index);
    return guard(r)&&ok;
}
static bool data_read(void *ctx,uint8_t *out)
{
    struct rtc_runtime_863 *r=ctx;
    uint8_t value=0;
    if(!out || !r->busy || !r->bound || !r->owner.held || !guard(r))return false;
    bool ok=r->ops->data(r->ctx,&value);
    if(!guard(r)||!ok)return false;
    *out=value;
    return true;
}
bool rtc_runtime_bind_863(struct rtc_runtime_863 *r,
    const struct rtc_runtime_ops_863 *ops,void *ctx,uint64_t root,
    uint64_t epoch,uint8_t known_index)
{
    static const struct rtc_port_ops_854 ports={index_write,data_read};
    if(!r || !ops || !ops->observe || !ops->index || !ops->data ||
       !root || (root&0xfffu) || !epoch ||
       r->bound || r->busy || r->quarantined || r->root || r->epoch ||
       r->ops || r->ctx || r->owner.admitted || r->owner.held ||
       r->owner.quarantined || r->owner.ops || r->owner.ctx ||
       r->owner.shadow || r->owner.saved)return false;
    r->busy=true;r->ops=ops;r->ctx=ctx;r->root=root;r->epoch=epoch;
    if(!guard(r)||!rtc_owner_init_854(&r->owner,&ports,r,true,known_index)){
        r->quarantined=true;r->busy=false;return false;
    }
    r->bound=true;r->busy=false;return true;
}
bool rtc_runtime_read_863(struct rtc_runtime_863 *r,struct rtc_time_854 *out)
{
    if(!r || !out || !r->bound || r->busy || r->quarantined ||
       r->owner.quarantined)return false;
    r->busy=true;
    struct rtc_time_854 t;
    bool ok=guard(r)&&rtc_owner_read_854(&r->owner,&t);
    bool live=guard(r);
    if(r->owner.held || r->owner.quarantined){
        r->quarantined=true;live=false;
    }
    r->busy=false;
    if(!ok||!live)return false;
    *out=t;return true;
}
