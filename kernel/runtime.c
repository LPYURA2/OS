/* Freestanding compiler-support routines, not a hosted C library. */
#include <stddef.h>
#include <stdint.h>
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

/* Compiler-generated overlapping aggregate moves in the wide guest.
 * Only this separate profile needs the additional symbol for now.
 * Volatile byte accesses prevent compiler recognition as a recursive
 * memmove libcall; preserve overlap semantics in either direction. */
#ifdef STYLEOS_WIDE_VOLUME_38
void *memmove(void *dest, const void *source, size_t size)
{
    volatile unsigned char *d = dest;
    const volatile unsigned char *s = source;
    if (d == s || !size) return dest;
    if ((uintptr_t)d > (uintptr_t)s &&
        (uintptr_t)d - (uintptr_t)s < size) {
        for (size_t i = size; i != 0; --i) d[i-1] = s[i-1];
    } else {
        for (size_t i = 0; i < size; ++i) d[i] = s[i];
    }
    return dest;
}
#endif
