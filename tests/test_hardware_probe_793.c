#include <styleos/hardware_probe_793.h>
#include "efi.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    struct efi_memory_descriptor map[3] = {
        {.type=11,.physical_start=0x80000000,.number_of_pages=16,.attribute=1},
        {.type=7,.physical_start=0x100000,.number_of_pages=256,.attribute=8},
        {.type=6,.physical_start=0x300000,.number_of_pages=2,
         .attribute=(UINT64_C(1)<<63)|8}
    };
    struct boot_info info={.magic=STYLEOS_BOOT_MAGIC,.version=STYLEOS_BOOT_VERSION,
        .size=sizeof(struct boot_info),.arch=STYLEOS_ARCH_X86_64,
        .platform=STYLEOS_PLATFORM_UEFI,.flags=STYLEOS_BOOT_EXITED_SERVICES,
        .memory_map=(uintptr_t)map,.memory_map_size=sizeof(map),
        .descriptor_size=sizeof(map[0]),.descriptor_version=1};
    struct hw793_map_report r;
    assert(hw793_map_inspect(&info,0x80000123,0x2345,&r));
    assert(r.page_start==0x80000000 && r.page_end==0x80003000);
    assert(r.all_pages_described && r.covered_bytes==0x3000);
    assert(r.overlaps==1 && r.matched_type==11 && r.runtime_count==1);
    assert(hw793_map_inspect(&info,0x8000f000,0x4000,&r));
    assert(!r.all_pages_described && r.covered_bytes==0x1000);
    info.descriptor_size=39;
    assert(!hw793_map_inspect(&info,0x80000000,4096,&r));
    info.descriptor_size=sizeof(map[0]);
    info.descriptor_version=2;
    assert(!hw793_map_inspect(&info,0x80000000,4096,&r));
    info.descriptor_version=1;
    assert(!hw793_map_inspect(&info,UINT64_MAX-4095,8192,&r));
    info.memory_map_size=sizeof(map[0])*4097u;
    assert(!hw793_map_inspect(&info,0x80000000,4096,&r));
    info.memory_map_size=sizeof(map);
    map[1].physical_start=0x80000000;
    assert(!hw793_map_inspect(&info,0x80000000,4096,&r));
    puts("STYLEOS 793 HOST PASS malformed, fragmented and overlapping descriptors rejected");
    return 0;
}
