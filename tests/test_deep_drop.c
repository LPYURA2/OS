#include <styleos/deep_drop.h>
#include <styleos/sdk_v0.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct destination {
    uint8_t data[STYLEOS_TEXT_MAX];
    size_t size;
    unsigned applies;
};

static void apply(void *context, const uint8_t *data, size_t size)
{
    struct destination *destination = context;
    assert(size <= sizeof(destination->data));
    memcpy(destination->data, data, size);
    destination->size = size;
    ++destination->applies;
}

static bool prepare_slide(void *context, const struct styleos_drop_offer *offer,
                          uint8_t *staging, size_t capacity, size_t *written)
{
    (void)context;
    if (offer->size > UINT32_MAX || capacity > UINT32_MAX) {
        return false;
    }
    const uint32_t result = styleos_app_text_v0(offer->data, (uint32_t)offer->size,
                                                staging, (uint32_t)capacity);
    if (result == STYLEOS_TEXT_REJECTED) {
        return false;
    }
    *written = result;
    return true;
}

static bool prepare_other(void *context, const struct styleos_drop_offer *offer,
                          uint8_t *staging, size_t capacity, size_t *written)
{
    ++*(unsigned *)context;
    if (offer->size > capacity) {
        return false;
    }
    for (size_t i = 0; i < offer->size; ++i) {
        staging[i] = offer->data[offer->size - 1 - i];
    }
    *written = offer->size;
    return true;
}

static bool excessive_reply(void *context, const struct styleos_drop_offer *offer,
                            uint8_t *staging, size_t capacity, size_t *written)
{
    (void)context;
    (void)offer;
    staging[0] = 0xff;
    *written = capacity + 1;
    return true;
}

static void expect(const struct destination *destination, const uint8_t *data,
                   size_t size, unsigned applies)
{
    assert(destination->size == size);
    assert(destination->applies == applies);
    assert(memcmp(destination->data, data, size) == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    static const char *const slide_types[] = {"text/plain"};
    static const uint8_t selection[] = "Выделенный текст\nСтрока 2";
    static const uint8_t expected_file[] = "Текст из файла\r\nВторая строка\n";
    uint8_t staging[STYLEOS_TEXT_MAX];
    uint8_t file_bytes[STYLEOS_TEXT_MAX + 1];
    struct destination slide = {0};
    const struct styleos_drop_target slides = {
        .accepted_types = slide_types,
        .accepted_type_count = 1,
        .max_input_size = STYLEOS_TEXT_MAX,
        .prepare = prepare_slide,
        .apply = apply,
        .apply_context = &slide,
    };

    struct styleos_drop_offer offer = {"text/plain", selection, sizeof(selection) - 1};
    assert(styleos_drop_transfer(&offer, &slides, staging, sizeof(staging)) == STYLEOS_DROP_OK);
    expect(&slide, selection, offer.size, 1);
    puts("PASS selection to SDK Presentations callback");

    /* Files adapter for the stand. Resolve the selected file outside the
     * transfer core. Neither the core nor the receiver gets its path. */
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    size_t file_size = fread(file_bytes, 1, sizeof(file_bytes), file);
    assert(!ferror(file));
    assert(fclose(file) == 0);
    assert(file_size == sizeof(expected_file) - 1);
    assert(memcmp(file_bytes, expected_file, file_size) == 0);
    offer = (struct styleos_drop_offer){"text/plain", file_bytes, file_size};
    assert(styleos_drop_transfer(&offer, &slides, staging, sizeof(staging)) == STYLEOS_DROP_OK);
    expect(&slide, expected_file, file_size, 2);
    puts("PASS real .txt to same callback with UTF-8 and exact line endings");

    offer = (struct styleos_drop_offer){"image/png", file_bytes, file_size};
    assert(styleos_drop_transfer(&offer, &slides, staging, sizeof(staging)) ==
           STYLEOS_DROP_UNSUPPORTED_TYPE);
    expect(&slide, expected_file, file_size, 2);
    puts("PASS reject unsupported type without editing the slide");

    const struct styleos_drop_offer too_big = {"text/plain", file_bytes, sizeof(file_bytes)};
    assert(styleos_drop_transfer(&too_big, &slides, staging, sizeof(staging)) == STYLEOS_DROP_TOO_LARGE);
    expect(&slide, expected_file, file_size, 2);
    puts("PASS reject input over receiver limit");

    const struct styleos_drop_offer malformed = {"text/plain", NULL, 1};
    assert(styleos_drop_transfer(&malformed, &slides, staging, sizeof(staging)) == STYLEOS_DROP_INVALID);
    expect(&slide, expected_file, file_size, 2);
    puts("PASS reject malformed offer");

    offer = (struct styleos_drop_offer){"text/plain", file_bytes, file_size};
    assert(styleos_drop_transfer(&offer, &slides, staging, 1) == STYLEOS_DROP_PREPARE_FAILED);
    expect(&slide, expected_file, file_size, 2);
    puts("PASS SDK callback refusal leaves the slide intact");

    struct styleos_drop_target bad_reply = slides;
    bad_reply.prepare = excessive_reply;
    assert(styleos_drop_transfer(&offer, &bad_reply, staging, sizeof(staging)) ==
           STYLEOS_DROP_PREPARE_FAILED);
    expect(&slide, expected_file, file_size, 2);
    puts("PASS invalid staged result is not applied");

    static const char *const other_types[] = {"text/plain", "application/octet-stream"};
    static const uint8_t binary[] = {0, 128, 255};
    static const uint8_t reversed[] = {255, 128, 0};
    struct destination other = {0};
    unsigned prepares = 0;
    const struct styleos_drop_target other_target = {
        .accepted_types = other_types,
        .accepted_type_count = 2,
        .max_input_size = sizeof(binary),
        .prepare = prepare_other,
        .prepare_context = &prepares,
        .apply = apply,
        .apply_context = &other,
    };
    assert(styleos_drop_transfer(&offer, &other_target, staging, sizeof(staging)) == STYLEOS_DROP_TOO_LARGE);
    assert(prepares == 0 && other.applies == 0);
    offer = (struct styleos_drop_offer){"application/octet-stream", binary, sizeof(binary)};
    assert(styleos_drop_transfer(&offer, &other_target, staging, sizeof(staging)) == STYLEOS_DROP_OK);
    assert(prepares == 1);
    expect(&other, reversed, sizeof(reversed), 1);
    expect(&slide, expected_file, file_size, 2);
    puts("PASS independent receiver handles opaque typed bytes through the same core");
    return 0;
}
