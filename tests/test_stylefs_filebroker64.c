#include <styleos/stylefs_filebroker64.h>
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
static enum wv64_status issue(uint64_t p,const char *path,uint64_t *token){
 return fbk64_issue(&broker,&m,&w,p,(const uint8_t *)path,strlen(path),token);
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
 uint64_t token=99,old=0,other_token=0;
 assert(issue(101,"/",&token)==WV64_INVALID&&token==99);
 assert(issue(101,"/missing",&token)==WV64_MISSING&&token==99);
 assert(issue(101,"/a",&old)==WV64_OK);
 assert(issue(102,"/a",&other_token)==WV64_OK&&old!=other_token);
 uint8_t buf[512];memset(buf,0xcc,sizeof buf);size_t done=99;
 uint64_t before=reads,wr_before=writes;
 assert(fbk64_read(&broker,&m,&w,102,old,0,buf,sizeof buf,&done)==WV64_LOCKED&&reads==before&&done==99&&buf[0]==0xcc);
 assert(fbk64_read(&broker,&m,&w,101,old+999,0,buf,sizeof buf,&done)==WV64_LOCKED&&reads==before);
 assert(fbk64_read(&broker,&m,&w,101,old,0,(uint8_t *)&broker,1,&done)==WV64_INVALID&&reads==before);
 assert(fbk64_read(&broker,&m,&w,101,old,511,buf,sizeof buf,&done)==WV64_OK&&done==512&&writes==wr_before);
 for(size_t i=0;i<done;i++)assert(buf[i]==(uint8_t)((511+i)^0x5a));
 assert(fbk64_read(&broker,&m,&w,101,old,1039,buf,sizeof buf,&done)==WV64_OK&&done==2);
 assert(fbk64_revoke(&broker,102,old)==WV64_LOCKED);
 assert(fbk64_revoke(&broker,101,old)==WV64_OK);
 assert(fbk64_read(&broker,&m,&w,101,old,0,buf,sizeof buf,&done)==WV64_LOCKED);
 assert(fbk64_read(&broker,&m,&w,102,other_token,0,buf,sizeof buf,&done)==WV64_OK);
 assert(fbk64_revoke_principal(&broker,102)==WV64_OK);
 assert(fbk64_read(&broker,&m,&w,102,other_token,0,buf,sizeof buf,&done)==WV64_LOCKED);
 assert(issue(101,"/a",&old)==WV64_OK);
 struct dc64_edit mkdir={.op=DC64_INSERT,.entry={.key={.parent=1,.length=1,.name={'d'}},.target=10,.kind=SD64_DIRECTORY}};
 assert(wv64_edit(&m.volume,&w,inv,&mkdir)==WV64_OK);
 before=reads;
 assert(fbk64_read(&broker,&m,&w,101,old,0,buf,sizeof buf,&done)==WV64_LOCKED&&reads==before);
 assert(issue(101,"/a",&old)==WV64_OK);
 assert(vm64_close(&m)==WV64_OK&&mount(false)==WV64_OK);
 before=reads;
 assert(fbk64_read(&broker,&m,&w,101,old,0,buf,sizeof buf,&done)==WV64_LOCKED&&reads==before);
 assert(issue(101,"/a",&token)==WV64_OK&&token!=old);
 for(unsigned i=1;i<FBK64_SLOTS;i++)assert(issue(101,"/a",&other_token)==WV64_OK);
 before=reads;assert(issue(101,"/a",&other_token)==WV64_FULL&&reads==before);
 assert(fbk64_revoke_principal(&broker,101)==WV64_OK);
 assert(issue(101,"/a",&other_token)==WV64_OK&&other_token!=token);
 assert(fbk64_read(&broker,&m,&w,101,token,0,buf,sizeof buf,&done)==WV64_LOCKED);
 assert(fbk64_read(&broker,&m,&w,101,other_token,0,buf,sizeof buf,&done)==WV64_OK);
 assert(vm64_close(&m)==WV64_OK&&grants()==1);
 puts("STYLEFS 41 FILE BROKER PASS: exact file read, principal/token isolation, revoke, epoch and remount expiry, bounded slots");
 return 0;
}
