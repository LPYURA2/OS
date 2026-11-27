#include <styleos/stylefs_file_syscall43.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { DIR=258,INO=258,DATA=2048,TOTAL=2+DIR+INO+DATA+1 };
static uint8_t disk[TOTAL][512];
static struct bs_service b;
static struct vm64_mount m;
static struct wv64_workspace w;
static struct ns64_node nodes[100];
static struct ow64_extent entries[2][100];
static struct sfs64_run runs[2][100];
static struct ow64_inventory inv[2]={{entries[0],runs[0],100,0},{entries[1],runs[1],100,0}};
static uint64_t writes,reads,gen;
static bool rd(void *ctx,uint64_t a,uint8_t *out){(void)ctx;++reads;if(a>=TOTAL)return false;memcpy(out,disk[a],512);return true;}
static bool wr(void *ctx,uint64_t a,const uint8_t *in){(void)ctx;++writes;if(a>=TOTAL)return false;memcpy(disk[a],in,512);return true;}
static bool fl(void *ctx){(void)ctx;return true;}
static struct vm64_geometry g;
static enum wv64_status mount(bool write){return vm64_open(&m,&b,&g,write,nodes,100,&w,inv);}
static unsigned grants(void){unsigned n=0;for(unsigned i=0;i<BS_GRANTS;i++)if(b.grants[i].token)++n;return n;}
static struct fbk64_broker broker;
static bool source(void *ctx,uint64_t off,uint8_t *out,size_t n){
 (void)ctx;for(size_t i=0;i<n;i++)out[i]=(uint8_t)((off+i)^0x5a);
 return true;
}

