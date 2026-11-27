#include <styleos/deep_drop.h>
static void wipe(void *p,size_t n){volatile uint8_t *b=p;while(n--)*b++=0;}
static bool eq(struct dd_endpoint a,struct dd_endpoint b){
 return a.pid==b.pid&&a.window==b.window&&a.generation==b.generation&&a.root==b.root;
}
bool dd_endpoint_valid(struct dd_service *s,struct dd_endpoint e){
 if(!s||!s->channel||!e.window)return false;
 enum nwc_status st=nwc_input_ready(s->channel,e.pid,e.generation,e.root,e.window);
 return st==NWC_OK||st==NWC_EMPTY;
}
/* Strict UTF-8: no NUL, overlong, surrogate, out-of-range, truncated sequence. */
static bool utf8(const uint8_t *p,size_t n){
 for(size_t i=0;i<n;){
  uint32_t v=p[i++],min=0;unsigned k=0;
  if(!v)return false;
  if(v<128)continue;
  if(v>=0xc2&&v<=0xdf){v&=31;k=1;min=128;}
  else if(v>=0xe0&&v<=0xef){v&=15;k=2;min=2048;}
  else if(v>=0xf0&&v<=0xf4){v&=7;k=3;min=65536;}
  else return false;
  if(k>n-i)return false;
  while(k--){uint8_t b=p[i++];if((b&0xc0)!=0x80)return false;v=(v<<6)|(b&63);}
  if(v<min||v>0x10ffff||(v>=0xd800&&v<=0xdfff))return false;
 }
 return true;
}
enum dd_status dd_init(struct dd_service *s,struct nwc_service *n){
 if(!s||!n||!n->ready)return DD_INVALID;
 if(s->channel||s->last_id||s->active.phase!=DD_IDLE)return DD_BUSY;
 s->channel=n;return DD_OK;
}
bool dd_reconcile(struct dd_service *s,uint64_t now){
 if(!s||!s->channel||s->active.phase==DD_IDLE)return false;
 if(now<s->active.deadline&&dd_endpoint_valid(s,s->active.source)&&dd_endpoint_valid(s,s->active.target))return false;
 wipe(&s->active,sizeof s->active);return true;
}
enum dd_status dd_offer(struct dd_service *s,struct dd_endpoint a,struct dd_endpoint b,
 const uint8_t *p,size_t n,uint64_t now,uint64_t deadline,bool gesture,uint64_t *id){
 if(!s||!s->channel||!p||!id||!n||n>DD_TEXT_MAX||deadline<=now)return DD_INVALID;
 if(!gesture||a.pid==b.pid||!dd_endpoint_valid(s,a)||!dd_endpoint_valid(s,b))return DD_DENIED;
 if(s->active.phase!=DD_IDLE)return DD_BUSY;
 if(s->last_id==UINT64_MAX)return DD_EXHAUSTED;
 if(!utf8(p,n))return DD_INVALID;
 /* All refusals precede these stores. Publish the slot last on this
  * serialized boundary, without a second 1 KiB payload on the stack. */
 struct dd_transfer *t=&s->active;
 t->id=++s->last_id;t->deadline=deadline;t->source=a;t->target=b;t->size=n;
 for(size_t i=0;i<n;++i)t->text[i]=p[i];
 t->phase=DD_OFFERED;*id=t->id;return DD_OK;
}
static enum dd_status target(struct dd_service *s,struct dd_endpoint e,uint64_t id,uint64_t now,enum dd_phase phase){
 if(!s||!s->channel||!id)return DD_INVALID;
 if(s->active.phase!=phase||s->active.id!=id)return DD_STALE;
 if(!eq(e,s->active.target)||!dd_endpoint_valid(s,e))return DD_DENIED;
 if(now>=s->active.deadline||!dd_endpoint_valid(s,s->active.source))return DD_STALE;
 return DD_OK;
}
enum dd_status dd_accept(struct dd_service *s,struct dd_endpoint e,uint64_t id,uint64_t now){
 enum dd_status r=target(s,e,id,now,DD_OFFERED);
 if(r==DD_OK)s->active.phase=DD_ACCEPTED;
 return r;
}
enum dd_status dd_deliver(struct dd_service *s,struct dd_endpoint e,uint64_t id,uint64_t now,
 uint8_t *out,size_t capacity,size_t *written){
 if(!out||!written)return DD_INVALID;
 enum dd_status r=target(s,e,id,now,DD_ACCEPTED);if(r!=DD_OK)return r;
 if(capacity<s->active.size)return DD_CAPACITY;
 for(size_t i=0;i<s->active.size;++i)out[i]=s->active.text[i];
 *written=s->active.size;wipe(s->active.text,sizeof s->active.text);s->active.phase=DD_DELIVERED;
 return DD_OK;
}
enum dd_status dd_finish(struct dd_service *s,struct dd_endpoint e,uint64_t id,uint64_t now){
 enum dd_status r=target(s,e,id,now,DD_DELIVERED);
 if(r==DD_OK)wipe(&s->active,sizeof s->active);
 return r;
}
enum dd_status dd_cancel(struct dd_service *s,struct dd_endpoint e,uint64_t id){
 if(!s||!s->channel||!id)return DD_INVALID;
 if(s->active.phase==DD_IDLE||s->active.id!=id)return DD_STALE;
 if((!eq(e,s->active.source)&&!eq(e,s->active.target))||!dd_endpoint_valid(s,e))return DD_DENIED;
 wipe(&s->active,sizeof s->active);return DD_OK;
}
