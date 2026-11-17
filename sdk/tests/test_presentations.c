#include <assert.h>
#include <string.h>
#include <styleos/sdk_v0.h>

int main(void)
{
    const uint8_t input[] = {0xd0u, 0xa2u, 'e', 'x', 't', '\n'};
    uint8_t output[sizeof(input) + 1] = {0};
    output[sizeof(input)] = 0x5a;
    assert(styleos_app_text_v0(input, sizeof(input), output, sizeof(output)) == sizeof(input));
    assert(memcmp(input, output, sizeof(input)) == 0);
    assert(output[sizeof(input)] == 0x5a);
    assert(styleos_app_text_v0(input, sizeof(input), output, sizeof(input) - 1) == STYLEOS_TEXT_REJECTED);
    assert(styleos_app_text_v0(input, STYLEOS_TEXT_MAX + 1, output, sizeof(output)) == STYLEOS_TEXT_REJECTED);
    assert(styleos_app_text_v0(0, 0, 0, 0) == 0);
    assert(styleos_app_text_v0(0, 1, output, sizeof(output)) == STYLEOS_TEXT_REJECTED);
    return 0;
}
