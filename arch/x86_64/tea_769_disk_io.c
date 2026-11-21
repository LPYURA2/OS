#include <styleos/tea_769_disk_io.h>

#define BASE 0x1f0
#define CTRL 0x3f6
#define TIMEOUT 300000u
static uint32_t max_lba;
static inline void o8(uint16_t port,uint8_t value)
{__asm__ volatile("outb %0,%1"::"a"(value),"Nd"(port));}
static inline uint8_t i8(uint16_t port)
{uint8_t x;__asm__ volatile("inb %1,%0":"=a"(x):"Nd"(port));return x;}
static inline uint16_t i16(uint16_t port)
{uint16_t x;__asm__ volatile("inw %1,%0":"=a"(x):"Nd"(port));return x;}
static void delay(void){for(unsigned i=0;i<4;++i)(void)i8(CTRL);}
static int wait_drq(int want)
{
    for(unsigned i=0;i<TIMEOUT;++i){
        uint8_t s=i8(BASE+7);
        if(s==0 || s==255)return 0;
        if(s&0x80u)continue;
        if(s&(0x20u|1u))return 0;
        if(!!(s&8u)==!!want)return 1;
    }
    return 0;
}
int tea_769_disk_init(uint32_t *sectors)
{
    if(!sectors)return 0;
    max_lba=0;
    o8(CTRL,2);o8(BASE+6,0xa0);delay();
    if(!wait_drq(0))return 0;
    for(uint16_t p=BASE+2;p<=BASE+5;++p)o8(p,0);
    o8(BASE+7,0xec);delay();
    if(!wait_drq(1) || i8(BASE+4) || i8(BASE+5))return 0;
    uint16_t w[256];for(unsigned i=0;i<256;++i)w[i]=i16(BASE);
    if(!wait_drq(0) || !(w[49]&(1u<<9)))return 0;
    max_lba=(uint32_t)w[60]|((uint32_t)w[61]<<16);
    if(max_lba<34 || max_lba>=UINT32_C(0x10000000))return 0;
    *sectors=max_lba;return 1;
}
int tea_769_disk_sector(uint32_t lba,uint8_t out[512])
{
    if(!out || !max_lba || lba>=max_lba || lba>=UINT32_C(0x10000000))return 0;
    o8(BASE+6,(uint8_t)(0xe0u|(lba>>24)));delay();
    if(!wait_drq(0))return 0;
    o8(BASE+1,0);o8(BASE+2,1);
    o8(BASE+3,(uint8_t)lba);o8(BASE+4,(uint8_t)(lba>>8));
    o8(BASE+5,(uint8_t)(lba>>16));o8(BASE+7,0x20);delay();
    if(!wait_drq(1))return 0;
    for(unsigned i=0;i<256;++i){uint16_t w=i16(BASE);out[2*i]=(uint8_t)w;out[2*i+1]=(uint8_t)(w>>8);}
    delay();return wait_drq(0);
}
