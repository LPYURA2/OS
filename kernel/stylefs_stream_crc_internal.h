#ifndef STYLEOS_STREAM_CRC_INTERNAL_H
#define STYLEOS_STREAM_CRC_INTERNAL_H
#include <stdint.h>
#include <stddef.h>
/* Existing stream CRC32, shared unchanged by both storage paths. */
static inline uint32_t sfs64_stream_crc_update(uint32_t c,const uint8_t *p,size_t n){
    for(size_t i=0;i<n;++i){c^=p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}
    return c;
}
#endif
