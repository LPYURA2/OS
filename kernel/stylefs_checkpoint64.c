#include <styleos/stylefs_checkpoint64.h>
#include "stylefs_recovery_internal.h"
static const uint8_t magic[8]={'S','F','6','4','C','P','0','1'};
struct layout {uint64_t identity,receipt,dir,data,ds,fs,start;};
struct receipt {uint64_t seq,epoch,gen;bool valid;};
#include "stylefs_checkpoint_common_internal.h"
static bool geometry(struct bs_service *b,const struct cp64_layout *l,bool write,struct layout *out){
 if(!l||l->dir_sectors<18||(l->dir_sectors-2)%16||l->data_start<SFS64_JOURNAL_SECTORS||
    l->data_start>=l->data_sectors)return false;
 const struct bs_grant *a=grant(b,l->owner,l->receipt,2,write),
 *d=grant(b,l->owner,l->dir,l->dir_sectors,write),*f=grant(b,l->owner,l->data,l->data_sectors,write);
 if(!a||!d||!f||a->device!=d->device||a->device!=f->device||a->generation!=d->generation||
    a->generation!=f->generation||overlap(a,d)||overlap(a,f)||overlap(d,f))return false;
 *out=(struct layout){b->devices[a->device].backend.identity,a->start,d->start,f->start,d->length,f->length,l->data_start};return true;
}
static void encode(uint8_t p[512],const struct layout *g,struct receipt x,unsigned slot){
 for(unsigned i=0;i<512;i++)p[i]=0;
 for(unsigned i=0;i<8;i++)p[i]=magic[i];
 put(p+8,x.seq,8);put(p+16,x.epoch,8);put(p+24,x.gen,8);
 put(p+32,g->identity,8);put(p+40,g->receipt,8);put(p+48,g->dir,8);put(p+56,g->data,8);
 put(p+64,g->ds,8);put(p+72,g->fs,8);put(p+80,g->start,8);put(p+88,slot,4);put(p+92,crc(p,92),4);
}
static struct receipt decode(const uint8_t p[512],const struct layout *g,unsigned slot){
 struct receipt x={0};
 if(!equal(p,magic,8)||!get(p+8,8)||!get(p+16,8)||!get(p+24,8)||
    get(p+32,8)!=g->identity||get(p+40,8)!=g->receipt||get(p+48,8)!=g->dir||get(p+56,8)!=g->data||
    get(p+64,8)!=g->ds||get(p+72,8)!=g->fs||get(p+80,8)!=g->start||get(p+88,4)!=slot||
    get(p+92,4)!=crc(p,92)||!zero(p+96,416))return x;
 return (struct receipt){get(p+8,8),get(p+16,8),get(p+24,8),true};
}
static enum cp64_status read_receipts(struct bs_service *b,const struct cp64_layout *l,const struct layout *g,
 struct receipt x[2],bool *blank){
 *blank=true;
 for(unsigned i=0;i<2;i++){
  uint8_t p[512];
  if(bs_read_sector(b,l->owner,l->receipt,i,p)!=BS_OK)return CP64_IO;
  if(!zero(p,512))*blank=false;
  x[i]=decode(p,g,i);
 }
 if(x[0].valid&&x[1].valid&&x[0].seq==x[1].seq)return CP64_CORRUPT;
 return CP64_OK;
}
enum cp64_status cp64_save(struct bs_service *b,const struct cp64_layout *l,struct fb64_view *v){
 struct layout g;
 if(!v||!v->valid||!geometry(b,l,true,&g)||!v->directory||!v->data||!v->proof||
    v->directory->block!=b||v->data->block!=b||v->dir_token!=l->dir||v->data_token!=l->data||
    v->directory->token!=l->dir||v->data->token!=l->data||v->data->data_start!=l->data_start||
    v->directory->owner!=l->owner||v->data->owner!=l->owner||
    !v->directory->valid||!v->data->valid||v->data->active>1||
    v->epoch!=v->directory->epoch||v->generation!=v->data->generation[v->data->active])return CP64_STALE;
 struct fb64_view check;
 if(fb64_bind(&check,v->directory,v->proof,v->data)!=FB64_OK)return CP64_CORRUPT;
 struct receipt x[2];bool blank;enum cp64_status rc=read_receipts(b,l,&g,x,&blank);
 if(rc!=CP64_OK)return rc;
 if(!x[0].valid&&!x[1].valid&&!blank)return CP64_CORRUPT;
 unsigned last=x[1].valid&&(!x[0].valid||x[1].seq>x[0].seq)?1:0;
 unsigned target=x[last].valid?last^1u:0;
 uint64_t seq=x[last].valid?x[last].seq:0;
 if(seq==UINT64_MAX)return CP64_FULL;
 if(seq){
  if(check.epoch<x[last].epoch||check.generation<x[last].gen)return CP64_STALE;
  if(check.epoch==x[last].epoch&&check.generation==x[last].gen)return CP64_OK;
  if(check.epoch-x[last].epoch>1||check.generation-x[last].gen>1)return CP64_STALE;
 }
 uint8_t p[512],q[512];encode(p,&g,(struct receipt){seq+1,check.epoch,check.generation,true},target);
 if(bs_write_sector(b,l->owner,l->receipt,target,p)!=BS_OK||
    bs_read_sector(b,l->owner,l->receipt,target,q)!=BS_OK||!equal(p,q,512)){
  v->valid=false;return CP64_UNCERTAIN;
 }return CP64_OK;
}
enum cp64_status cp64_recover(struct bs_service *b,const struct cp64_layout *l,
 struct dc64_state *d,struct sfs64_journal *j,struct ns64_proof *p,struct ns64_node *nodes,size_t slots,
 struct dc64_workspace *w,struct fb64_view *v,struct cp64_result *out){
 struct layout g;
 if(slots>SIZE_MAX/sizeof *nodes||!geometry(b,l,false,&g))return CP64_INVALID;
 const void *ptr[]={b,l,d,j,p,nodes,w,v,out};
 const size_t sizes[]={sizeof *b,sizeof *l,sizeof *d,sizeof *j,sizeof *p,slots*sizeof *nodes,sizeof *w,sizeof *v,sizeof *out};
 for(unsigned i=0;i<9;i++)for(unsigned k=0;k<i;k++)if(!sep(ptr[i],sizes[i],ptr[k],sizes[k]))return CP64_INVALID;
 v->valid=false;p->valid=false;d->valid=false;j->valid=false;
 struct receipt x[2];bool blank;enum cp64_status rc=read_receipts(b,l,&g,x,&blank);
 if(rc!=CP64_OK)return rc;
 unsigned first=x[1].valid&&(!x[0].valid||x[1].seq>x[0].seq)?1:0;
 for(unsigned i=0;i<2;i++){
  const struct receipt *a=&x[first^i];if(!a->valid)continue;
  enum dc64_status dr=dc64_mount_epoch(d,b,l->owner,l->dir,l->dir_sectors,a->epoch,w);
  if(dr==DC64_IO)return CP64_IO;
  if(dr==DC64_INVALID)return CP64_INVALID;
  if(dr!=DC64_OK)continue;
  enum sj_status jr=sfs64_journal_mount_generation(j,b,l->owner,l->data,l->data_sectors,l->data_start,a->gen);
  if(jr==SJ_IO)return CP64_IO;
  if(jr==SJ_INVALID||jr==SJ_STALE)return CP64_STALE;
  if(jr!=SJ_OK)continue;
  enum ns64_status nr=ns64_validate(d,w,nodes,slots,p);
  if(nr==NS64_IO)return CP64_IO;
  if(nr==NS64_FULL)return CP64_FULL;
  if(nr==NS64_INVALID||nr==NS64_STALE)return CP64_STALE;
  if(nr!=NS64_OK)continue;
  if(fb64_bind(v,d,p,j)!=FB64_OK)continue;
  *out=(struct cp64_result){a->seq,a->epoch,a->gen,i!=0};return CP64_OK;
 }
 v->valid=false;p->valid=false;d->valid=false;j->valid=false;return CP64_CORRUPT;
}

