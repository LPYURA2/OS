#include <styleos/runtime_bank_862.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct rt862_bank bank;
static struct dpr_service d;
static struct pr_table pr;
static struct ws_service ws;
static struct s846_root child;
static struct s844_lease lease={.pa=0x100000,.generation=17,.owner=S846_OWNER_A,.pages=3};
static struct nte_loaded image={.entry=S846_CODE+32,.code_bytes=64,.memory_bytes=4096};
static struct up_context initial={.pc=S846_CODE+32,.sp=S846_STACK+4096,.status=2,.cs=0x5b,.ss=0x63};
static unsigned checks;
static uint32_t pid=0xdeadbeef;
#define OK(x) do{assert(x);++checks;}while(0)
#define REJECT(x) do{struct rt862_bank b=bank;struct dpr_service s=d;struct pr_table p=pr;struct ws_service w=ws;uint32_t old=pid;OK((x)!=PS_OK);OK(!memcmp(&b,&bank,sizeof b)&&!memcmp(&s,&d,sizeof s)&&!memcmp(&p,&pr,sizeof p)&&!memcmp(&w,&ws,sizeof w)&&old==pid);}while(0)
#define ADMIT() rt865_bank_admit_image(&bank,&d,0x1000,0,&child,&lease,&image,"loader image",&initial,&pid)
int main(void){
 pr_init(&pr);ws_init(&ws);OK(dpr_start(&d,&pr,&ws,0x1000)==DPR_OK);
 OK(rt862_bank_open(&bank,&d,0x1000,0x5b,0x63));
 child.ready=true;child.built=(struct pg_result){.root=0x2000,.used=1,.error=PG_OK};
 child.plan=(struct pg_plan){.arch=PG_X86,.tables_pa=0x2000,.capacity=PG_TABLE_PAGES,.count=3,
 .maps={{S846_CODE,0x100000,4096,PG_EXEC},{S846_DATA,0x101000,4096,PG_WRITE},{S846_STACK,0x102000,4096,PG_WRITE}}};
 child.lease=lease;initial.gpr[0]=0xabcdef;initial.gpr[14]=0x123456;
 lease.generation++;REJECT(ADMIT());lease.generation--;
 lease.owner++;REJECT(ADMIT());lease.owner--;
 child.plan.maps[0].flags=PG_WRITE|PG_EXEC;REJECT(ADMIT());child.plan.maps[0].flags=PG_EXEC;
 child.plan.maps[1].pa+=4096;REJECT(ADMIT());child.plan.maps[1].pa-=4096;
 child.built.root=0x1000;REJECT(ADMIT());child.built.root=0x2000;
 image.code_bytes=32;REJECT(ADMIT());image.code_bytes=64;
 image.memory_bytes=4097;REJECT(ADMIT());image.memory_bytes=4096;
 initial.pc++;REJECT(ADMIT());initial.pc--;
 initial.sp--;REJECT(ADMIT());initial.sp++;
 OK(ADMIT()==PS_OK&&pid==1);
 OK(bank.slots[0].handle==lease.generation&&bank.slots[0].root==child.built.root);
 unsigned slot;uint32_t chosen;OK(dpr_dispatch(&d,&slot,&chosen)==PS_OK&&slot==0&&chosen==pid);
 struct rt862_selection t={slot,d.dispatcher.dispatches,pid,lease.generation,child.built.root,0x1000};
 struct up_context c={0};OK(rt862_bank_enter(&bank,&d,&t,0x1000,&c));
 OK(c.pc==image.entry&&c.gpr[0]==initial.gpr[0]&&c.gpr[14]==initial.gpr[14]&&c.status==0x202);
 printf("S865 image admission HOST ONLY: %u checks PASS\n",checks);
}
