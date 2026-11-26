#include <styleos/ata_runtime_863.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fixture {struct ata_runtime_863 a;unsigned cmd,commands,index,busy,pauses,words;bool permanent,pace_fail;uint16_t id[256];};
static bool status(void *ctx,uint8_t *out){
 struct fixture*f=ctx;
 if(f->cmd==0x20&&f->index==0&&(f->permanent||f->busy)){
  if(f->busy)--f->busy;
  *out=0xd0;return true;
 }
 *out=(f->cmd&&f->index<256)?0x58:0x50;return true;
}
static bool reg(void *ctx,unsigned r,uint8_t v){
 struct fixture*f=ctx;assert(ata857_allowed(r,v));
 if(r==7){f->cmd=v;f->index=0;++f->commands;}return true;
}
static bool rd(void *ctx,uint16_t *out){
 struct fixture*f=ctx;assert(f->index<256);
 *out=f->cmd==0xec?f->id[f->index]:(uint16_t)(0x1000+f->index);
 if(f->cmd==0x20)++f->words;
 ++f->index;return true;
}
static bool wr(void *ctx,uint16_t v){(void)ctx;(void)v;assert(0);return false;}
static bool pace(void *ctx){
 struct fixture*f=ctx;assert(f->a.busy&&f->a.phase==ATA863_READ&&f->cmd==0x20&&f->index==0);
 ++f->pauses;return !f->pace_fail;
}
static const struct ata_runtime_ops_863 ops={{status,reg,rd},wr,0};
static void init(struct fixture*f){
 memset(f,0,sizeof *f);f->id[0]=0x40;f->id[49]=0x200;f->id[60]=4096;f->id[83]=0x5000;
 assert(ata_runtime_bind_863(&f->a,&ops,f));
}
static void unchanged(uint8_t*out){for(unsigned i=0;i<512;i++)assert(out[i]==0xa5);}
int main(void){
 struct fixture f;uint8_t out[512];
 init(&f);f.busy=4+ATA857_POLLS-1;
 assert(ata_runtime_read_paced_863(&f.a,4095,out,pace));
 assert(f.commands==2&&f.words==256&&f.pauses==ATA857_POLLS-1);
 for(unsigned i=0;i<256;i++)assert(out[2*i]==(uint8_t)i&&out[2*i+1]==0x10);
 init(&f);f.busy=4+ATA857_POLLS;memset(out,0xa5,sizeof out);
 assert(!ata_runtime_read_paced_863(&f.a,4095,out,pace));
 assert(f.commands==2&&f.words==0&&f.pauses==ATA857_POLLS-1&&f.a.quarantined);unchanged(out);
 assert(!ata_runtime_read_paced_863(&f.a,0,out,pace)&&f.commands==2);
 init(&f);f.permanent=true;f.pace_fail=true;memset(out,0xa5,sizeof out);
 assert(!ata_runtime_read_paced_863(&f.a,4095,out,pace)&&f.pauses==1&&f.commands==2&&f.words==0&&f.a.quarantined);unchanged(out);
 init(&f);f.busy=4+ATA857_POLLS;memset(out,0xa5,sizeof out);
 assert(!ata_runtime_read_863(&f.a,4095,out)&&!f.pauses&&f.commands==2&&f.a.quarantined);unchanged(out);
 init(&f);assert(ata_runtime_read_paced_863(&f.a,0,out,pace)&&!f.pauses&&f.words==256&&f.commands==2);
 puts("ATA read pacing: last allowed poll, timeout, pacing refusal, legacy and ready PASS; no retries");
}