enum cp64_status cp64_current(struct bs_service *b,const struct cp64_layout *l,struct fb64_view *v,struct cp64_result *out){
 struct layout g;
 if(!v||!v->valid||!out||!geometry(b,l,true,&g)||!v->directory||!v->data||!v->proof||
    v->directory->block!=b||v->data->block!=b||v->dir_token!=l->dir||v->data_token!=l->data||
    v->directory->token!=l->dir||v->data->token!=l->data||v->data->data_start!=l->data_start||
    v->directory->owner!=l->owner||v->data->owner!=l->owner||
    !v->directory->valid||!v->data->valid||v->data->active>1||
    v->epoch!=v->directory->epoch||v->generation!=v->data->generation[v->data->active])return CP64_STALE;
 struct fb64_view check;
 if(!sep(out,sizeof *out,b,sizeof *b)||!sep(out,sizeof *out,l,sizeof *l)||
    !sep(out,sizeof *out,v,sizeof *v)||!sep(out,sizeof *out,v->directory,sizeof *v->directory)||
    !sep(out,sizeof *out,v->data,sizeof *v->data)||!sep(out,sizeof *out,v->proof,sizeof *v->proof)||
    fb64_bind(&check,v->directory,v->proof,v->data)!=FB64_OK||
    !sep(out,sizeof *out,check.proof->nodes,(size_t)check.proof->count*sizeof *check.proof->nodes))return CP64_INVALID;
 struct receipt x[2];bool blank;
 enum cp64_status rc=read_receipts(b,l,&g,x,&blank);
 if(rc!=CP64_OK)return rc;
 if(!x[0].valid&&!x[1].valid)return CP64_CORRUPT;
 unsigned last=x[1].valid&&(!x[0].valid||x[1].seq>x[0].seq)?1:0;
 if(check.epoch!=x[last].epoch||check.generation!=x[last].gen)return CP64_STALE;
 *out=(struct cp64_result){x[last].seq,check.epoch,check.generation,false};
 return CP64_OK;
}

