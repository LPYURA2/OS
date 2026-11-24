/* Terminal first-frame profile: no desktop device driver may be called.
 * Fail instead of pretending a disk, input controller or clock exists. */
#include <styleos/desktop_v0.h>
#include <styleos/arch.h>
static _Noreturn void denied(void)
{
    arch_puts("STYLEOS S852 FAIL desktop-io-denied\n");
    arch_halt();
}
#ifndef STYLEOS_TEST_DESKTOP_INPUT_853
unsigned desk_input_init(void){denied();}
bool desk_input_next(struct desk_event *e){(void)e;denied();}
#endif
bool desk_clock(unsigned *h,unsigned *m){(void)h;(void)m;denied();}
void desk_pointer_clock_init(void){denied();}
uint64_t desk_pointer_clock(void){denied();}
bool desk_pointer_near(uint64_t a,uint64_t b){(void)a;(void)b;denied();}
void desk_reboot(void){denied();}
uint32_t desk_ata_probe(void){denied();}
bool desk_ata_read(uint32_t lba,uint8_t out[512]){(void)lba;(void)out;denied();}
bool desk_ata_write(uint32_t lba,const uint8_t in[512]){(void)lba;(void)in;denied();}
bool desk_ata_flush(void){denied();}
uint32_t desk_ata_probe_data2(void){denied();}
bool desk_ata_read_data2(uint32_t lba,uint8_t out[512]){(void)lba;(void)out;denied();}
bool desk_ata_write_data2(uint32_t lba,const uint8_t in[512]){(void)lba;(void)in;denied();}
bool desk_ata_flush_data2(void){denied();}
