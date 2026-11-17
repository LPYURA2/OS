#include <styleos/sdk_v0.h>

/* One editable slide: its whole text is supplied by the caller. On success,
 * the reply becomes the replacement slide text. The caller owns I/O and UI. */
uint32_t styleos_app_text_v0(const uint8_t *input, uint32_t input_len,
                             uint8_t *output, uint32_t output_capacity)
{
    if (input_len > STYLEOS_TEXT_MAX || input_len > output_capacity ||
        (input_len && (!input || !output))) {
        return STYLEOS_TEXT_REJECTED;
    }
    for (uint32_t i = 0; i < input_len; ++i) {
        output[i] = input[i];
    }
    return input_len;
}
