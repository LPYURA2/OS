#include <styleos/deep_drop.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct pr_table p;
static struct ws_service w;
static struct usr_runtime u;
static struct nwc_service n;
static struct dd_service d,old;
static unsigned checks;
#define OK(x) do{assert(x);++checks;}while(0)
#define NO(expr,status) do{old=d;assert((expr)==(status));assert(!memcmp(&old,&d,sizeof d));++checks;}while(0)
static struct dd_endpoint app(uint32_t sid,uint64_t gen){
 struct dd_endpoint e={.generation=gen,.root=gen*4096};
 struct pr_admission a={PR_NATIVE_USER,0,gen,e.root,0,"Deep Drop test"};
 OK(pr_admit(&p,&a,&e.pid)==PR_OK);
 OK(usr_bind(&u,sid,e.pid,gen)==USR_OK);
 OK(ws_open_app(&w,&p,e.pid,"App",(struct ws_bounds){0,0,400,300},&e.window)==WS_OK);
 OK(nwc_claim(&n,e.pid,gen,e.root,e.window)==NWC_OK);
 return e;
}
static void empty(void){struct dd_transfer z={0};OK(!memcmp(&d.active,&z,sizeof z));}
int main(void){
 pr_init(&p);ws_init(&w);OK(usr_init(&u,&p,&w)==USR_OK);OK(nwc_start(&n,&p,&u,&w)==NWC_OK);
 OK(dd_init(&d,&n)==DD_OK);NO(dd_init(&d,&n),DD_BUSY);
 uint32_t sid=0,other=0;OK(usr_create(&u,1,&sid)==USR_OK);OK(usr_create(&u,2,&other)==USR_OK);
 OK(usr_select(&u,sid)==USR_OK);
 struct dd_endpoint a=app(sid,10),b=app(sid,11),peer=app(sid,12),bad=b;bad.generation++;
 uint8_t text[]={0xd0,0xa7,0xd0,0xb0,0xd0,0xb9,10};const uint8_t original[]={0xd0,0xa7,0xd0,0xb0,0xd0,0xb9,10};
 uint8_t out[DD_TEXT_MAX];size_t len=123;uint64_t id=99;memset(out,0x5a,sizeof out);
 NO(dd_offer(&d,a,b,text,sizeof text,1,10,false,&id),DD_DENIED);OK(id==99);
 NO(dd_offer(&d,a,bad,text,sizeof text,1,10,true,&id),DD_DENIED);
 NO(dd_offer(&d,a,a,text,sizeof text,1,10,true,&id),DD_DENIED);
 NO(dd_offer(&d,a,b,text,DD_TEXT_MAX+1,1,10,true,&id),DD_INVALID);
 const uint8_t invalid[]={0xed,0xa0,0x80};
 NO(dd_offer(&d,a,b,invalid,sizeof invalid,1,10,true,&id),DD_INVALID);
 OK(dd_offer(&d,a,b,text,sizeof text,1,10,true,&id)==DD_OK&&id==1);
 memset(text,0,sizeof text); /* staging does not alias source */
 NO(dd_offer(&d,a,b,original,sizeof original,1,10,true,&id),DD_BUSY);
 NO(dd_deliver(&d,b,id,2,out,sizeof out,&len),DD_STALE);OK(len==123&&out[0]==0x5a);
 NO(dd_accept(&d,peer,id,2),DD_DENIED);NO(dd_accept(&d,bad,id,2),DD_DENIED);
 OK(ws_compositor_restart(&w)==2);OK(!dd_reconcile(&d,2));
 OK(dd_accept(&d,b,id,2)==DD_OK);NO(dd_accept(&d,b,id,2),DD_STALE);
 NO(dd_deliver(&d,b,id,2,out,1,&len),DD_CAPACITY);OK(len==123&&out[0]==0x5a);
 OK(dd_deliver(&d,b,id,2,out,sizeof out,&len)==DD_OK&&len==sizeof original&&!memcmp(out,original,len)&&out[len]==0x5a);
 for(unsigned i=0;i<DD_TEXT_MAX;++i)assert(d.active.text[i]==0);
 NO(dd_deliver(&d,b,id,2,out,sizeof out,&len),DD_STALE);
 OK(dd_finish(&d,b,id,2)==DD_OK);empty();NO(dd_finish(&d,b,id,2),DD_STALE);
 for(unsigned i=0;i<100;++i){
  OK(dd_offer(&d,a,b,original,sizeof original,3,10,true,&id)==DD_OK&&id==i+2);
  NO(dd_cancel(&d,peer,id),DD_DENIED);OK(dd_cancel(&d,a,id)==DD_OK);empty();
 }
 OK(dd_offer(&d,a,b,original,sizeof original,3,10,true,&id)==DD_OK);
 NO(dd_accept(&d,b,id,10),DD_STALE);OK(dd_reconcile(&d,10));empty();OK(!dd_reconcile(&d,10));
 OK(dd_offer(&d,a,b,original,sizeof original,11,20,true,&id)==DD_OK);
 OK(usr_select(&u,other)==USR_OK);struct dd_endpoint foreign=app(other,13);
 NO(dd_accept(&d,foreign,id,12),DD_DENIED);OK(dd_reconcile(&d,12));empty();
 OK(usr_select(&u,sid)==USR_OK);
 NO(dd_offer(&d,a,foreign,original,sizeof original,12,20,true,&id),DD_DENIED);
 OK(dd_offer(&d,a,b,original,sizeof original,12,20,true,&id)==DD_OK);
 OK(pr_transition(&p,a.pid,a.generation,PR_EXITED)==PR_OK);
 OK(dd_reconcile(&d,13));empty();OK(pr_is_live(&p,b.pid)&&pr_is_live(&p,peer.pid));
 struct dd_endpoint fresh=app(sid,14);
 OK(dd_offer(&d,fresh,b,original,sizeof original,13,20,true,&id)==DD_OK);
 OK(dd_accept(&d,b,id,14)==DD_OK);OK(pr_transition(&p,b.pid,b.generation,PR_EXITED)==PR_OK);
 OK(dd_reconcile(&d,15));empty();OK(pr_is_live(&p,fresh.pid));
 d.last_id=UINT64_MAX;NO(dd_offer(&d,fresh,peer,original,sizeof original,15,20,true,&id),DD_EXHAUSTED);
 printf("Deep Drop 71 HOST ONLY: %u checks PASS, fixed payload %u bytes, service %zu bytes\n",checks,DD_TEXT_MAX,sizeof d);
 return 0;
}
