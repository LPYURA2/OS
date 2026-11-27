#include <styleos/stylefs_process_file64.h>
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

 struct pr_table table;pr_init(&table);
 struct pf64_service service={&table,&broker};
 struct pr_admission spec={PR_NATIVE_USER,0,11,0x2000,4096,"editor"};
 uint32_t pid=0,other_pid=0,kernel_pid=0,new_pid=0;
 assert(pr_admit(&table,&spec,&pid)==PR_OK);
 spec.scheduler_handle=12;spec.page_root=0x3000;spec.label="notes";
 assert(pr_admit(&table,&spec,&other_pid)==PR_OK);
 spec.type=PR_KERNEL_TASK;spec.scheduler_handle=13;spec.page_root=0;spec.label="service";
 assert(pr_admit(&table,&spec,&kernel_pid)==PR_OK);
 struct pf64_identity a_id={pid,11,0x2000},b_id={other_pid,12,0x3000},bad={pid,12,0x2000},kernel={kernel_pid,13,0};
 uint64_t token=0,second=0,output=99;
 uint64_t before=reads;
 assert(pf64_issue(&service,&m,&w,&bad,(const uint8_t *)"/a",2,&output)==WV64_LOCKED&&output==99&&reads==before);
 assert(pf64_issue(&service,&m,&w,&kernel,(const uint8_t *)"/a",2,&output)==WV64_LOCKED&&reads==before);
 assert(pf64_issue(&service,&m,&w,&a_id,(const uint8_t *)"/a",2,&token)==WV64_OK);
 assert(pf64_issue(&service,&m,&w,&b_id,(const uint8_t *)"/a",2,&second)==WV64_OK);
 uint8_t buf[512];size_t done=99;
 before=reads;
 assert(pf64_read(&service,&m,&w,&a_id,token,0,buf,sizeof buf,&done)==WV64_LOCKED&&reads==before&&done==99);
 assert(pr_transition(&table,pid,11,PR_RUNNING)==PR_OK);
 assert(pr_transition(&table,other_pid,12,PR_RUNNING)==PR_OK);
 assert(pf64_read(&service,&m,&w,&b_id,token,0,buf,sizeof buf,&done)==WV64_LOCKED&&reads==before);
 bad=a_id;bad.root=0x3000;
 assert(pf64_read(&service,&m,&w,&bad,token,0,buf,sizeof buf,&done)==WV64_LOCKED&&reads==before);
 assert(pf64_read(&service,&m,&w,&a_id,token,0,(uint8_t *)&table,1,&done)==WV64_INVALID&&reads==before);
 assert(pf64_read(&service,&m,&w,&a_id,token,511,buf,sizeof buf,&done)==WV64_OK&&done==512);
 for(size_t i=0;i<done;i++)assert(buf[i]==(uint8_t)((511+i)^0x5a));
 assert(pr_transition(&table,pid,11,PR_FAULTED)==PR_OK);
 before=reads;
 assert(pf64_read(&service,&m,&w,&a_id,token,0,buf,sizeof buf,&done)==WV64_LOCKED&&reads==before);
 assert(pf64_sweep(&service)==WV64_OK&&reads==before);
 assert(fbk64_revoke(&broker,pid,token)==WV64_LOCKED);
 assert(pr_reap(&table,pid,11)==PR_OK);
 spec=(struct pr_admission){PR_NATIVE_USER,0,11,0x2000,4096,"editor-restarted"};
 assert(pr_admit(&table,&spec,&new_pid)==PR_OK&&new_pid!=pid);
 struct pf64_identity fresh={new_pid,11,0x2000};
 assert(pr_transition(&table,new_pid,11,PR_RUNNING)==PR_OK);
 assert(pf64_read(&service,&m,&w,&fresh,token,0,buf,sizeof buf,&done)==WV64_LOCKED);
 assert(pf64_read(&service,&m,&w,&b_id,second,0,buf,sizeof buf,&done)==WV64_OK);
 assert(pr_transition(&table,other_pid,12,PR_EXITED)==PR_OK);
 assert(pr_reap(&table,other_pid,12)==PR_OK);
 assert(pf64_sweep(&service)==WV64_OK);
 assert(fbk64_revoke(&broker,other_pid,second)==WV64_LOCKED);
 assert(pf64_issue(&service,&m,&w,&fresh,(const uint8_t *)"/a",2,&output)==WV64_OK&&output!=token);
 assert(pf64_read(&service,&m,&w,&fresh,output,0,buf,sizeof buf,&done)==WV64_OK);

 uint64_t save_token=0,observer=0;
 const uint8_t replacement[]={0x41,0,0x42,0xff};
 uint64_t wbefore=writes;
 assert(pf64_save(&service,&m,&w,inv,&fresh,output,replacement,sizeof replacement)==WV64_LOCKED&&writes==wbefore);
 assert(pf64_issue_save(&service,&m,&w,&fresh,(const uint8_t *)"/a",2,&save_token)==WV64_OK);
 assert(pf64_issue(&service,&m,&w,&fresh,(const uint8_t *)"/a",2,&observer)==WV64_OK);
 struct pf64_identity wrong=fresh;wrong.root=0x3000;
 assert(pf64_save(&service,&m,&w,inv,&wrong,save_token,replacement,sizeof replacement)==WV64_LOCKED&&writes==wbefore);
 assert(pf64_save(&service,&m,&w,inv,&fresh,save_token,(const uint8_t *)&table,1)==WV64_INVALID&&writes==wbefore);
 assert(pf64_save(&service,&m,&w,inv,&fresh,save_token,replacement,SFS64_WINDOW_BYTES+1)==WV64_INVALID&&writes==wbefore);
 assert(pf64_save(&service,&m,&w,inv,&fresh,save_token,replacement,sizeof replacement)==WV64_OK);
 assert(pf64_save(&service,&m,&w,inv,&fresh,save_token,replacement,sizeof replacement)==WV64_LOCKED);
 assert(pf64_read(&service,&m,&w,&fresh,observer,0,buf,sizeof buf,&done)==WV64_LOCKED);
 assert(pf64_issue(&service,&m,&w,&fresh,(const uint8_t *)"/a",2,&observer)==WV64_OK);
 assert(pf64_read(&service,&m,&w,&fresh,observer,0,buf,sizeof buf,&done)==WV64_OK&&done==sizeof replacement&&!memcmp(buf,replacement,done));
 assert(vm64_close(&m)==WV64_OK&&mount(false)==WV64_OK);
 assert(pf64_issue_save(&service,&m,&w,&fresh,(const uint8_t *)"/a",2,&save_token)==WV64_LOCKED);
 assert(pf64_issue(&service,&m,&w,&fresh,(const uint8_t *)"/a",2,&observer)==WV64_OK);
 assert(pf64_read(&service,&m,&w,&fresh,observer,0,buf,sizeof buf,&done)==WV64_OK&&done==sizeof replacement&&!memcmp(buf,replacement,done));
 assert(vm64_close(&m)==WV64_OK&&mount(true)==WV64_OK);
 assert(pf64_issue_save(&service,&m,&w,&fresh,(const uint8_t *)"/a",2,&save_token)==WV64_OK);
 assert(pf64_save(&service,&m,&w,inv,&fresh,save_token,0,0)==WV64_OK);
 assert(pf64_issue(&service,&m,&w,&fresh,(const uint8_t *)"/a",2,&observer)==WV64_OK);
 assert(pf64_read(&service,&m,&w,&fresh,observer,0,buf,sizeof buf,&done)==WV64_OK&&done==0);
 puts("STYLEFS 47 SCOPED SAVE PASS: explicit grant, read-only denial, authenticated identity, stale tokens, cold reopen, empty replacement");
 assert(vm64_close(&m)==WV64_OK&&grants()==1);
 puts("STYLEFS 42 PROCESS FILE PASS: scheduler identity, wrong root/handle, fault/exit sweep, PID nonreuse and independent readers");
 return 0;
}