static struct pr_table table;
static struct ws_service windows;
static struct dpr_service d;
static struct rt862_bank bank;
static _Alignas(4096) uint8_t pages[2][4096];
static unsigned audit_calls,fail_audit;
static bool audit(void *ctx,const struct rt862_selection *t,const uint8_t *page){
 (void)ctx;++audit_calls;
 return audit_calls!=fail_audit&&t->slot<2&&page==pages[t->slot];
}
static struct rt862_selection select_slot(unsigned expected){
 unsigned slot;uint32_t pid;
 assert(dpr_dispatch(&d,&slot,&pid)==PS_OK&&slot==expected);
 return (struct rt862_selection){slot,d.dispatcher.dispatches,pid,d.dispatcher.slots[slot].handle,d.dispatcher.slots[slot].user_root,d.dispatcher.safe_root};
}
static struct s847_raw frame(const struct rt862_selection *t,uint64_t token,uint64_t off,uint64_t dest,uint64_t n){
 struct up_context u={0};assert(rt862_bank_enter(&bank,&d,t,0x1000,&u));
 struct s847_raw f={.vector=128,.rip=u.pc,.rsp=u.sp,.cs=u.cs,.ss=u.ss,.rflags=u.status};
 f.gpr[0]=FS43_CALL_READ;f.gpr[1]=token;f.gpr[2]=off;f.gpr[3]=dest;f.gpr[4]=n;return f;
}
int main(void){
 struct bs_backend be={0,1,TOTAL,true,false,rd,wr,fl};
 assert(bs_attach(&b,&be,&gen)==BS_OK);
 g=(struct vm64_geometry){gen,7,0,2,2+DIR,2+DIR+INO,DIR,INO,DATA,32};
 uint64_t other;
 assert(bs_grant_range(&b,gen,9,TOTAL-1,1,BS_READ|BS_WRITE,&other)==BS_OK);
 /* Failed blank mount must revoke all acquired grants and write nothing. */
 assert(mount(true)!=WV64_OK&&!m.active&&grants()==1&&writes==0);
 struct wv64_layout l={.owner=7,.dir_sectors=DIR,.inode_sectors=INO,.data_sectors=DATA,.data_start=32};
 assert(bs_grant_range(&b,gen,7,0,2,BS_READ|BS_WRITE,&l.receipt)==BS_OK);
 assert(bs_grant_range(&b,gen,7,2,DIR,BS_READ|BS_WRITE,&l.dir)==BS_OK);
 assert(bs_grant_range(&b,gen,7,2+DIR,INO,BS_READ|BS_WRITE,&l.inode)==BS_OK);
 assert(bs_grant_range(&b,gen,7,2+DIR+INO,DATA,BS_READ|BS_WRITE,&l.data)==BS_OK);
 assert(dc64_provision(&b,7,l.dir,DIR)==DC64_OK);
 assert(ic64_provision(&b,7,l.inode,INO,DATA,32)==IC64_OK);
 assert(wv64_provision_checkpoint(&b,&l,&w)==WV64_OK);
 assert(bs_revoke_owner(&b,7)==BS_OK&&grants()==1);

 assert(mount(true)==WV64_OK);
 struct sd64_record a={.key={.parent=1,.length=1,.name={'a'}},.target=2,.kind=SD64_FILE};
 assert(wv64_create(&m.volume,&w,inv,&a,1041,source,0)==WV64_OK);


 pr_init(&table);ws_init(&windows);assert(dpr_start(&d,&table,&windows,0x1000)==DPR_OK);
 uint32_t pid[2];assert(dpr_admit(&d,0,11,0x2000,0,"A",&pid[0])==PS_OK);
 assert(dpr_admit(&d,1,12,0x3000,0,"B",&pid[1])==PS_OK);
 assert(rt862_bank_init(&bank,&d,0x1000,0x5b,0x63));
 struct pf64_service service={&table,&broker};
 struct pf64_identity ident={pid[0],11,0x2000};uint64_t token;
 assert(pf64_issue(&service,&m,&w,&ident,(const uint8_t *)"/a",2,&token)==WV64_OK);
 memset(pages,0xcc,sizeof pages);
 const uint64_t va=S846_IST+4096-16-sizeof(struct s847_raw);
 struct fs43_binding bind={&service,&m,&w,pages[0],audit,0};
 struct rt862_selection ticket=select_slot(0);
 struct s847_raw raw=frame(&ticket,token,511,S846_DATA+4064,32);
 uint64_t before=reads,wr_before=writes;
 struct rt862_selection stale=ticket;stale.dispatch--;
 assert(rt43_file_read(&bank,&d,&stale,0x1000,2,&raw,va,0,&bind)==PS_EVIDENCE&&reads==before);
 assert(rt43_file_read(&bank,&d,&ticket,0x1000,2,&raw,va-8,0,&bind)==PS_EVIDENCE&&reads==before);
 bind.page=pages[1];
 assert(rt43_file_read(&bank,&d,&ticket,0x1000,2,&raw,va,0,&bind)==PS_EVIDENCE&&reads==before);
 bind.page=pages[0];fail_audit=audit_calls+2;
 assert(rt43_file_read(&bank,&d,&ticket,0x1000,2,&raw,va,0,&bind)==PS_EVIDENCE&&pages[0][4064]==0xcc&&d.dispatcher.current==0);
 fail_audit=0;
 assert(rt43_file_read(&bank,&d,&ticket,0x1000,2,&raw,va,0,&bind)==PS_OK);
 assert(bank.slots[0].context.gpr[0]==32&&d.dispatcher.current==-1);
 for(size_t i=0;i<32;i++)assert(pages[0][4064+i]==(uint8_t)((511+i)^0x5a));
 assert(pages[0][4063]==0xcc&&pages[1][4064]==0xcc);
 ticket=select_slot(1);bind.page=pages[1];raw=frame(&ticket,token,0,S846_DATA,16);
 before=reads;
 assert(rt43_file_read(&bank,&d,&ticket,0x1000,2,&raw,va,0,&bind)==PS_OK&&reads==before);
 assert(bank.slots[1].context.gpr[0]==UINT64_C(0)-WV64_LOCKED&&pages[1][0]==0xcc);
 ticket=select_slot(0);bind.page=pages[0];raw=frame(&ticket,token,0,S846_DATA+4095,2);
 assert(rt43_file_read(&bank,&d,&ticket,0x1000,2,&raw,va,0,&bind)==PS_OK&&reads==before);
 assert(bank.slots[0].context.gpr[0]==UINT64_C(0)-WV64_INVALID);
 ticket=select_slot(1);bind.page=pages[1];raw=frame(&ticket,token,0,UINT64_MAX,1);
 assert(rt43_file_read(&bank,&d,&ticket,0x1000,2,&raw,va,0,&bind)==PS_OK&&reads==before);
 ticket=select_slot(0);bind.page=pages[0];raw=frame(&ticket,token,1039,S846_DATA,32);
 assert(rt43_file_read(&bank,&d,&ticket,0x1000,2,&raw,va,0,&bind)==PS_OK);
 assert(bank.slots[0].context.gpr[0]==2&&pages[0][0]==(uint8_t)(1039^0x5a)&&pages[0][1]==(uint8_t)(1040^0x5a)&&pages[0][2]==0xcc);
 assert(writes==wr_before&&vm64_close(&m)==WV64_OK&&grants()==1);
 puts("STYLEFS 43 FILE SYSCALL PASS: safe-root ticket, exact page, 32-byte copy, foreign token, range and EOF");
 return 0;
}
