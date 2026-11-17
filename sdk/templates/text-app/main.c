#include <styleos/sdk_v0.h>

uint32_t styleos_app_text_v0(const uint8_t *input, uint32_t input_len,
                             uint8_t *output, uint32_t output_capacity)
{
    (void)input;
    (void)input_len;
    static const char message[] = "Hello from my StyleOS app!";
    uint32_t bytes = (uint32_t)sizeof(message) - 1;
    if (!output || bytes > output_capacity) {
        return STYLEOS_TEXT_REJECTED;
    }
    for (uint32_t i = 0; i < bytes; ++i) {
        output[i] = (uint8_t)message[i];
    }
    return bytes;
}
