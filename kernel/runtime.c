/* Freestanding compiler-support routines, not a hosted C library. */
#include <stddef.h>
/* In the isolated S823 ACPICA test only, upstream utclib supplies
 * freestanding memcpy/memset. Never change the ordinary kernel ABI. */
#ifndef STYLEOS_TEST_ACPICA_FIRMWARE_823
void *memset(void *dest, int value, size_t size)
{
    unsigned char *d = dest;
    for (size_t i = 0; i < size; ++i) d[i] = (unsigned char)value;
    return dest;
}
void *memcpy(void *dest, const void *source, size_t size)
{
    unsigned char *d = dest;
    const unsigned char *s = source;
    for (size_t i = 0; i < size; ++i) d[i] = s[i];
    return dest;
}

#endif /* STYLEOS_TEST_ACPICA_FIRMWARE_823 */
