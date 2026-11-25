#include <styleos/native_tea_elf.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) uint8_t elf[8192],page[4096];
static struct nte_loaded loaded;
static uint8_t digest[32];
static unsigned checks;
#define OK(x) do{assert(x);++checks;}while(0)
static void put(unsigned off,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)elf[off+i]=(uint8_t)(v>>(8*i));}
static void build(void){
 memset(elf,0,sizeof elf);
 elf[0]=127;elf[1]='E';elf[2]='L';elf[3]='F';elf[4]=2;elf[5]=1;elf[6]=1;
 put(16,2,2);put(18,62,2);put(20,1,4);put(24,NTE_USER_CODE_VA+32,8);
 put(32,64,8);put(52,64,2);put(54,56,2);put(56,1,2);put(64,1,4);put(68,5,4);
 put(72,4096,8);put(80,NTE_USER_CODE_VA,8);put(96,128,8);put(104,4096,8);put(112,4096,8);
 for(unsigned i=0;i<128;i++)elf[4096+i]=(uint8_t)(i*7+3);
 nte_sha256(elf,4224,digest);
}
static void refuse(enum nte_status status){
 memset(page,0xa5,sizeof page);loaded=(struct nte_loaded){17,18,19};
 OK(nte_load_x64_image(elf,4224,digest,page,sizeof page,&loaded)==status);
 OK(loaded.entry==17&&loaded.code_bytes==18&&loaded.memory_bytes==19);
 for(unsigned i=0;i<sizeof page;i++)OK(page[i]==0xa5);
}
int main(void){
 build();OK(nte_load_x64_image(elf,4224,digest,page,sizeof page,&loaded)==NTE_OK);
 OK(loaded.entry==NTE_USER_CODE_VA+32&&loaded.code_bytes==128&&loaded.memory_bytes==4096);
 OK(!memcmp(page,elf+4096,128));for(unsigned i=128;i<4096;i++)OK(!page[i]);
 OK(nte_load_x64_page(elf,4224,digest,page,sizeof page,&loaded)==NTE_RIGHTS);
 build();put(24,NTE_USER_CODE_VA+128,8);nte_sha256(elf,4224,digest);refuse(NTE_RIGHTS);
 build();put(68,7,4);nte_sha256(elf,4224,digest);refuse(NTE_RIGHTS);
 build();put(96,129,8);nte_sha256(elf,4224,digest);refuse(NTE_BOUNDS);
 build();elf[4096]^=1;refuse(NTE_HASH);
 build();uint8_t original[8192];memcpy(original,elf,sizeof elf);
 OK(nte_load_x64_image(elf,4224,digest,elf+4096,4096,&loaded)==NTE_ARGUMENT);
 OK(!memcmp(elf,original,sizeof elf));
 OK(nte_load_x64_image(elf,4224,page,page,4096,&loaded)==NTE_ARGUMENT);
 OK(nte_load_x64_image(elf,4224,digest,page,4096,(struct nte_loaded *)(void *)page)==NTE_ARGUMENT);
 printf("S865 native image loader HOST ONLY: %u checks PASS\n",checks);
}
