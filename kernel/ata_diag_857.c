#include <styleos/ata_diag_857.h>
enum { BSY=128, DRDY=64, DF=32, DRQ=8, ERR=1 };
bool ata857_allowed(unsigned reg,uint8_t v){
 switch(reg){case 1:return v==0;case 2:return v<=1;case 3:case 4:return true;case 5:return v==0;case 6:return v==0xa0||v==0xe0;case 7:return v==0xec||v==0x20;case 8:return v==2;default:return false;}
}
bool ata857_allowed_lba28(unsigned reg,uint8_t v){
 /* No slave, DMA, reset, or new commands. Platform opt-in only. */
 return ata857_allowed(reg,v)||reg==5||(reg==6&&v>=0xe0&&v<=0xef);
}
bool ata857_geometry_capacity(const uint16_t w[ATA857_WORDS],uint32_t expected){
 return expected&&expected<=ATA_LBA28_SECTORS&&w&&w[0]&&w[0]!=UINT16_MAX&&
        !(w[0]&0x8000u)&&(w[49]&0x200u)&&(w[106]&0xd000u)!=0x5000u&&
        ((uint32_t)w[60]|((uint32_t)w[61]<<16))==expected;
}
bool ata857_geometry(const uint16_t w[ATA857_WORDS]){
 return ata857_geometry_capacity(w,ATA857_SECTORS);
}
static bool status(const struct ata857_ops *o,void *c,bool data,bool (*pace)(void *)){
 for(unsigned n=0;n<ATA857_POLLS;n++){
  uint8_t s;if(!o->status(c,&s)||s==0||s==255)return false;
  if(s&BSY){if(pace&&n+1<ATA857_POLLS&&!pace(c))return false;continue;}
  if(s&(DF|ERR))return false;
  if(!!(s&DRQ)==data&&(s&DRDY))return true;
 }
 return false;
}
static bool delay(const struct ata857_ops *o,void *c){uint8_t s;for(unsigned i=0;i<4;i++)if(!o->status(c,&s)||!s||s==255)return false;return true;}
static bool transfer(const struct ata857_ops *o,void *c,bool identify,uint32_t lba,uint16_t *w,bool (*pace)(void *)){
 if(!status(o,c,false,0)||!o->write_reg(c,8,2)||!o->write_reg(c,6,identify?0xa0:(uint8_t)(0xe0u|(lba>>24)))||!delay(o,c)||!status(o,c,false,0))return false;
 if(!o->write_reg(c,1,0)||!o->write_reg(c,2,identify?0:1)||!o->write_reg(c,3,(uint8_t)lba)||!o->write_reg(c,4,(uint8_t)(lba>>8))||!o->write_reg(c,5,(uint8_t)(lba>>16))||!o->write_reg(c,7,identify?0xec:0x20)||!delay(o,c)||!status(o,c,true,pace))return false;
 for(unsigned i=0;i<ATA857_WORDS;i++)if(!o->read_data(c,&w[i]))return false;
 return delay(o,c)&&status(o,c,false,pace);
}
static bool run(struct ata857_state *s,const struct ata857_ops *o,void *c,bool identify,uint32_t expected,uint32_t lba,uint16_t *out,bool (*pace)(void *)){
 if(!s||!o||!o->status||!o->write_reg||!o->read_data||!out||s->busy||s->quarantined||
    s->identified==identify||!expected||expected>ATA_LBA28_SECTORS||
    (identify?s->sectors!=0:s->sectors!=expected)||lba>=expected)return false;
 uint16_t temp[ATA857_WORDS];s->busy=true;
 bool ok=transfer(o,c,identify,lba,temp,pace)&&(!identify||ata857_geometry_capacity(temp,expected));
 s->busy=false;if(!ok){s->quarantined=true;return false;}
 for(unsigned i=0;i<ATA857_WORDS;i++)out[i]=temp[i];
 if(identify){s->sectors=expected;s->identified=true;}
 return true;
}
bool ata857_identify_capacity(struct ata857_state *s,const struct ata857_ops *o,void *c,uint32_t expected,uint16_t out[ATA857_WORDS]){
 return run(s,o,c,true,expected,0,out,0);
}
bool ata857_identify(struct ata857_state *s,const struct ata857_ops *o,void *c,uint16_t out[ATA857_WORDS]){return ata857_identify_capacity(s,o,c,ATA857_SECTORS,out);}
bool ata857_read(struct ata857_state *s,const struct ata857_ops *o,void *c,uint32_t lba,uint16_t out[ATA857_WORDS]){return s&&run(s,o,c,false,s->sectors,lba,out,0);}
bool ata857_read_paced(struct ata857_state *s,const struct ata857_ops *o,void *c,uint32_t lba,uint16_t out[ATA857_WORDS],bool (*pace)(void *)){return s&&pace&&run(s,o,c,false,s->sectors,lba,out,pace);}
