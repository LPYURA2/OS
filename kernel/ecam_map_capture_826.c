#include <styleos/ecam_map_capture_826.h>
#include <styleos/arch.h>
#include <stdint.h>
static void hex64(uint64_t v){
 for(int n=60;n>=0;n-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)n)&15u]);
}
int styleos826_emit_final_map(const struct boot_info *boot){
 if(boot_info_validate(boot,STYLEOS_ARCH_X86_64)!=BOOT_INFO_OK ||
    boot->memory_map_size>STYLEOS826_MAP_LIMIT ||
    boot->memory_map_size/boot->descriptor_size>8192u)return 0;
 const unsigned char *bytes=(const unsigned char *)(uintptr_t)boot->memory_map;
 /* No calls to firmware, allocation or mutation; the map is the successful
  * ExitBootServices handoff allocation, still reserved by the boot contract. */
 arch_puts("STYLEOS S826 FINAL-MAP size=0x");hex64(boot->memory_map_size);
 arch_puts(" stride=0x");hex64(boot->descriptor_size);
 arch_puts(" version=0x");hex64(boot->descriptor_version);
 arch_puts(" data=");
 for(uint64_t i=0;i<boot->memory_map_size;i++){
  unsigned v=bytes[i];arch_putc("0123456789abcdef"[v>>4]);arch_putc("0123456789abcdef"[v&15]);
 }
 arch_putc('\n');return 1;
}
