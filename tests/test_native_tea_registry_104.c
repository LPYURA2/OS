#include <styleos/native_tea_registry_104.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct ntr104_registry registry;
static uint8_t pkg[2][NTP105_MAX_PACKAGE],elf[2][NTP105_MAX_ELF],manifest[2][4096],sha[2][32],eh[2][32];
static size_t readfile(const char *path,uint8_t *buf,size_t cap){
 FILE *f=fopen(path,"rb");assert(f);size_t n=fread(buf,1,cap,f);
 assert(n&&!ferror(f)&&fgetc(f)==EOF&&!fclose(f));return n;
}
int main(int argc,char **argv){
 assert(argc==7);
 struct nts_pin pins[2];
 for(unsigned i=0;i<2;++i){
  size_t n=readfile(argv[1+i*3],pkg[i],sizeof pkg[i]),m=readfile(argv[2+i*3],manifest[i],sizeof manifest[i]);
  size_t e=readfile(argv[3+i*3],elf[i],sizeof elf[i]);
  nte_sha256(pkg[i],n,sha[i]);nte_sha256(elf[i],e,eh[i]);
  pins[i]=(struct nts_pin){sha[i],eh[i],manifest[i],m,(uint32_t)n};
 }
 assert(pins[1].package_bytes>NTP_MAX_PACKAGE);
 struct ntp_payload large={0};
 assert(ntp_open(pkg[1],pins[1].package_bytes,sha[1],eh[1],manifest[1],pins[1].manifest_bytes,&large)==NTP_BOUNDS);
 assert(!large.elf&&!large.elf_bytes);
 assert(ntp105_open(pkg[1],pins[1].package_bytes,sha[1],eh[1],manifest[1],pins[1].manifest_bytes,&large)==NTP_OK);
 assert(large.elf_bytes==20480&&large.elf[12288+0x1ff8]==0xde&&large.elf[12288+0x1ffb]==0x10);
 assert(ntp105_open(pkg[1],NTP105_MAX_PACKAGE+1,sha[1],eh[1],manifest[1],pins[1].manifest_bytes,&large)==NTP_BOUNDS);
 assert(large.elf_bytes==20480);
 unsigned slot=99;
 assert(ntr104_match(&registry,pkg[0],pins[0].package_bytes,&slot)==NTR104_UNAPPROVED&&slot==99);
 assert(ntr104_register(&registry,&pins[0],"",false,&slot)==NTR104_INVALID&&slot==99);
 assert(ntr104_register(&registry,&pins[0],"First",false,&slot)==NTR104_OK&&slot==0);
 assert(ntr104_register(&registry,&pins[0],"Alias",true,&slot)==NTR104_DUPLICATE&&slot==0);
 assert(ntr104_match(&registry,pkg[1],pins[1].package_bytes,&slot)==NTR104_UNAPPROVED&&slot==0);
 assert(ntr104_register(&registry,&pins[1],"Second",true,&slot)==NTR104_OK&&slot==1);
 assert(ntr104_match(&registry,pkg[0],pins[0].package_bytes,&slot)==NTR104_OK&&slot==0);
 assert(ntr104_match(&registry,pkg[1],pins[1].package_bytes,&slot)==NTR104_OK&&slot==1);
 struct nts_pin p;
 assert(ntr104_pin(&registry,1,&p)&&p.package_sha!=pins[1].package_sha&&p.manifest!=pins[1].manifest);
 memset(sha[1],0,32);memset(eh[1],0,32);memset(manifest[1],0,sizeof manifest[1]);
 assert(ntr104_match(&registry,pkg[1],pins[1].package_bytes,&slot)==NTR104_OK&&slot==1);
 assert(registry.entries[1].wait_input&&!registry.entries[0].wait_input);
 pkg[0][0]^=1;
 assert(ntr104_match(&registry,pkg[0],pins[0].package_bytes,&slot)==NTR104_UNAPPROVED&&slot==1);
 uint8_t badhash[32];nte_sha256(pkg[0],pins[0].package_bytes,badhash);
 struct nts_pin bad=pins[0];bad.package_sha=badhash;
 assert(ntr104_register(&registry,&bad,"Malformed approved bytes",false,&slot)==NTR104_OK&&slot==2);
 assert(ntr104_match(&registry,pkg[0],pins[0].package_bytes,&slot)==NTR104_PACKAGE&&slot==2);
 pkg[0][0]^=1;
 badhash[0]^=1;
 assert(ntr104_register(&registry,&bad,"Fourth",false,&slot)==NTR104_OK&&slot==3);
 badhash[1]^=1;
 assert(ntr104_register(&registry,&bad,"Overflow",false,&slot)==NTR104_FULL&&slot==3);
 assert(ntr104_match(&registry,pkg[0],pins[0].package_bytes,(unsigned *)&registry)==NTR104_INVALID);
 puts("TEA105 registry PASS: legacy limit retained, expanded package, immutable pins, content lookup, malformed and full refusal");
 return 0;
}
