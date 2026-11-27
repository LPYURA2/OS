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

/* Clang may lower overlapping aggregate assignments to memmove even in
 * freestanding builds. Keep the diagnostic addition out of the ordinary
 * kernel ABI. Volatile byte accesses prevent recursive builtin lowering. */
#ifdef STYLEOS_PAGED_GUEST_34
#include <stdint.h>
void *memmove(void *dest,const void *source,size_t size)
{
    volatile unsigned char *d=dest;
    const volatile unsigned char *s=source;
    if ((uintptr_t)d>(uintptr_t)s && (uintptr_t)d-(uintptr_t)s<size) {
        for (size_t i=size;i>0;--i) d[i-1]=s[i-1];
    } else {
        for (size_t i=0;i<size;++i) d[i]=s[i];
    }
    return dest;
}
#endif
#endif /* STYLEOS_TEST_ACPICA_FIRMWARE_823 */
