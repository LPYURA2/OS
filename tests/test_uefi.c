#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exit_boot.h"

enum scenario {
    NORMAL, STALE_KEY, MAP_GROWS, ALLOCATION_FAILS, MAP_FAILS,
    EXIT_FAILS, NEVER_STABLE, NEVER_FITS, BAD_DESCRIPTOR, HUGE_MAP, NULL_ALLOCATION
};
static enum scenario scenario;
static unsigned int get_calls, allocate_calls, free_calls, exit_calls;
static UINTN latest_key;
static int exited;

static EFI_STATUS EFIAPI get_map(UINTN *size, struct efi_memory_descriptor *map,
                                 UINTN *key, UINTN *stride, uint32_t *version)
{
    assert(!exited);
    ++get_calls;
    *stride = 48;
    *version = 1;
    if (scenario == MAP_FAILS)
        return EFI_DEVICE_ERROR;
    if (scenario == HUGE_MAP) {
        *size = STYLEOS_MAX_MEMORY_MAP;
        return EFI_BUFFER_TOO_SMALL;
    }
    if (map == NULL) {
        *size = 96;
        return EFI_BUFFER_TOO_SMALL;
    }
    if (scenario == NEVER_FITS) {
        *size += 48;
        return EFI_BUFFER_TOO_SMALL;
    }
    if (scenario == MAP_GROWS && get_calls == 2) {
        *size = 640;
        return EFI_BUFFER_TOO_SMALL;
    }
    assert(*size >= 96);
    memset(map, 0, 96);
    *size = 96;
    *key = ++latest_key;
    if (scenario == BAD_DESCRIPTOR)
        *stride = 39;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI allocate_pool(uint32_t type, UINTN size, void **buffer)
{
    assert(!exited);
    assert(type == EFI_LOADER_DATA);
    assert(size <= STYLEOS_MAX_MEMORY_MAP);
    ++allocate_calls;
    if (scenario == ALLOCATION_FAILS)
        return EFI_OUT_OF_RESOURCES;
    if (scenario == NULL_ALLOCATION) {
        *buffer = NULL;
        return EFI_SUCCESS;
    }
    *buffer = malloc((size_t)size);
    assert(*buffer != NULL);
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI free_pool(void *buffer)
{
    assert(!exited);
    assert(buffer != NULL);
    ++free_calls;
    free(buffer);
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI exit_services(EFI_HANDLE image, UINTN key)
{
    assert(!exited);
    assert(image == (EFI_HANDLE)(uintptr_t)1);
    assert(key == latest_key);
    ++exit_calls;
    if (scenario == EXIT_FAILS)
        return EFI_DEVICE_ERROR;
    if (scenario == NEVER_STABLE || (scenario == STALE_KEY && exit_calls == 1))
        return EFI_INVALID_PARAMETER;
    exited = 1;
    return EFI_SUCCESS;
}

static struct efi_boot_services services = {
    .get_memory_map = get_map,
    .allocate_pool = allocate_pool,
    .free_pool = free_pool,
    .exit_boot_services = exit_services,
};

static EFI_STATUS run(enum scenario value, struct boot_info *info)
{
    scenario = value;
    get_calls = allocate_calls = free_calls = exit_calls = 0;
    latest_key = 0;
    exited = 0;
    memset(info, 0, sizeof(*info));
    return styleos_exit_boot_services(&services, (EFI_HANDLE)(uintptr_t)1, info);
}

static void release_successful_map(struct boot_info *info)
{
    assert(exited);
    assert(info->flags == STYLEOS_BOOT_EXITED_SERVICES);
    assert(info->memory_map != 0 && info->memory_map_size == 96);
    assert(info->descriptor_size == 48 && info->descriptor_version == 1);
    /* Host test cleanup, not a firmware call after ExitBootServices. */
    free((void *)(uintptr_t)info->memory_map);
}

int main(void)
{
    struct boot_info info;
    assert(run(NORMAL, &info) == EFI_SUCCESS);
    assert(get_calls == 2 && allocate_calls == 1 && free_calls == 0 && exit_calls == 1);
    release_successful_map(&info);

    assert(run(STALE_KEY, &info) == EFI_SUCCESS);
    assert(get_calls == 3 && allocate_calls == 1 && free_calls == 0 && exit_calls == 2);
    release_successful_map(&info);

    assert(run(MAP_GROWS, &info) == EFI_SUCCESS);
    assert(get_calls == 3 && allocate_calls == 2 && free_calls == 1 && exit_calls == 1);
    release_successful_map(&info);

    assert(run(ALLOCATION_FAILS, &info) == EFI_OUT_OF_RESOURCES);
    assert(allocate_calls == 1 && exit_calls == 0 && free_calls == 0);
    assert(info.flags == 0 && info.memory_map == 0);

    assert(run(MAP_FAILS, &info) == EFI_DEVICE_ERROR);
    assert(allocate_calls == 0 && exit_calls == 0);

    assert(run(EXIT_FAILS, &info) == EFI_DEVICE_ERROR);
    assert(exit_calls == 1 && free_calls == 1);
    assert(info.memory_map == 0 && info.flags == 0);

    assert(run(NEVER_FITS, &info) == EFI_ABORTED);
    assert(get_calls == STYLEOS_EBS_MAX_ATTEMPTS && exit_calls == 0);
    assert(allocate_calls == free_calls && info.flags == 0);

    assert(run(NEVER_STABLE, &info) == EFI_ABORTED);
    assert(get_calls == STYLEOS_EBS_MAX_ATTEMPTS && exit_calls > 1 && free_calls == 1);
    assert(info.memory_map == 0 && info.flags == 0);

    assert(run(BAD_DESCRIPTOR, &info) == EFI_INVALID_PARAMETER);
    assert(exit_calls == 0 && free_calls == 1);

    assert(run(HUGE_MAP, &info) == EFI_OUT_OF_RESOURCES);
    assert(allocate_calls == 0 && exit_calls == 0);

    assert(run(NULL_ALLOCATION, &info) == EFI_OUT_OF_RESOURCES);
    assert(exit_calls == 0 && free_calls == 0);

    assert(styleos_exit_boot_services(NULL, (EFI_HANDLE)(uintptr_t)1, &info) ==
           EFI_INVALID_PARAMETER);
    assert(styleos_exit_boot_services(&services, NULL, &info) == EFI_INVALID_PARAMETER);
    assert(styleos_exit_boot_services(&services, (EFI_HANDLE)(uintptr_t)1, NULL) ==
           EFI_INVALID_PARAMETER);
    puts("UEFI HOST TESTS PASS: map, stale key, growth, failures, bounded retries (14 cases)");
    return 0;
}
