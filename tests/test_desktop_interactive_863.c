#include <styleos/desktop_v0.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned logs;
void arch_puts(const char *s){assert(s&&strstr(s,"DENIED"));logs++;}
int main(void){unsigned n=0;for(unsigned i=0;i<128;i++){
 uint8_t out[512],before[512];memset(out,(int)i,sizeof out);memcpy(before,out,sizeof out);
 unsigned h=55,m=66;assert(!desk_clock(&h,&m)&&h==55&&m==66);n++;
 desk_pointer_clock_init();assert(!desk_pointer_clock()&&!desk_pointer_near(i,i+1));n++;
 assert(!desk_ata_probe()&&!desk_ata_probe_data2());n++;
 assert(!desk_ata_read(i,out)&&!memcmp(before,out,sizeof out));n++;
 assert(!desk_ata_read_data2(i,out)&&!memcmp(before,out,sizeof out));n++;
 assert(!desk_ata_write(i,out)&&!memcmp(before,out,sizeof out));n++;
 assert(!desk_ata_write_data2(i,out)&&!memcmp(before,out,sizeof out));n++;
 assert(!desk_ata_flush()&&!desk_ata_flush_data2());n++;
 desk_reboot();n++;
 }assert(logs==384);printf("S863 deny adapters HOST ONLY: %u checks PASS; output buffers unchanged; %u explicit deny logs\n",n,logs);return 0;}
