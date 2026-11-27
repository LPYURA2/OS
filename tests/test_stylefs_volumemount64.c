#include <styleos/stylefs_volumemount64.h>
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
 uint64_t before=writes,read_before=reads;
 uint64_t save=g.data_start;g.data_start=g.dir_start;
 assert(mount(true)==WV64_INVALID&&writes==before&&reads==read_before&&grants()==1);
 g.data_start=save;
 /* Denial after first grant must roll back exactly that grant. */
 uint64_t blocked;
 assert(bs_grant_range(&b,gen,8,g.dir_start,DIR,BS_READ,&blocked)==BS_OK);
 assert(mount(true)!=WV64_OK&&grants()==2&&!m.active&&writes==before);
 assert(bs_revoke(&b,8,blocked)==BS_OK);
 assert(mount(true)==WV64_OK&&grants()==5&&m.volume.sequence==1);
 assert(mount(true)==WV64_LOCKED&&grants()==5);
 uint64_t old=m.grants.data;
 m.volume.busy=true;assert(vm64_close(&m)==WV64_LOCKED&&grants()==5);m.volume.busy=false;
 assert(vm64_close(&m)==WV64_OK&&grants()==1&&!m.active);
 uint8_t buf[512];
 assert(bs_read_sector(&b,7,old,0,buf)==BS_STALE);
 assert(bs_read_sector(&b,9,other,0,buf)==BS_OK);
 assert(mount(false)==WV64_OK&&grants()==5&&!m.volume.write);
 assert(m.grants.data!=old);
 assert(bs_write_sector(&b,7,m.grants.data,0,buf)==BS_DENIED);
 assert(vm64_close(&m)==WV64_OK&&grants()==1&&writes==before);
 /* Unplug invalidates all device tokens; close is still safe and local. */
 assert(mount(true)==WV64_OK);
 assert(bs_detach(&b,gen)==BS_OK);
 assert(vm64_close(&m)==WV64_OK&&!m.active&&grants()==0);
 assert(mount(true)==WV64_INVALID);
 puts("STYLEFS 40 MOUNT LIFETIME PASS: grant rollback, exact revoke, stale token, RO refusal, busy and detach");
 return 0;
}