static enum cp64_status dir_status(enum dc64_status rc){
 return rc==DC64_OK?CP64_OK:rc==DC64_IO?CP64_IO:rc==DC64_INVALID?CP64_INVALID:CP64_CORRUPT;
}
static enum cp64_status data_status(enum sj_status rc){
 return rc==SJ_OK?CP64_OK:rc==SJ_IO?CP64_IO:rc==SJ_STALE?CP64_STALE:CP64_CORRUPT;
}
static enum cp64_status validate_pair(struct dc64_state *d,struct sfs64_journal *j,
 struct ns64_proof *p,struct ns64_node *nodes,size_t slots,struct dc64_workspace *w,struct fb64_view *v){
 enum ns64_status nr=ns64_validate(d,w,nodes,slots,p);
 if(nr!=NS64_OK)return nr==NS64_IO?CP64_IO:nr==NS64_FULL?CP64_FULL:nr==NS64_STALE?CP64_STALE:CP64_CORRUPT;
 return fb64_bind(v,d,p,j)==FB64_OK?CP64_OK:CP64_CORRUPT;
}
static enum cp64_status erase_header(struct bs_service *b,uint64_t owner,uint64_t token,uint64_t sector){
 uint8_t clear[512]={0},check[512];
 if(bs_write_sector(b,owner,token,sector,clear)!=BS_OK||
    bs_read_sector(b,owner,token,sector,check)!=BS_OK||!zero(check,512))return CP64_UNCERTAIN;
 return CP64_OK;
}
static enum cp64_status reconcile_inner(struct bs_service *b,const struct cp64_layout *l,const struct layout *g,
 struct dc64_state *d,struct sfs64_journal *j,struct ns64_proof *p,struct ns64_node *nodes,size_t slots,
 struct dc64_workspace *w,struct fb64_view *v,struct cp64_result *out){
 struct receipt x[2]={0};
 for(unsigned i=0;i<2;i++){
  uint8_t raw[512];
  if(bs_read_sector(b,l->owner,l->receipt,i,raw)!=BS_OK)return CP64_IO;
  x[i]=decode(raw,g,i);
  /* Could be a damaged acknowledged receipt: never guess writable rollback. */
  if(!x[i].valid&&!zero(raw,512))return CP64_CORRUPT;
 }
 if(!x[0].valid&&!x[1].valid)return CP64_CORRUPT;
 unsigned last=x[1].valid&&(!x[0].valid||x[1].seq>x[0].seq)?1:0;
 struct receipt wanted=x[last],prior=x[last^1u];
 if(prior.valid&&(wanted.seq<=prior.seq||wanted.seq-prior.seq!=1||
    wanted.epoch<prior.epoch||wanted.epoch-prior.epoch>1||
    wanted.gen<prior.gen||wanted.gen-prior.gen>1))return CP64_CORRUPT;
 int dd=-1,jd=-1;
 enum cp64_status rc=dir_status(dc64_repair_select(d,b,l->owner,l->dir,l->dir_sectors,wanted.epoch,w,&dd));
 if(rc!=CP64_OK)return rc;
 rc=data_status(sfs64_journal_mount(j,b,l->owner,l->data,l->data_sectors,l->data_start));
 if(rc!=CP64_OK)return rc;
 uint64_t latest=j->generation[j->active];
 if(latest<wanted.gen||latest-wanted.gen>1)return CP64_CORRUPT;
 if(latest>wanted.gen){
  jd=(int)j->active;
  if(j->generation[j->active^1u]!=wanted.gen)return CP64_CORRUPT;
  j->active^=1u;
 }
 rc=validate_pair(d,j,p,nodes,slots,w,v);
 if(rc!=CP64_OK)return rc;
 /* Exact checkpoint is proven; invalidate only strictly newer headers.
  * Never touch retained data or checkpoint receipt sectors. */
 v->valid=false;p->valid=false;d->valid=false;j->valid=false;
 if(dd>=0){rc=erase_header(b,l->owner,l->dir,(uint64_t)dd);if(rc!=CP64_OK)return rc;}
 if(jd>=0){rc=erase_header(b,l->owner,l->data,1+(uint64_t)jd*(1+SFS64_BANK_SECTORS));if(rc!=CP64_OK)return rc;}
 rc=dir_status(dc64_mount(d,b,l->owner,l->dir,l->dir_sectors,w));if(rc!=CP64_OK)return rc;
 rc=data_status(sfs64_journal_mount(j,b,l->owner,l->data,l->data_sectors,l->data_start));if(rc!=CP64_OK)return rc;
 rc=validate_pair(d,j,p,nodes,slots,w,v);if(rc!=CP64_OK)return rc;
 struct cp64_result result;
 rc=cp64_current(b,l,v,&result);if(rc!=CP64_OK)return rc;
 if(result.sequence!=wanted.seq||result.epoch!=wanted.epoch||result.generation!=wanted.gen)return CP64_CORRUPT;
 *out=result;return CP64_OK;
}
enum cp64_status cp64_reconcile(struct bs_service *b,const struct cp64_layout *l,
 struct dc64_state *d,struct sfs64_journal *j,struct ns64_proof *p,struct ns64_node *nodes,size_t slots,
 struct dc64_workspace *w,struct fb64_view *v,struct cp64_result *out){
 struct layout g;
 if(!slots||slots>SIZE_MAX/sizeof *nodes||!geometry(b,l,true,&g))return CP64_INVALID;
 const void *ptr[]={b,l,d,j,p,nodes,w,v,out};
 const size_t sizes[]={sizeof *b,sizeof *l,sizeof *d,sizeof *j,sizeof *p,slots*sizeof *nodes,sizeof *w,sizeof *v,sizeof *out};
 for(unsigned i=0;i<9;i++)for(unsigned k=0;k<i;k++)if(!sep(ptr[i],sizes[i],ptr[k],sizes[k]))return CP64_INVALID;
 v->valid=false;p->valid=false;d->valid=false;j->valid=false;
 enum cp64_status rc=reconcile_inner(b,l,&g,d,j,p,nodes,slots,w,v,out);
 if(rc!=CP64_OK){v->valid=false;p->valid=false;d->valid=false;j->valid=false;}
 return rc;
}
