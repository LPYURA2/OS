#include <styleos/desktop_process_runtime.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct pr_table p;
static struct ws_service w;
static struct dpr_service d;
static struct pa_row rows[PA_MAX_ROWS];
static struct pa_reply reply;
int main(void){
 pr_init(&p);ws_init(&w);
 struct pa_request q={.version=PA_VERSION,.order=PI_BY_PID};
 memset(rows,0xa5,sizeof rows);memset(&reply,0xa5,sizeof reply);
 assert(dpr_supervisor_snapshot(&d,&q,rows,PA_MAX_ROWS,&reply)==PA_DENIED);
 assert(rows[0].pid==0xa5a5a5a5u&&reply.version==0xa5a5a5a5u);
 assert(dpr_start(&d,&p,&w,0x1000)==DPR_OK);
 assert(dpr_supervisor_snapshot(&d,&q,rows,PA_MAX_ROWS,&reply)==PA_OK&&reply.total==0);
 uint32_t a,b,c,seen;unsigned slot;
 assert(dpr_admit(&d,0,101,0x2000,0,"first",&a)==PS_OK);
 assert(dpr_admit(&d,1,102,0x3000,0,"second",&b)==PS_OK);
 assert(pr_account(&p,b,102,7,12288)==PR_OK);
 assert(dpr_dispatch(&d,&slot,&seen)==PS_OK&&slot==0&&seen==a);
 memset(rows,0xa5,sizeof rows);memset(&reply,0xa5,sizeof reply);
 assert(dpr_supervisor_snapshot(&d,&q,rows,PA_MAX_ROWS,&reply)==PA_DENIED);
 assert(rows[0].pid==0xa5a5a5a5u&&reply.version==0xa5a5a5a5u);
 assert(dpr_observe(&d,0,101,0x2000,0x1000,PS_EXIT)==PS_OK);
 assert(dpr_release(&d,0,101)==PS_OK);
 assert(dpr_admit(&d,0,103,0x4000,0,"third",&c)==PS_OK&&c>b);
 struct pi_snapshot_info info;struct pi_row raw[1];
 assert(pi_collect(&p,&w,raw,1,&info)==PI_TRUNCATED&&raw[0].pid==b&&info.total==2);
 assert(dpr_supervisor_snapshot(&d,&q,rows,1,&reply)==PA_TRUNCATED);
 assert(reply.total==2&&reply.returned==1&&rows[0].pid==b&&rows[0].cpu_ticks==7&&rows[0].resident_bytes==12288);
 assert(reply.measurement_flags==0);
 assert(dpr_supervisor_snapshot(&d,&q,rows,PA_MAX_ROWS,&reply)==PA_OK&&rows[1].pid==c);
 assert(dpr_snapshot(&d,c,false,&q,rows,PA_MAX_ROWS,&reply)==PA_DENIED);
 assert(dpr_snapshot(&d,c,true,&q,rows,PA_MAX_ROWS,&reply)==PA_OK);
 q.reserved=1;
 assert(dpr_supervisor_snapshot(&d,&q,rows,PA_MAX_ROWS,&reply)==PA_BAD_REQUEST);
 puts("PROCESS SUPERVISOR PASS: boundary, empty registry, live rows, recycled slot ordering, accounting, auth unchanged");
 return 0;
}
