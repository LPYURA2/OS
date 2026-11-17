#ifndef STYLEOS_SDK_V0_H
#define STYLEOS_SDK_V0_H

#include <stdint.h>

#define STYLEOS_TEXT_MAX UINT32_C(4096)
#define STYLEOS_TEXT_REJECTED UINT32_MAX

/* Experimental text callback. The runner supplies distinct buffers.
 * Return bytes written, not including a terminator, or STYLEOS_TEXT_REJECTED.
 * This is not a native process startup routine or a stable system call ABI. */
uint32_t styleos_app_text_v0(const uint8_t *input, uint32_t input_len,
                             uint8_t *output, uint32_t output_capacity);

#endif
