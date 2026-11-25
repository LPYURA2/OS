/* Explicit deny-only adapters: no probe, no fake storage, clock or reset.
 * The real PS/2 adapter is linked separately and admitted by S863. */
#include <styleos/desktop_v0.h>
#include <styleos/arch.h>
#ifndef STYLEOS_TEST_DESKTOP_INTERACTIVE_863
#error diagnostic profile only
#endif
bool desk_clock(unsigned *h,unsigned *m){(void)h;(void)m;return false;}
void desk_pointer_clock_init(void){arch_puts("STYLEOS S863 CLOCK DENIED\n");}
uint64_t desk_pointer_clock(void){return 0;}
bool desk_pointer_near(uint64_t a,uint64_t b){(void)a;(void)b;return false;}
void desk_reboot(void){arch_puts("STYLEOS S863 RESET DENIED\n");}
uint32_t desk_ata_probe(void){arch_puts("STYLEOS S863 STORAGE DENIED\n");return 0;}
bool desk_ata_read(uint32_t lba,uint8_t out[512]){(void)lba;(void)out;return false;}
bool desk_ata_write(uint32_t lba,const uint8_t in[512]){(void)lba;(void)in;return false;}
bool desk_ata_flush(void){return false;}
uint32_t desk_ata_probe_data2(void){return 0;}
bool desk_ata_read_data2(uint32_t lba,uint8_t out[512]){(void)lba;(void)out;return false;}
bool desk_ata_write_data2(uint32_t lba,const uint8_t in[512]){(void)lba;(void)in;return false;}
bool desk_ata_flush_data2(void){return false;}
