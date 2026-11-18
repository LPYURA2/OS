#ifndef STYLEOS_DEEP_DROP_H
#define STYLEOS_DEEP_DROP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum styleos_drop_result {
    STYLEOS_DROP_OK,
    STYLEOS_DROP_INVALID,
    STYLEOS_DROP_UNSUPPORTED_TYPE,
    STYLEOS_DROP_TOO_LARGE,
    STYLEOS_DROP_PREPARE_FAILED,
};

/* The source has already resolved a user selection. An offer carries bytes,
 * not the source's path, directory handle, window, or application identity. */
struct styleos_drop_offer {
    const char *media_type;
    const uint8_t *data;
    size_t size;
};

struct styleos_drop_target {
    const char *const *accepted_types;
    size_t accepted_type_count;
    size_t max_input_size;

    /* Prepare writes only to staging (and returns its length). It must not
     * change live destination state or retain pointers into an offer. */
    bool (*prepare)(void *context, const struct styleos_drop_offer *offer,
                    uint8_t *staging, size_t capacity, size_t *written);
    void *prepare_context;

    /* Commit a prepared result. This callback is infallible for a validated
     * reply. It may copy the bytes but must not retain the staging pointer. */
    void (*apply)(void *context, const uint8_t *data, size_t size);
    void *apply_context;
};

/* Caller owns all buffers and supplies non-overlapping input and staging.
 * Type matching is exact. This synchronous core is transport-independent;
 * it does not implement process isolation, filesystem access, or UI. */
enum styleos_drop_result styleos_drop_transfer(
    const struct styleos_drop_offer *offer,
    const struct styleos_drop_target *target,
    uint8_t *staging, size_t capacity);

#endif
