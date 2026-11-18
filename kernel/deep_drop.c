#include <styleos/deep_drop.h>

static bool same_type(const char *left, const char *right)
{
    if (!right) {
        return false;
    }
    while (*left && *left == *right) {
        ++left;
        ++right;
    }
    return *left == *right;
}

enum styleos_drop_result styleos_drop_transfer(
    const struct styleos_drop_offer *offer,
    const struct styleos_drop_target *target,
    uint8_t *staging, size_t capacity)
{
    if (!offer || !offer->media_type || !*offer->media_type ||
        (!offer->data && offer->size) || !target || !target->prepare ||
        !target->apply || (!target->accepted_types && target->accepted_type_count) ||
        !staging || !capacity) {
        return STYLEOS_DROP_INVALID;
    }

    bool supported = false;
    for (size_t i = 0; i < target->accepted_type_count; ++i) {
        if (same_type(offer->media_type, target->accepted_types[i])) {
            supported = true;
            break;
        }
    }
    if (!supported) {
        return STYLEOS_DROP_UNSUPPORTED_TYPE;
    }
    if (offer->size > target->max_input_size) {
        return STYLEOS_DROP_TOO_LARGE;
    }

    size_t written = SIZE_MAX;
    if (!target->prepare(target->prepare_context, offer, staging, capacity, &written) ||
        written == SIZE_MAX || written > capacity) {
        return STYLEOS_DROP_PREPARE_FAILED;
    }
    target->apply(target->apply_context, staging, written);
    return STYLEOS_DROP_OK;
}
