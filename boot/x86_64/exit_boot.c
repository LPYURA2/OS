#include "exit_boot.h"

EFI_STATUS styleos_exit_boot_services(struct efi_boot_services *bs,
                                     EFI_HANDLE image, struct boot_info *info)
{
    void *buffer = NULL;
    UINTN capacity = 0;
    EFI_STATUS status = EFI_ABORTED;

    if (bs == NULL || image == NULL || info == NULL ||
        bs->get_memory_map == NULL || bs->allocate_pool == NULL ||
        bs->free_pool == NULL || bs->exit_boot_services == NULL)
        return EFI_INVALID_PARAMETER;

    for (unsigned int attempt = 0; attempt < STYLEOS_EBS_MAX_ATTEMPTS; ++attempt) {
        UINTN map_size = capacity;
        UINTN map_key = 0;
        UINTN stride = sizeof(struct efi_memory_descriptor);
        uint32_t descriptor_version = 0;

        status = bs->get_memory_map(&map_size, buffer, &map_key, &stride,
                                    &descriptor_version);
        if (status == EFI_BUFFER_TOO_SMALL) {
            UINTN slack;
            if (stride < sizeof(struct efi_memory_descriptor) || stride > 4096u) {
                status = EFI_INVALID_PARAMETER;
                break;
            }
            slack = stride * 8u;
            if (map_size == 0 || map_size > STYLEOS_MAX_MEMORY_MAP - slack) {
                status = EFI_OUT_OF_RESOURCES;
                break;
            }
            if (buffer != NULL) {
                status = bs->free_pool(buffer);
                buffer = NULL;
                capacity = 0;
                if (status != EFI_SUCCESS)
                    return status;
            }
            capacity = map_size + slack;
            status = bs->allocate_pool(EFI_LOADER_DATA, capacity, &buffer);
            if (status != EFI_SUCCESS || buffer == NULL) {
                buffer = NULL;
                return status == EFI_SUCCESS ? EFI_OUT_OF_RESOURCES : status;
            }
            /* Allocation is not a completed handoff. In particular, exhaustion
             * immediately after the last growth must never return EFI_SUCCESS. */
            status = EFI_ABORTED;
            continue;
        }
        if (status != EFI_SUCCESS)
            break;
        if (buffer == NULL || map_size == 0 || map_size > capacity ||
            stride < sizeof(struct efi_memory_descriptor) || stride > 4096u ||
            map_size % stride != 0 || descriptor_version != 1u) {
            status = EFI_INVALID_PARAMETER;
            break;
        }

        /* Direct memory writes only between the final map and ExitBootServices.
         * In particular, no console protocol, allocation or FreePool is called. */
        info->memory_map = (uint64_t)(uintptr_t)buffer;
        info->memory_map_size = map_size;
        info->descriptor_size = (uint32_t)stride;
        info->descriptor_version = descriptor_version;
        status = bs->exit_boot_services(image, map_key);
        if (status == EFI_SUCCESS) {
            info->flags |= STYLEOS_BOOT_EXITED_SERVICES;
            return EFI_SUCCESS;
        }
        if (status != EFI_INVALID_PARAMETER)
            break;
        /* A changed map key is the one retryable ExitBootServices error. */
        status = EFI_ABORTED;
    }

    if (buffer != NULL)
        (void)bs->free_pool(buffer);
    info->memory_map = 0;
    info->memory_map_size = 0;
    info->descriptor_size = 0;
    info->descriptor_version = 0;
    return status;
}
