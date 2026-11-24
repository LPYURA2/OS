#include <styleos/ecam_map_capture_826.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
static char out[70000];static size_t used;
void arch_putc(char c){assert(used+1<sizeof out);out[used++]=c;out[used]=0;}
void arch_puts(const char*s){while(*s)arch_putc(*s++);}
_Noreturn void arch_halt(void){abort();}
static _Alignas(8) unsigned char map[STYLEOS826_MAP_LIMIT+48];
static struct boot_info valid(void){
 return (struct boot_info){.magic=STYLEOS_BOOT_MAGIC,.version=STYLEOS_BOOT_VERSION,.size=sizeof(struct boot_info),.arch=STYLEOS_ARCH_X86_64,.platform=STYLEOS_PLATFORM_UEFI,.flags=STYLEOS_BOOT_EXITED_SERVICES,.memory_map=(uintptr_t)map,.memory_map_size=48,.descriptor_size=48,.descriptor_version=1,.uart_base=0x3f8};
}
static void reject(const struct boot_info*b){used=0;out[0]=0;assert(!styleos826_emit_final_map(b));assert(!used);}
#define BAD(field,value) do{struct boot_info b=valid();b.field=(value);reject(&b);n++;}while(0)
int main(void){
 unsigned n=0; for(unsigned i=0;i<sizeof map;i++)map[i]=(unsigned char)i;
 struct boot_info b=valid();unsigned char saved[48];memcpy(saved,map,48);
 assert(styleos826_emit_final_map(&b));assert(!memcmp(saved,map,48));
 const char *prefix="STYLEOS S826 FINAL-MAP size=0x0000000000000030 stride=0x0000000000000030 version=0x0000000000000001 data=";
 assert(!strncmp(out,prefix,strlen(prefix)));assert(strlen(out)==strlen(prefix)+97);
 for(unsigned i=0;i<48;i++){char h[3];snprintf(h,3,"%02x",map[i]);assert(!memcmp(out+strlen(prefix)+2*i,h,2));}n++;
 reject(NULL);n++; BAD(magic,0);BAD(version,2);BAD(size,0);BAD(arch,STYLEOS_ARCH_AARCH64);BAD(platform,0);BAD(flags,0);BAD(memory_map,0);BAD(memory_map,(uintptr_t)map+1);BAD(memory_map,UINT64_MAX-7);BAD(memory_map_size,0);BAD(memory_map_size,47);BAD(memory_map_size,STYLEOS826_MAP_LIMIT+16);BAD(descriptor_size,0);BAD(descriptor_size,39);BAD(descriptor_size,41);BAD(descriptor_size,4104);BAD(descriptor_version,2);BAD(dtb,1);BAD(boot_el,1);BAD(uart_base,0);BAD(reserved0,1);
 printf("S826 capture host %u/%u PASS (synthetic handoff; no hardware)\n",n,n);
}
