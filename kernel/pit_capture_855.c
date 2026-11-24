#include <styleos/pit_capture_855.h>
static bool sample(const struct pit_capture_ops_855 *ops,void *ctx,uint64_t *v,uint64_t *last,bool *seen)
{
    if(!ops->counter(ctx,v)||(*seen&&*v<*last))return false;
    *last=*v;*seen=true;return true;
}
bool pit_capture_window_855(struct pit_capture_855 *state,const struct pit_capture_ops_855 *ops,void *ctx,const struct pit_capture_contract_855 *c,struct calibration_window_855 *out)
{
    if(!state||state->busy||state->quarantined||!ops||!out||!c||!c->terminal||!c->exclusive||!c->reference_attested||!ops->prepare||!ops->counter||!ops->gate_on||!ops->output||!ops->stop||!c->count||!c->ref_ticks_min||c->ref_ticks_min>c->count||c->ref_ticks_max<c->count||c->ref_ticks_max>65536u||!c->max_polls||c->max_polls>S855_CAPTURE_MAX_POLLS)return false;
    state->busy=true;
    struct calibration_window_855 w={.ref_ticks_min=c->ref_ticks_min,.ref_ticks_max=c->ref_ticks_max};
    bool ok=false,seen=false,high=false,low_seen=false;uint64_t last=0,low_before=0,before,after;
    /* Preparation may partially change hardware even when returning false. */
    if(!ops->prepare(ctx,c->count)||!ops->output(ctx,&high)||high||
       !sample(ops,ctx,&w.start_before,&last,&seen)||!ops->gate_on(ctx)||
       !sample(ops,ctx,&w.start_after,&last,&seen))goto cleanup;
    for(uint32_t i=0;i<c->max_polls;i++){
        if(!sample(ops,ctx,&before,&last,&seen)||!ops->output(ctx,&high)||!sample(ops,ctx,&after,&last,&seen))goto cleanup;
        if(!high){low_seen=true;low_before=before;continue;}
        /* Only a low observation after enabling the gate can anchor the end
         * bracket. Its BEFORE timestamp is conservative; AFTER could follow
         * the actual edge. The first high observation supplies the upper bound. */
        if(!low_seen||low_before<=w.start_after)goto cleanup;
        w.end_before=low_before;w.end_after=after;ok=true;break;
    }
cleanup:
    if(!ops->stop(ctx)){state->quarantined=true;ok=false;}
    state->busy=false;
    if(ok)*out=w;
    return ok;
}
