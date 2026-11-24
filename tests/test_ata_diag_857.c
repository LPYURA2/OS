#include <styleos/ata_diag_857.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct model {uint16_t words[256];unsigned calls,fail,stage,idx,commands;uint8_t task[9];bool data,bad_geometry,stuck;};
static bool step(struct model*m){return ++m->calls!=m->fail;}
static bool status(void*c,uint8_t*v){struct model*m=c;if(!step(m))return false;*v=m->stuck?0x80:(m->data?0x58:0x50);return true;}
static bool write(void*c,unsigned r,uint8_t v){struct model*m=c;if(!step(m))return false;assert(ata857_allowed(r,v));const unsigned order[]={8,6,1,2,3,4,5,7};assert(m->stage<8&&r==order[m->stage++]);m->task[r]=v;if(r==7){assert(m->task[8]==2&&m->task[1]==0&&m->task[5]==0);assert(v==0xec?m->task[2]==0&&m->task[6]==0xa0:m->task[2]==1&&m->task[6]==0xe0);m->commands++;m->idx=0;m->data=true;m->stage=0;if(v==0xec){memset(m->words,0,sizeof m->words);m->words[0]=0x40;m->words[49]=0x200;m->words[60]=m->bad_geometry?4095:4096;}else{unsigned lba=m->task[3]+256u*m->task[4];for(unsigned i=0;i<256;i++)m->words[i]=(uint16_t)(lba*17u+i);}}return true;}
static bool data(void*c,uint16_t*v){struct model*m=c;if(!step(m))return false;assert(m->data&&m->idx<256);*v=m->words[m->idx++];if(m->idx==256)m->data=false;return true;}
static const struct ata857_ops ops={status,write,data};
static void initbuf(uint16_t*w){for(unsigned i=0;i<256;i++)w[i]=0xa5a5;}
static void untouched(const uint16_t*w){for(unsigned i=0;i<256;i++)assert(w[i]==0xa5a5);}
int main(void){
 struct ata857_state s={0};struct model m={0};uint16_t w[256];initbuf(w);assert(!ata857_read(&s,&ops,&m,0,w)&&m.calls==0);untouched(w);
 assert(ata857_identify(&s,&ops,&m,w)&&ata857_geometry(w));unsigned idcalls=m.calls;assert(!ata857_identify(&s,&ops,&m,w)&&m.calls==idcalls);
 unsigned pos=1,neg=2,readcalls=0;
 for(unsigned lba=0;lba<4096;lba++){unsigned before=m.calls;assert(ata857_read(&s,&ops,&m,lba,w));for(unsigned i=0;i<256;i++)assert(w[i]==(uint16_t)(lba*17u+i));readcalls=m.calls-before;pos++;}
 unsigned before=m.calls;initbuf(w);assert(!ata857_read(&s,&ops,&m,4096,w)&&m.calls==before);untouched(w);neg++;
 for(unsigned f=1;f<=idcalls;f++){s=(struct ata857_state){0};m=(struct model){.fail=f};initbuf(w);assert(!ata857_identify(&s,&ops,&m,w)&&s.quarantined&&!s.identified);untouched(w);unsigned n=m.calls;assert(!ata857_identify(&s,&ops,&m,w)&&m.calls==n);neg++;}
 for(unsigned f=1;f<=readcalls;f++){s=(struct ata857_state){0};m=(struct model){0};assert(ata857_identify(&s,&ops,&m,w));m.fail=m.calls+f;initbuf(w);assert(!ata857_read(&s,&ops,&m,4095,w)&&s.quarantined);untouched(w);unsigned n=m.calls;assert(!ata857_read(&s,&ops,&m,0,w)&&m.calls==n);neg++;}
 s=(struct ata857_state){0};m=(struct model){.bad_geometry=true};initbuf(w);assert(!ata857_identify(&s,&ops,&m,w)&&s.quarantined);untouched(w);neg++;
 s=(struct ata857_state){0};m=(struct model){.stuck=true};initbuf(w);assert(!ata857_identify(&s,&ops,&m,w)&&m.calls==ATA857_POLLS&&s.quarantined);untouched(w);neg++;
 unsigned allow=0,deny=0;for(unsigned r=0;r<16;r++)for(unsigned v=0;v<256;v++){bool expected=(r==1&&v==0)||(r==2&&v<=1)||r==3||r==4||(r==5&&v==0)||(r==6&&(v==0xa0||v==0xe0))||(r==7&&(v==0xec||v==0x20))||(r==8&&v==2);assert(ata857_allowed(r,(uint8_t)v)==expected);if(expected)allow++;else deny++;}
 printf("ATA fixture protocol: %u positive, %u refusal, %u+%u callback fault offsets; register policy %u allow / %u deny PASS\n",pos,neg,idcalls,readcalls,allow,deny);
}
