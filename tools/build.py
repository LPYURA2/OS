#!/usr/bin/env python3
"""Build real freestanding CPU binaries; no downloaded dependencies or root."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def tool(variable, names):
    value = os.environ.get(variable)
    if value:
        found = shutil.which(value)
        if found:
            return found
        raise RuntimeError(f'{variable} is not executable: {value}')
    for name in names:
        found = shutil.which(name)
        if found:
            return found
    raise RuntimeError(f'Missing {variable}: install {" / ".join(names)}')


def run(args):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True, cwd=ROOT)


def build(arch, output, invalid=False, invalid_map=False, test_exception=False,
          test_bootpool=False, test_paging=False, test_paging_fault=False,
          test_ownership_plan=False, test_ownership_plan_invalid=False,
          test_tasks=False, test_user_processes=False, test_storage=False,
          test_disk_app=False, test_desktop_display=False, desktop_v0=False,
          test_paging_roundtrip=False, test_frame_grants=False, test_tss_ltr=False, test_ist_hw=False, test_guard_pf=False, test_double_fault=False, test_root_slots=False, test_owned_low=False, test_wx_fault=False, test_wx_exec=False, test_hw_probe_793=False, test_pci_probe_794=False, test_pci_aperture_795=False, test_pci_topology_796=False, test_pci_range_799=False, test_cache_800=False, test_cache_801=False, test_cache_803=False, test_cache_804=False, test_irq_apic_805=False, test_acpi_madt_806=False, test_irq_delivery_807=False, test_dmar_808=False, test_vtd_809=False, test_iommu_810=False, test_acpi_namespace_816=False, test_acpi_osl_818=False, test_acpica_bridge_819=False, test_acpica_core_821=False, test_acpica_aml_822=False, acpica_core_lib=None, acpica_core_include=None, test_acpi_hostbridge_815=False, test_pci_rootbridge_814=False, test_pci_mcfg_813=False, test_pci_requester_812=False, test_pci_requester_811=False, test_native_tea=False, test_native_tea_disk=False, test_native_tea_session=False, test_user_syscall=False, test_user_syscall_deny=False, test_user_ipc=False, test_user_ipc_deny=False, update_build=None, update_public_key=None, test_update_fail=False):
    if update_build is not None and (not desktop_v0 or arch != 'x86_64' or not update_public_key):
        raise ValueError('updatable guest requires x86-64 desktop and an Ed25519 public key')
    if test_acpica_aml_822:
        test_acpica_core_821 = True
        test_acpi_namespace_816 = True
    if test_acpica_core_821:
        if arch != "x86_64" or not acpica_core_lib or not acpica_core_include:
            raise ValueError("S821 requires x86-64 and verified ACPICA core paths")
        if not test_acpica_aml_822: test_acpica_bridge_819 = True
    if test_acpica_bridge_819:
        test_acpi_osl_818 = True
    if test_acpi_osl_818:
        if arch != 'x86_64': raise ValueError('818 OSL guest profile requires x86_64')
        test_acpi_namespace_816 = True
    if test_acpi_namespace_816 and arch != 'x86_64':
        raise ValueError('816 AML literal namespace diagnostic requires x86_64')
    if test_acpi_hostbridge_815 and arch != 'x86_64':
        raise ValueError('815 ACPI DSDT diagnostic requires x86_64')
    if test_pci_rootbridge_814 and arch != 'x86_64':
        raise ValueError('814 root bridge profile requires x86_64')
    if test_pci_mcfg_813 and arch != 'x86_64':
        raise ValueError('813 MCFG segment audit requires x86_64')
    if test_pci_requester_812 and arch != 'x86_64':
        raise ValueError('812 PCI hierarchy requires x86_64')
    if test_pci_requester_811 and arch != 'x86_64':
        raise ValueError('811 requester scope probe requires x86_64')
    if test_iommu_810 and arch != 'x86_64':
        raise ValueError('810 cross-vendor ACPI IOMMU probe requires x86_64')
    if test_vtd_809 and arch != 'x86_64':
        raise ValueError('809 read-only VT-d diagnostic requires x86_64')
    if test_dmar_808 and arch != 'x86_64':
        raise ValueError('808 DMAR diagnostic requires x86_64')
    if test_hw_probe_793 and arch != 'x86_64':
        raise ValueError('793 terminal firmware/device observation requires x86-64')
    if test_irq_delivery_807 and arch != 'x86_64':
        raise ValueError('807 IRQ delivery requires x86_64')
    if test_acpi_madt_806 and arch != 'x86_64':
        raise ValueError('806 ACPI MADT requires x86_64')
    if test_irq_apic_805 and arch != 'x86_64':
        raise ValueError('805 IRQ/APIC snapshot requires x86-64')
    if test_cache_804 and arch != 'x86_64':
        raise ValueError('804 same-boot complete-known-range scan requires x86-64')
    if test_cache_803 and arch != 'x86_64':
        raise ValueError('803 same-boot PAT/MTRR/PTE observation is x86-64 only')
    if test_cache_801 and arch != 'x86_64':
        raise ValueError('801 read-only live page table observation requires x86-64')
    if test_cache_800 and arch != 'x86_64':
        raise ValueError('800 read-only MTRR/PAT evidence is x86-64 only')
    if test_pci_range_799 and arch != 'x86_64':
        raise ValueError('799 isolated bridge BAR measurement requires x86-64')
    if test_pci_topology_796 and arch != 'x86_64':
        raise ValueError('796 PCI topology terminal diagnostic requires x86_64')
    if test_pci_aperture_795 and arch != 'x86_64':
        raise ValueError('795 terminal PCI aperture probe requires x86-64')
    if test_pci_probe_794 and arch != 'x86_64':
        raise ValueError('794 bus-zero PCI BAR observation requires x86-64')
    if test_wx_fault:
        if arch != 'x86_64': raise ValueError('791 hardware W^X probes require x86-64')
        test_frame_grants = True
    if test_owned_low:
        if arch != 'x86_64': raise ValueError('790 detached low hierarchy hardware test requires x86-64')
        test_frame_grants = True
    if test_root_slots:
        if arch != 'x86_64': raise ValueError('789 restricted inherited PML4 hardware test requires x86-64')
        test_frame_grants = True
    if test_double_fault:
        if arch != 'x86_64': raise ValueError('788 double-fault hardware probe requires x86-64')
        test_frame_grants = True
    if test_guard_pf:
        if arch != 'x86_64': raise ValueError('787 guard-page hardware probe requires x86-64')
        test_frame_grants = True
    if test_ist_hw:
        if arch != 'x86_64': raise ValueError('786 terminal hardware IST test is x86-64 only')
        test_frame_grants = True
    if test_tss_ltr:
        if arch != 'x86_64': raise ValueError('784 hardware-only TSS LTR trial is x86-64 only')
        test_frame_grants = True
    if test_user_ipc_deny: test_user_ipc=True
    if test_user_ipc:
        if arch != 'x86_64': raise ValueError('773 IPC hardware probe is x86-64 only')
        if test_user_syscall or test_user_syscall_deny or test_native_tea:
            raise ValueError('773 IPC and previous user tests cannot mix')
        test_user_processes=True
    if test_user_syscall_deny:
        test_user_syscall = True
    if test_user_syscall:
        if arch != 'x86_64': raise ValueError('772 hardware syscall profile is x86-64 only')
        test_user_processes = True
    if test_native_tea_session:
        test_native_tea_disk = True
    if test_native_tea_disk:
        test_native_tea = True
    if test_native_tea and arch != 'x86_64':
        raise ValueError('native .tea CPL3 test is x86-64 only')
    if test_native_tea:
        test_user_processes = True
    if desktop_v0 and arch != 'x86_64':
        raise ValueError('desktop v0 requires x86-64 UEFI')
    if test_desktop_display and arch != 'x86_64':
        raise ValueError('desktop display check supports only x86-64 UEFI')
    if test_storage and arch != 'aarch64':
        raise ValueError('storage test supports only AArch64 QEMU virt')
    if test_disk_app and arch != 'aarch64':
        raise ValueError('disk app test supports only AArch64 QEMU virt')
    cc = tool('CLANG', ['clang', 'clang-19', 'clang-18'])
    folder = output / arch
    folder.mkdir(parents=True, exist_ok=True)
    if test_native_tea:
        from build_native_tea_768 import build as build_tea
        build_tea(folder)
        if test_native_tea_disk:
            from build_native_tea_769 import build as build_disk_tea
            build_disk_tea(folder)
    common = ['kernel/main.c', 'kernel/boot_info.c', 'kernel/runtime.c',
              'kernel/boot_memory.c', 'kernel/boot_memory_diagnostic.c',
              'kernel/exception.c', 'kernel/boot_pool.c']
    if test_bootpool:
        common.append('kernel/boot_pool_selftest.c')
    if test_tasks:
        common += ['kernel/tasks.c','kernel/tasks_demo.c',
                   f'arch/{arch}/tasks.c',f'arch/{arch}/tasks_context.S']
    if test_user_processes:
        common += ['kernel/process_runtime.c', 'kernel/process_scheduler.c', 'kernel/user_processes.c', 'kernel/user_processes_demo.c',
                   f'arch/{arch}/user_processes.c', f'arch/{arch}/user_processes_entry.S']
        if test_user_syscall:
            common.append('kernel/user_copy_772.c')
        if test_user_ipc:
            common += ['kernel/user_copy_772.c','kernel/user_ipc_773.c',
                       'kernel/window_service.c','kernel/user_session_runtime.c',
                       'kernel/native_session_hw_770.c','kernel/native_window_channel.c']
        if test_native_tea_session:
            common += ['kernel/window_service.c', 'kernel/user_session_runtime.c',
                       'kernel/native_session_hw_770.c']
    if test_native_tea:
        common += ['kernel/native_tea_elf.c']
        if test_native_tea_disk:
            common += ['kernel/native_tea_package.c', 'kernel/native_tea_staging.c', 'kernel/tea_769_demo.c',
                       'arch/x86_64/tea_769_disk_io.c']
        else:
            common += ['kernel/tea_768_demo.c']
    if test_storage:
        common += ['kernel/storage.c', 'kernel/storage_demo.c',
                   'boot/aarch64/storage_fdt.c', 'arch/aarch64/virtio_block.c']
    if test_disk_app:
        common += ['kernel/storage.c', 'arch/aarch64/virtio_block.c',
                   'kernel/disk_app.c', 'kernel/disk_app_demo.c',
                   'boot/aarch64/disk_app_fdt.c', 'arch/aarch64/disk_app.c',
                   'arch/aarch64/disk_app_entry.S']
    if test_desktop_display or desktop_v0 or test_hw_probe_793 or test_pci_probe_794 or test_pci_aperture_795 or test_pci_topology_796 or test_pci_range_799 or test_cache_800 or test_cache_801 or test_cache_803 or test_cache_804:
        common += ['kernel/desktop_display.c', 'boot/x86_64/desktop_display.c']
    if desktop_v0:
        common += ['kernel/process_runtime.c', 'kernel/process_scheduler.c', 'kernel/desktop_process_runtime.c', 'kernel/desktop_vm_stage.c', 'kernel/desktop_frame_owner.c', 'kernel/window_service.c', 'kernel/user_session_runtime.c', 'kernel/native_launch_stage.c', 'kernel/native_window_channel.c', 'kernel/native_desktop_launch.c', 'kernel/native_tea_elf.c', 'kernel/native_tea_package.c', 'kernel/native_tea_staging.c', 'kernel/native_desktop_bridge.c', 'kernel/process_inspector.c', 'kernel/process_view.c', 'kernel/process_api.c', 'kernel/system_apps.c', 'kernel/desktop_sessions.c', 'kernel/desktop_text_app.c', 'kernel/desktop_shell.c', 'kernel/desktop_draw.c',
                   'kernel/desktop_store.c', 'kernel/stylefs.c', 'kernel/desktop_volume_io.c',
                   'kernel/desktop_stylefs.c', 'kernel/desktop_extra_volume.c',
                   'arch/x86_64/desktop_io.c', 'arch/x86_64/desktop_process_arch.c']
    ownership = test_ownership_plan or test_ownership_plan_invalid
    if ownership:
        common += ['kernel/ownership.c', 'kernel/ownership_plan.c',
                   f'arch/{arch}/ownership.c']
    paging = test_paging or test_paging_fault or test_paging_roundtrip or test_frame_grants
    if paging:
        common += ['kernel/paging.c', 'kernel/paging_admission.c',
                   'kernel/paging_experiment.c', f'arch/{arch}/paging.c']
        if test_paging_fault:
            common.append(f'arch/{arch}/paging_fault.S')
    if test_frame_grants:
        common += ['kernel/frame_grant_proof.c', 'kernel/frame_grant_experiment.c',
                   'kernel/desktop_vm_stage.c', 'kernel/desktop_frame_owner.c',
                   'kernel/desktop_grant_bridge.c', 'kernel/kernel_closure_audit.c', 'kernel/kernel_mapping_inventory.c', 'kernel/kernel_dependency_inventory.c', 'kernel/descriptor_state_781.c', 'kernel/tss_ist_stage_782.c', 'kernel/tss_descriptor_783.c', 'kernel/process_runtime.c']
    if test_hw_probe_793:
        common += ['kernel/hardware_probe_793.c', 'boot/x86_64/hardware_probe_793.c']
    if test_acpi_osl_818:
        common.append('kernel/acpi_osl_818.c')
    if test_acpica_bridge_819:
        common.append('kernel/acpica_bridge_819.c')
    if test_acpica_core_821:
        common.append('kernel/acpica_core_821.c')
    if test_acpica_aml_822:
        common.append('kernel/acpica_runtime_822.c')
    if test_acpi_namespace_816:
        common += ['kernel/acpi_madt_806.c','kernel/dmar_808.c','kernel/iommu_platform_810.c',
                   'kernel/pci_requester_812.c','kernel/pci_mcfg_813.c','kernel/pci_rootbridge_814.c',
                   'kernel/acpi_hostbridge_815.c','kernel/acpi_namespace_816.c',
                   'boot/x86_64/pci_acpi_816.c']
    if test_acpi_hostbridge_815:
        common += ['kernel/acpi_madt_806.c','kernel/dmar_808.c','kernel/iommu_platform_810.c',
                   'kernel/pci_requester_812.c','kernel/pci_mcfg_813.c','kernel/pci_rootbridge_814.c',
                   'kernel/acpi_hostbridge_815.c','boot/x86_64/pci_acpi_815.c']
    if test_pci_rootbridge_814:
        common += ['kernel/acpi_madt_806.c','kernel/dmar_808.c','kernel/iommu_platform_810.c',
                   'kernel/pci_requester_812.c','kernel/pci_mcfg_813.c','kernel/pci_rootbridge_814.c',
                   'boot/x86_64/pci_rootbridge_814.c']
    if test_pci_mcfg_813:
        common += ['kernel/acpi_madt_806.c','kernel/dmar_808.c','kernel/iommu_platform_810.c',
                   'kernel/pci_requester_812.c','kernel/pci_mcfg_813.c',
                   'boot/x86_64/pci_mcfg_813.c']
    if test_pci_requester_812:
        common += ['kernel/acpi_madt_806.c','kernel/dmar_808.c','kernel/iommu_platform_810.c',
                   'kernel/pci_requester_812.c','boot/x86_64/pci_requester_812.c']
    if test_pci_requester_811:
        common += ['kernel/acpi_madt_806.c','kernel/dmar_808.c','kernel/iommu_platform_810.c',
                   'kernel/pci_requester_811.c','boot/x86_64/pci_requester_811.c']
    if test_iommu_810:
        common += ['kernel/acpi_madt_806.c','kernel/dmar_808.c','kernel/iommu_platform_810.c', 'boot/x86_64/iommu_platform_810.c']
    if test_vtd_809:
        common += ['kernel/acpi_madt_806.c','kernel/dmar_808.c','boot/x86_64/dmar_808.c',
                   'kernel/vtd_809.c','kernel/cache_pagewalk_801.c','boot/x86_64/vtd_809.c']
    if test_dmar_808:
        common += ['kernel/acpi_madt_806.c','kernel/dmar_808.c','boot/x86_64/dmar_808.c']
    if test_irq_delivery_807:
        common += ['kernel/acpi_madt_806.c','boot/x86_64/acpi_madt_806.c',
                   'kernel/irq_route_807.c','boot/x86_64/irq_delivery_807.c']
    if test_acpi_madt_806:
        common += ['kernel/acpi_madt_806.c', 'boot/x86_64/acpi_madt_806.c']
    if test_irq_apic_805:
        common += ['kernel/irq_apic_805.c', 'boot/x86_64/irq_apic_805.c']
    if test_cache_804:
        common += ['kernel/cache_policy_800.c','kernel/cache_pagewalk_801.c',
                   'kernel/cache_extent_804.c','boot/x86_64/cache_range_804.c']
    if test_cache_803:
        common += ['kernel/cache_policy_800.c','kernel/cache_pagewalk_801.c',
                   'boot/x86_64/cache_sameboot_803.c']
    if test_cache_801:
        common += ['kernel/cache_pagewalk_801.c', 'boot/x86_64/cache_pagewalk_801.c']
    if test_cache_800:
        common += ['kernel/cache_policy_800.c', 'boot/x86_64/cache_probe_800.c']
    if test_pci_range_799:
        common += ['kernel/pci_range_799.c', 'kernel/pci_probe_794.c', 'kernel/pci_topology_796.c',
                   'boot/x86_64/pci_range_799.c']
    if test_pci_topology_796:
        common += ['kernel/hardware_probe_793.c', 'kernel/pci_probe_794.c',
                   'kernel/pci_topology_796.c', 'boot/x86_64/pci_topology_796.c']
    if test_pci_aperture_795:
        common += ['kernel/hardware_probe_793.c', 'kernel/pci_probe_794.c',
                   'kernel/pci_aperture_795.c', 'boot/x86_64/pci_aperture_795.c']
    if test_pci_probe_794:
        common += ['kernel/hardware_probe_793.c', 'kernel/pci_probe_794.c',
                   'boot/x86_64/pci_probe_794.c']
    if test_tss_ltr:
        common.append('kernel/tss_ltr_trial_784.c')
    if test_ist_hw:
        common += ['kernel/ist_guard_plan_785.c', 'kernel/ist_hw_trial_786.c']
    if test_guard_pf:
        common += ['kernel/ist_guard_plan_785.c', 'kernel/guard_pf_trial_787.c']
    if test_double_fault:
        common += ['kernel/ist_guard_plan_785.c', 'kernel/double_fault_trial_788.c']
    if test_root_slots:
        common += ['kernel/ist_guard_plan_785.c', 'kernel/root_minimization_789.c', 'kernel/root_slot_trial_789.c']
    if test_wx_fault:
        common += ['kernel/ist_guard_plan_785.c', 'kernel/owned_low_790.c', 'kernel/wx_fault_trial_791.c']
    if test_owned_low:
        common += ['kernel/ist_guard_plan_785.c', 'kernel/owned_low_790.c', 'kernel/owned_low_trial_790.c']
    if (test_paging_roundtrip or test_frame_grants) and arch != 'x86_64':
        raise ValueError('CR3 roundtrip is x86-64 only')
    if arch == 'x86_64':
        target = 'x86_64-pc-win32-coff'
        sources = common + ['boot/x86_64/exit_boot.c', 'boot/x86_64/uefi.c',
                            'boot/x86_64/memory_map.c',
                            'arch/x86_64/serial.c', 'arch/x86_64/entry.S',
                            'arch/x86_64/exceptions.c', 'arch/x86_64/exception_entry.S']
        cpu_flags = ['-mno-red-zone', '-mno-sse', '-mno-mmx', '-msoft-float']
        arch_id = '1'
    else:
        target = 'aarch64-none-elf'
        sources = common + ['boot/aarch64/platform.c', 'boot/aarch64/start.S',
                            'boot/aarch64/fdt_memory.c',
                            'arch/aarch64/console.c']
        # Early execution has the MMU off. Do not let Clang combine naturally
        # aligned fields into wider unaligned accesses to stack or DTB data.
        cpu_flags = ['-mgeneral-regs-only', '-mno-outline-atomics', '-mstrict-align']
        arch_id = '2'
    objects = []
    flags = ['--target=' + target, '-O2', '-g0', '-ffreestanding',
             '-fno-stack-protector', '-fno-builtin', '-fno-pic', '-fno-pie',
             '-fno-asynchronous-unwind-tables', '-fno-unwind-tables',
             '-Wall', '-Wextra', '-Werror', '-I', str(ROOT / 'include'),
             '-I', str(ROOT / 'boot/x86_64'),
             '-DSTYLEOS_EXPECT_ARCH=' + arch_id,
             '-ffile-prefix-map=' + str(ROOT) + '=.'] + cpu_flags
    if invalid:
        flags.append('-DSTYLEOS_TEST_INVALID_BOOT_INFO=1')
    if invalid_map:
        flags.append('-DSTYLEOS_TEST_INVALID_MEMORY_MAP=1')
    if test_exception:
        flags.append('-DSTYLEOS_TEST_EXCEPTION=1')
    if test_bootpool:
        flags.append('-DSTYLEOS_TEST_BOOTPOOL=1')
    if paging:
        flags.append('-DSTYLEOS_TEST_PAGING=1')
    if test_paging_fault:
        flags.append('-DSTYLEOS_TEST_PAGING_FAULT=1')
    if test_paging_roundtrip or test_frame_grants:
        flags.append('-DSTYLEOS_TEST_PAGING_ROUNDTRIP=1')
    if test_frame_grants:
        flags.append('-DSTYLEOS_TEST_FRAME_GRANTS=1')
    if test_tss_ltr:
        flags.append('-DSTYLEOS_TEST_TSS_LTR_784=1')
    if test_ist_hw:
        flags.append('-DSTYLEOS_TEST_IST_HW_786=1')
    if test_guard_pf:
        flags.append('-DSTYLEOS_TEST_GUARD_PF_787=1')
    if test_double_fault:
        flags.append('-DSTYLEOS_TEST_DOUBLE_FAULT_788=1')
    if test_root_slots:
        flags.append('-DSTYLEOS_TEST_ROOT_SLOTS_789=1')
    if test_hw_probe_793:
        flags.append('-DSTYLEOS_TEST_HW_PROBE_793=1')
    if test_acpi_namespace_816:
        flags.append('-DSTYLEOS_TEST_ACPI_NAMESPACE_816=1')
    if test_acpi_osl_818:
        flags.append('-DSTYLEOS_TEST_ACPI_OSL_818=1')
    if test_acpica_bridge_819:
        flags.append('-DSTYLEOS_TEST_ACPICA_BRIDGE_819=1')
    if test_acpica_aml_822:
        flags.append('-DSTYLEOS_TEST_ACPICA_AML_822=1')
    if test_acpica_core_821:
        flags += ['-DSTYLEOS_TEST_ACPICA_CORE_821=1', '-D_GNU_EFI', '-I', str(Path(acpica_core_include).resolve())]
    if test_acpi_hostbridge_815:
        flags.append('-DSTYLEOS_TEST_ACPI_HOSTBRIDGE_815=1')
    if test_pci_rootbridge_814:
        flags.append('-DSTYLEOS_TEST_PCI_ROOTBRIDGE_814=1')
    if test_pci_mcfg_813:
        flags.append('-DSTYLEOS_TEST_PCI_MCFG_813=1')
    if test_pci_requester_812:
        flags.append('-DSTYLEOS_TEST_PCI_REQUESTER_812=1')
    if test_pci_requester_811:
        flags.append('-DSTYLEOS_TEST_PCI_REQUESTER_811=1')
    if test_iommu_810:
        flags.append('-DSTYLEOS_TEST_IOMMU_810=1')
    if test_vtd_809:
        flags.append('-DSTYLEOS_TEST_VTD_809=1')
    if test_dmar_808:
        flags.append('-DSTYLEOS_TEST_DMAR_808=1')
    if test_irq_delivery_807:
        flags.append('-DSTYLEOS_TEST_IRQ_DELIVERY_807=1')
    if test_acpi_madt_806:
        flags.append('-DSTYLEOS_TEST_ACPI_MADT_806=1')
    if test_irq_apic_805:
        flags.append('-DSTYLEOS_TEST_IRQ_APIC_805=1')
    if test_cache_804:
        flags.append('-DSTYLEOS_TEST_CACHE_804=1')
    if test_cache_803:
        flags.append('-DSTYLEOS_TEST_CACHE_803=1')
    if test_cache_801:
        flags.append('-DSTYLEOS_TEST_CACHE_801=1')
    if test_cache_800:
        flags.append('-DSTYLEOS_TEST_CACHE_800=1')
    if test_pci_range_799:
        flags.append('-DSTYLEOS_TEST_PCI_RANGE_799=1')
    if test_pci_topology_796:
        flags.append('-DSTYLEOS_TEST_PCI_TOPOLOGY_796=1')
    if test_pci_aperture_795:
        flags.append('-DSTYLEOS_TEST_PCI_APERTURE_795=1')
    if test_pci_probe_794:
        flags.append('-DSTYLEOS_TEST_PCI_PROBE_794=1')
    if test_wx_fault:
        flags.append('-DSTYLEOS_TEST_WX_FAULT_791=1')
        if test_wx_exec: flags.append('-DSTYLEOS_TEST_WX_EXEC_791=1')
    if test_owned_low:
        flags.append('-DSTYLEOS_TEST_OWNED_LOW_790=1')
    if ownership:
        flags.append('-DSTYLEOS_TEST_OWNERSHIP=1')
    if test_ownership_plan_invalid:
        flags.append('-DSTYLEOS_TEST_OWNERSHIP_INVALID=1')
    if test_tasks:
        flags.append('-DSTYLEOS_TEST_TASKS=1')
    if test_user_processes:
        flags.append('-DSTYLEOS_TEST_USER_PROCESSES=1')
    if test_user_syscall:
        flags.append('-DSTYLEOS_TEST_USER_SYSCALL=1')
    if test_user_syscall_deny:
        flags.append('-DSTYLEOS_TEST_USER_SYSCALL_DENY=1')
    if test_user_ipc:
        flags.append('-DSTYLEOS_TEST_USER_IPC_773=1')
    if test_user_ipc_deny:
        flags.append('-DSTYLEOS_TEST_USER_IPC_773_DENY=1')
    if test_native_tea_disk:
        flags.append('-DSTYLEOS_TEST_NATIVE_TEA_DISK=1')
    if test_native_tea_session:
        flags.append('-DSTYLEOS_TEST_NATIVE_TEA_SESSION=1')
    if test_native_tea:
        flags.append('-DSTYLEOS_TEST_NATIVE_TEA=1')
        flags += ['-I', str(folder)]
    if test_storage:
        flags.append('-DSTYLEOS_TEST_STORAGE=1')
    if test_disk_app:
        flags.append('-DSTYLEOS_TEST_DISK_APP=1')
    if test_desktop_display:
        flags.append('-DSTYLEOS_TEST_DESKTOP_DISPLAY=1')
    if desktop_v0:
        flags.append('-DSTYLEOS_DESKTOP_V0=1')
    if update_build is not None:
        vendor = ROOT / 'third_party/monocypher'
        public = Path(update_public_key).read_bytes()
        if len(public) != 32 or not any(public):
            raise ValueError('expected a 32-byte raw Ed25519 public key')
        (folder / 'styleos_update_key.h').write_text(
            'const unsigned char styleos_update_key[32] = {' +
            ','.join(str(x) for x in public) + '};\n')
        flags += ['-DSTYLEOS_DESKTOP_UPDATE=1', f'-DSTYLEOS_UPDATE_BUILD={update_build}',
                  '-I', str(folder), '-I', str(vendor)]
        if test_update_fail:
            flags.append('-DSTYLEOS_TEST_UPDATE_FAIL=1')
        sources += ['kernel/system_update.c', 'kernel/desktop_import.c', 'boot/x86_64/desktop_update.c',
                    'third_party/monocypher/monocypher.c',
                    'third_party/monocypher/monocypher-ed25519.c']
    for rel in sources:
        source = ROOT / rel
        obj = folder / (rel.replace('/', '_').rsplit('.', 1)[0] + '.o')
        extra = ['-std=c11'] if source.suffix == '.c' else []
        run([cc, *flags, *extra, '-c', source, '-o', obj])
        objects.append(obj)
    if arch == 'x86_64':
        linker = tool('LLD_LINK', ['lld-link', 'lld-link-19', 'lld-link-18'])
        binary = folder / 'BOOTX64.EFI'
        map_options = ['/map:' + str(folder / 'kernel.map')]
        run([linker, '/subsystem:efi_application', '/entry:efi_main',
             '/machine:x64', '/nodefaultlib', '/timestamp:0', '/fixed:no',
             '/dynamicbase', '/nxcompat', '/out:' + str(binary), *map_options,
             *(['/opt:ref'] if test_acpica_core_821 else []),
             *objects, *([str(Path(acpica_core_lib).resolve())] if test_acpica_core_821 else [])])
        run([sys.executable, ROOT / 'tools/make_disk.py', '--efi', binary,
             '--output', folder / 'styleos.img'])
        artifacts = [binary, folder / 'styleos.img']
    else:
        linker = tool('LD_LLD', ['ld.lld', 'ld.lld-19', 'ld.lld-18'])
        base = [linker, '-T', ROOT / 'boot/aarch64/linker.ld', '--build-id=none']
        run([*base, '-o', folder / 'kernel.elf', *objects])
        run([*base, '--oformat=binary', '-o', folder / 'styleos.bin', *objects])
        artifacts = [folder / 'kernel.elf', folder / 'styleos.bin']
    # Derive full-span placement from the linked image, never from a UART PASS.
    from pool_site import describe_pool
    pool_site = describe_pool(arch, artifacts[0],
                              folder / 'kernel.map' if arch == 'x86_64' else None,
                              folder / 'styleos.bin' if arch == 'aarch64' else None)
    if arch == 'x86_64':
        pool_site['disk_sha256'] = hashlib.sha256(
            (folder / 'styleos.img').read_bytes()).hexdigest()
    (folder / 'pool-site.json').write_text(json.dumps(pool_site, indent=2) + '\n')
    if test_tasks:
        from tasks_site import describe_tasks
        (folder / 'task-compiler.json').write_text(json.dumps({
            'flags': flags, 'sources': sources},indent=2)+'\n')
        site = describe_tasks(arch, artifacts[0],
            folder / 'kernel.map' if arch == 'x86_64' else None,
            folder / 'styleos.bin' if arch == 'aarch64' else None,
            compiler_flags=flags)
        if arch == 'x86_64':
            site['disk_sha256'] = hashlib.sha256(
                (folder / 'styleos.img').read_bytes()).hexdigest()
        (folder / 'tasks-site.json').write_text(json.dumps(site,indent=2)+'\n')
    if test_storage:
        from storage_site import describe_storage
        (folder / 'storage-compiler.json').write_text(json.dumps({
            'flags': flags, 'sources': sources}, indent=2) + '\n')
        site = describe_storage(arch, artifacts[0], raw_path=folder / 'styleos.bin',
                                compiler_flags=flags)
        (folder / 'storage-site.json').write_text(json.dumps(site, indent=2) + '\n')
    if test_disk_app:
        from disk_app_site import describe_disk_app
        (folder / 'disk-app-compiler.json').write_text(json.dumps({
            'flags': flags, 'sources': sources}, indent=2) + '\n')
        site = describe_disk_app(arch, artifacts[0], raw_path=folder / 'styleos.bin',
                                compiler_flags=flags)
        (folder / 'disk-app-site.json').write_text(json.dumps(site, indent=2) + '\n')
    if test_user_processes and not test_native_tea:
        from user_processes_site import describe_user_processes
        (folder / 'user-processes-compiler.json').write_text(json.dumps({
            'flags': flags, 'sources': sources}, indent=2) + '\n')
        site = describe_user_processes(arch, artifacts[0],
            folder / 'kernel.map' if arch == 'x86_64' else None,
            folder / 'styleos.bin' if arch == 'aarch64' else None,
            compiler_flags=flags)
        if arch == 'x86_64':
            site['disk_sha256'] = hashlib.sha256(
                (folder / 'styleos.img').read_bytes()).hexdigest()
        (folder / 'user-processes-site.json').write_text(json.dumps(site, indent=2) + '\n')
    if ownership:
        from ownership_site import describe_ownership
        site = describe_ownership(arch, artifacts[0],
            folder / 'kernel.map' if arch == 'x86_64' else None,
            folder / 'styleos.bin' if arch == 'aarch64' else None,
            invalid=test_ownership_plan_invalid)
        if arch == 'x86_64':
            site['disk_sha256'] = hashlib.sha256(
                (folder / 'styleos.img').read_bytes()).hexdigest()
        (folder / 'ownership-site.json').write_text(json.dumps(site, indent=2) + '\n')
    if paging:
        from paging_site import describe_paging
        site = describe_paging(arch, artifacts[0],
            folder / 'kernel.map' if arch == 'x86_64' else None,
            folder / 'styleos.bin' if arch == 'aarch64' else None,
            fault=test_paging_fault)
        if arch == 'x86_64':
            site['disk_sha256'] = hashlib.sha256(
                (folder / 'styleos.img').read_bytes()).hexdigest()
        (folder / 'paging-site.json').write_text(json.dumps(site, indent=2) + '\n')
    if test_exception:
        from fault_site import describe_fault
        site = describe_fault(arch, artifacts[0],
                              folder / 'kernel.map' if arch == 'x86_64' else None)
        site['binary_sha256'] = hashlib.sha256(artifacts[0].read_bytes()).hexdigest()
        if arch == 'aarch64':
            raw = (folder / 'styleos.bin').read_bytes()
            offset = site['image_offset']
            if raw[offset:offset + 4].hex() != site['instruction_hex']:
                raise ValueError('ELF fault symbol does not match raw ARM64 Image')
            site['raw_image_sha256'] = hashlib.sha256(raw).hexdigest()
        else:
            site['disk_sha256'] = hashlib.sha256((folder / 'styleos.img').read_bytes()).hexdigest()
        (folder / 'fault-site.json').write_text(json.dumps(site, indent=2) + '\n')
    return [{'path': str(p.relative_to(output)), 'size': p.stat().st_size,
             'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in artifacts]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', choices=['all', 'x86_64', 'aarch64'], default='all')
    parser.add_argument('--output', type=Path,
                        help='Default build/; dedicated test profiles use separate subdirectories')
    negative = parser.add_mutually_exclusive_group()
    negative.add_argument('--invalid-boot-info', action='store_true',
                        help='Negative test only: corrupt the handoff magic')
    negative.add_argument('--invalid-memory-map', action='store_true',
                          help='Negative test only: truncate the map input')
    negative.add_argument('--test-exception', action='store_true',
                          help='Dedicated image only: BRK / UD2 after valid handoff')
    negative.add_argument('--test-bootpool', action='store_true',
                          help='Dedicated image only: image-owned page pool write/read test')
    negative.add_argument('--test-paging', action='store_true',
                          help='Dedicated image only: owned translation tables and alias RW')
    negative.add_argument('--test-paging-fault', action='store_true',
                          help='Dedicated image only: owned tables and real unmapped read')
    negative.add_argument('--test-paging-roundtrip', action='store_true',
                          help='Dedicated x86-64 test only: prove new CR3 and restore inherited CR3')
    negative.add_argument('--test-hw-probe-793', action='store_true',
                          help='793 dedicated terminal UEFI GOP/memory and CPU cache-state observation')
    negative.add_argument('--test-acpica-aml-822', action='store_true',
                        help='822 isolated real ACPICA on synthetic DSDT/SSDT, deny all hardware')
    negative.add_argument('--test-acpica-core-821', action='store_true',
                        help='821 actual upstream ACPICA core checksum executed on real ACPI DSDT; NO AML')
    parser.add_argument('--acpica-core-lib', type=Path)
    parser.add_argument('--acpica-core-include', type=Path)
    negative.add_argument('--test-acpica-bridge-819', action='store_true',
                        help='819 test-only ACPICA-style callbacks and validated table-copy maps')
    negative.add_argument('--test-acpi-osl-818', action='store_true',
                          help='Isolated ACPICA OSL operation-region deny-by-default guest')
    negative.add_argument('--test-acpi-namespace-816', action='store_true',
                          help='816 separate terminal bounded AML literal+SSDT reader')
    negative.add_argument('--test-acpi-hostbridge-815', action='store_true',
                          help='815 terminal FADT/DSDT static AML hostbridge inventory, no eval')
    negative.add_argument('--test-pci-rootbridge-814', action='store_true',
                          help='814 isolated UEFI PCI Root Bridge I/O resource census')
    negative.add_argument('--test-pci-mcfg-813', action='store_true',
                          help='813 isolated read-only ACPI MCFG ECAM segment audit')
    negative.add_argument('--test-pci-requester-812', action='store_true',
                          help='812 bounded bridges, multi-hop DMAR and AMD IVHD audit')
    negative.add_argument('--test-pci-requester-811', action='store_true',
                          help='811 terminal bounded PCI requester ownership scope audit')
    negative.add_argument('--test-iommu-810', action='store_true',
                          help='810 terminal Intel DMAR / AMD IVRS vendor-neutral inventory')
    negative.add_argument('--test-vtd-809', action='store_true',
                          help='809 isolated read-only Intel VT-d DRHD register observation')
    negative.add_argument('--test-dmar-808', action='store_true',
                          help='isolated read-only ACPI DMAR/IOMMU discovery')
    negative.add_argument('--test-irq-delivery-807', action='store_true',
                          help='807 isolated genuine PIT->IOAPIC->LAPIC interrupt')
    negative.add_argument('--test-acpi-madt-806', action='store_true',
                          help='806 isolated read-only ACPI XSDT MADT IOAPIC topology')
    negative.add_argument('--test-irq-apic-805', action='store_true',
                          help='805 terminal read-only CPU APIC, IDTR and PIC snapshot')
    negative.add_argument('--test-cache-804', action='store_true',
                          help='804 terminal full-known-extent page/MTRR/PAT evidence')
    negative.add_argument('--test-cache-803', action='store_true',
                          help='803 terminal same-boot PAT/MTRR/PTE MMIO sampling')
    negative.add_argument('--test-cache-801', action='store_true',
                          help='801 terminal read-only observation of PAT/PTE MMIO translations')
    negative.add_argument('--test-cache-800', action='store_true',
                          help='800 terminal QEMU read-only PAT and variable MTRR evidence')
    negative.add_argument('--test-pci-range-799', action='store_true',
                          help='799 terminal reversible QEMU downstream PCI BAR measurement')
    negative.add_argument('--test-pci-topology-796', action='store_true',
                          help='796 isolated read-only PCI hierarchy and BAR-base census')
    negative.add_argument('--test-pci-aperture-795', action='store_true',
                          help='795 separate terminal reversible PCI BAR aperture sizing')
    negative.add_argument('--test-pci-probe-794', action='store_true',
                          help='794 terminal read-only PCI bus-zero BAR base correlation')
    negative.add_argument('--test-wx-write-791', action='store_true',
                          help='791 isolated hardware supervisor RX write protection fault')
    negative.add_argument('--test-wx-exec-791', action='store_true',
                          help='791 isolated hardware NX instruction fetch protection fault')
    negative.add_argument('--test-owned-low-790', action='store_true',
                          help='790 terminal true detached low 4KiB page tables and PE RX/RW/NX fault chain')
    negative.add_argument('--test-root-slots-789', action='store_true',
                          help='789 terminal two-slot PML4 plus genuine nested #DF hardware test')
    negative.add_argument('--test-double-fault-788', action='store_true',
                          help='788 terminal QEMU/OVMF genuine double-fault via IST3')
    negative.add_argument('--test-guard-pf-787', action='store_true',
                          help='787 isolated terminal hardware #BP IST1 -> guard #PF IST2')
    negative.add_argument('--test-ist-hw-786', action='store_true',
                          help='786 isolated terminal QEMU/OVMF CR3 + IST1 #BP test')
    negative.add_argument('--test-tss-ltr-784', action='store_true',
                          help='784 terminal x86 QEMU-only LGDT/LTR/STR test (no firmware return)')
    negative.add_argument('--test-frame-grants', action='store_true',
                          help='Dedicated x86-64 hardware proof of image-owned frame PA grants')
    negative.add_argument('--test-ownership-plan', action='store_true',
                          help='Dedicated image only: read-only ownership plan, GRANTED=0')
    negative.add_argument('--test-ownership-plan-invalid', action='store_true',
                          help='Dedicated image only: refuse an unproven mandatory claim')
    negative.add_argument('--test-tasks', action='store_true',
                          help='Dedicated image only: two cooperative integer kernel tasks')
    negative.add_argument('--test-user-processes', action='store_true',
                          help='Dedicated image only: CPL3/EL0 fault containment experiment')
    negative.add_argument('--test-user-syscall', action='store_true',
                          help='772 x86 CPL3 hardware user-copy trap positive proof')
    negative.add_argument('--test-user-ipc', action='store_true',
                          help='773 x86 CPL3 returning INT80 kernel-to-user window IPC guest')
    negative.add_argument('--test-user-ipc-deny', action='store_true',
                          help='773 deny foreign reply pointer before consuming window event')
    negative.add_argument('--test-user-syscall-deny', action='store_true',
                          help='772 x86 CPL3 hardware foreign-page refusal proof')
    negative.add_argument('--test-native-tea-session', action='store_true',
                          help='Hardware test only: real disk .tea PID/root bound to isolated session/window')
    negative.add_argument('--test-native-tea-disk', action='store_true',
                          help='Dedicated x86 CPL3 probe: guest reads canonical .tea from external read-only IDE')
    negative.add_argument('--test-native-tea', action='store_true',
                          help='Dedicated x86 CPL3 probe: ELF from a canonical build-verified .tea')
    negative.add_argument('--test-storage', action='store_true',
                          help='Dedicated AArch64 image only: persistent test file on VirtIO MMIO')
    negative.add_argument('--test-disk-app', action='store_true',
                          help='Dedicated AArch64 image only: load external disk app and execute EL0')
    negative.add_argument('--test-desktop-display', action='store_true',
                          help='Dedicated x86-64 UEFI image: framebuffer proof only')
    negative.add_argument('--desktop-v0', action='store_true',
                          help='Experimental interactive x86-64 UEFI desktop image')
    parser.add_argument('--update-build', type=int, help='Guest update build number')
    parser.add_argument('--update-public-key', type=Path, help='Raw Ed25519 verification key')
    parser.add_argument('--test-update-fail', action='store_true', help='Trial intentionally never becomes ready')
    args = parser.parse_args()
    if args.update_build is not None:
        if not (0 < args.update_build < 2**32 and args.desktop_v0 and args.arch == 'x86_64'
                and args.update_public_key and args.output):
            parser.error('--update-build needs x86-64 desktop, public key and dedicated --output')
        if args.output.resolve() in {(ROOT / 'build').resolve(), (ROOT / 'build/desktop-v0').resolve()}:
            parser.error('update builds must use a separate output folder')
    elif args.update_public_key or args.test_update_fail:
        parser.error('update key and test require --update-build')
    if (args.test_user_ipc or args.test_user_ipc_deny) and args.arch != 'x86_64':
        parser.error('773 returning IPC profile is x86-64 only')
    if (args.test_user_ipc or args.test_user_ipc_deny) and (args.test_user_syscall or args.test_user_syscall_deny):
        parser.error('772 and 773 trap profiles must be separate')
    if (args.test_user_syscall or args.test_user_syscall_deny) and args.arch != 'x86_64':
        parser.error('772 syscall profile is x86-64 only')
    if args.test_native_tea_session and args.arch != 'x86_64':
        parser.error('--test-native-tea-session requires --arch x86_64')
    if args.test_native_tea_disk and args.arch != 'x86_64':
        parser.error('--test-native-tea-disk requires --arch x86_64')
    if args.test_native_tea and args.arch != 'x86_64':
        parser.error('--test-native-tea requires --arch x86_64')
    if args.test_storage and args.arch != 'aarch64':
        parser.error('--test-storage requires --arch aarch64')
    if args.test_disk_app and args.arch != 'aarch64':
        parser.error('--test-disk-app requires --arch aarch64')
    if (args.test_paging_roundtrip or args.test_frame_grants or args.test_tss_ltr_784 or args.test_ist_hw_786 or args.test_guard_pf_787 or args.test_double_fault_788 or args.test_root_slots_789 or args.test_owned_low_790 or args.test_wx_write_791 or args.test_wx_exec_791) and args.arch != 'x86_64':
        parser.error('--test-paging-roundtrip requires --arch x86_64')
    if args.test_hw_probe_793 and args.arch != 'x86_64':
        parser.error('--test-hw-probe-793 requires --arch x86_64')
    if args.test_acpica_aml_822 and args.arch != 'x86_64':
        parser.error('--test-acpica-aml-822 requires x86_64')
    if args.test_acpica_core_821 and args.arch != 'x86_64':
        parser.error('--test-acpica-core-821 requires x86_64')
    if (args.test_acpica_core_821 or args.test_acpica_aml_822) and (not args.acpica_core_lib or not args.acpica_core_include):
        parser.error('--test-acpica-core-821 needs verified library and include directory')
    if args.test_acpica_bridge_819 and args.arch != 'x86_64':
        parser.error('--test-acpica-bridge-819 requires x86_64')
    if args.test_acpi_osl_818 and args.arch != 'x86_64':
        parser.error('--test-acpi-osl-818 requires x86_64')
    if args.test_acpi_namespace_816 and args.arch != 'x86_64':
        parser.error('--test-acpi-namespace-816 requires x86_64')
    if args.test_acpi_hostbridge_815 and args.arch != 'x86_64':
        parser.error('--test-acpi-hostbridge-815 requires x86_64')
    if args.test_pci_rootbridge_814 and args.arch != 'x86_64':
        parser.error('--test-pci-rootbridge-814 requires x86_64')
    if args.test_pci_mcfg_813 and args.arch != 'x86_64':
        parser.error('--test-pci-mcfg-813 requires x86_64')
    if args.test_pci_requester_812 and args.arch != 'x86_64':
        parser.error('--test-pci-requester-812 requires x86_64')
    if args.test_pci_requester_811 and args.arch != 'x86_64':
        parser.error('--test-pci-requester-811 requires x86_64')
    if args.test_iommu_810 and args.arch != 'x86_64':
        parser.error('--test-iommu-810 requires x86_64')
    if args.test_vtd_809 and args.arch != 'x86_64':
        parser.error('--test-vtd-809 requires x86_64')
    if args.test_dmar_808 and args.arch != 'x86_64':
        parser.error('--test-dmar-808 requires x86_64')
    if args.test_irq_delivery_807 and args.arch != 'x86_64':
        parser.error('--test-irq-delivery-807 requires x86_64')
    if args.test_acpi_madt_806 and args.arch != 'x86_64':
        parser.error('--test-acpi-madt-806 requires x86_64')
    if args.test_irq_apic_805 and args.arch != 'x86_64':
        parser.error('--test-irq-apic-805 requires x86_64')
    if args.test_cache_804 and args.arch != 'x86_64':
        parser.error('--test-cache-804 requires x86_64')
    if args.test_cache_803 and args.arch != 'x86_64':
        parser.error('--test-cache-803 requires x86_64')
    if args.test_cache_801 and args.arch != 'x86_64':
        parser.error('--test-cache-801 requires x86_64')
    if args.test_cache_800 and args.arch != 'x86_64':
        parser.error('--test-cache-800 requires x86_64')
    if args.test_pci_range_799 and args.arch != 'x86_64':
        parser.error('--test-pci-range-799 requires --arch x86_64')
    if args.test_pci_topology_796 and args.arch != 'x86_64':
        parser.error('--test-pci-topology-796 requires --arch x86_64')
    if args.test_pci_aperture_795 and args.arch != 'x86_64':
        parser.error('--test-pci-aperture-795 requires --arch x86_64')
    if args.test_pci_probe_794 and args.arch != 'x86_64':
        parser.error('--test-pci-probe-794 requires --arch x86_64')
    if args.test_desktop_display and args.arch != 'x86_64':
        parser.error('--test-desktop-display requires --arch x86_64')
    if args.desktop_v0 and args.arch != 'x86_64':
        parser.error('--desktop-v0 requires --arch x86_64')
    output = (args.output or ROOT / 'build' /
              ('exception' if args.test_exception else
               'bootpool' if args.test_bootpool else
               'paging' if args.test_paging else
               'paging-fault' if args.test_paging_fault else
               'paging-roundtrip' if args.test_paging_roundtrip else
               'acpica-aml-822' if args.test_acpica_aml_822 else
               'acpica-core-821' if args.test_acpica_core_821 else
               'acpica-bridge-819' if args.test_acpica_bridge_819 else
               'acpi-osl-818' if args.test_acpi_osl_818 else
               'acpi-namespace-816' if args.test_acpi_namespace_816 else
               'acpi-hostbridge-815' if args.test_acpi_hostbridge_815 else
               'pci-rootbridge-814' if args.test_pci_rootbridge_814 else
               'pci-mcfg-813' if args.test_pci_mcfg_813 else
               'pci-requester-812' if args.test_pci_requester_812 else
               'pci-requester-811' if args.test_pci_requester_811 else
               'iommu-810' if args.test_iommu_810 else
               'vtd-809' if args.test_vtd_809 else
               'dmar-808' if args.test_dmar_808 else
               'irq-delivery-807' if args.test_irq_delivery_807 else
               'acpi-madt-806' if args.test_acpi_madt_806 else
               'irq-apic-805' if args.test_irq_apic_805 else
               'cache-804' if args.test_cache_804 else
               'cache-803' if args.test_cache_803 else
               'cache-801' if args.test_cache_801 else
               'cache-800' if args.test_cache_800 else
               'hw-probe-793' if args.test_hw_probe_793 else
               'pci-range-799' if args.test_pci_range_799 else
               'pci-topology-796' if args.test_pci_topology_796 else
               'pci-aperture-795' if args.test_pci_aperture_795 else
               'pci-probe-794' if args.test_pci_probe_794 else
               'wx-exec-791' if args.test_wx_exec_791 else
               'wx-write-791' if args.test_wx_write_791 else
               'owned-low-790' if args.test_owned_low_790 else
               'root-slots-789' if args.test_root_slots_789 else
               'double-fault-788' if args.test_double_fault_788 else
               'guard-pf-787' if args.test_guard_pf_787 else
               'ist-hw-786' if args.test_ist_hw_786 else
               'tss-ltr-784' if args.test_tss_ltr_784 else
               'frame-grants' if args.test_frame_grants else
               'ownership' if args.test_ownership_plan else
               'ownership-invalid' if args.test_ownership_plan_invalid else
               'tasks' if args.test_tasks else
               'user-ipc-deny' if args.test_user_ipc_deny else
               'user-ipc' if args.test_user_ipc else
               'user-syscall-deny' if args.test_user_syscall_deny else
               'user-syscall' if args.test_user_syscall else
               'user-processes' if args.test_user_processes else
               'native-tea-session' if args.test_native_tea_session else
               'native-tea-disk' if args.test_native_tea_disk else
               'native-tea' if args.test_native_tea else
               'storage' if args.test_storage else
               'disk-app' if args.test_disk_app else
               'desktop-v0' if args.desktop_v0 else
               'desktop-display' if args.test_desktop_display else '')).resolve()
    if (args.test_acpica_aml_822 or args.test_acpica_core_821 or args.test_acpica_bridge_819 or args.test_acpi_osl_818 or args.test_acpi_namespace_816 or args.test_acpi_hostbridge_815 or args.test_pci_rootbridge_814 or args.test_pci_mcfg_813 or args.test_pci_requester_812 or args.test_pci_requester_811 or args.test_iommu_810 or args.test_vtd_809 or args.test_dmar_808 or args.test_irq_delivery_807 or args.test_acpi_madt_806 or args.test_irq_apic_805 or args.test_cache_804 or args.test_cache_803 or args.test_cache_801 or args.test_cache_800 or args.test_pci_range_799 or args.test_pci_topology_796 or args.test_pci_aperture_795 or args.test_pci_probe_794 or args.test_hw_probe_793 or args.test_exception or args.test_bootpool or args.test_paging or
            args.test_paging_fault or args.test_paging_roundtrip or args.test_frame_grants or args.test_tss_ltr_784 or args.test_ist_hw_786 or args.test_guard_pf_787 or args.test_double_fault_788 or args.test_root_slots_789 or args.test_owned_low_790 or args.test_wx_write_791 or args.test_wx_exec_791 or args.test_ownership_plan or
            args.test_ownership_plan_invalid or args.test_tasks or
            args.test_user_processes or args.test_user_syscall or args.test_user_syscall_deny or args.test_user_ipc or args.test_user_ipc_deny or args.test_native_tea or args.test_native_tea_disk or args.test_native_tea_session or args.test_storage or args.test_disk_app or
            args.test_desktop_display or args.desktop_v0) and output == (ROOT / 'build').resolve():
        parser.error('dedicated tests need a separate directory, e.g. build/bootpool')
    arches = ['x86_64', 'aarch64'] if args.arch == 'all' else [args.arch]
    records = []
    for arch in arches:
        records.extend(build(arch, output, args.invalid_boot_info,
                             args.invalid_memory_map, args.test_exception,
                             args.test_bootpool, args.test_paging,
                             args.test_paging_fault, args.test_ownership_plan,
                             args.test_ownership_plan_invalid, args.test_tasks,
                             args.test_user_processes, args.test_storage,
                             args.test_disk_app, **({'test_paging_roundtrip': True} if args.test_paging_roundtrip else {}),
                             **({'test_frame_grants': True} if args.test_frame_grants else {}),
                             **({'test_tss_ltr': True} if args.test_tss_ltr_784 else {}),
                             **({'test_ist_hw': True} if args.test_ist_hw_786 else {}),
                             **({'test_guard_pf': True} if args.test_guard_pf_787 else {}),
                             **({'test_double_fault': True} if args.test_double_fault_788 else {}),
                             **({'test_root_slots': True} if args.test_root_slots_789 else {}),
                             **({'test_wx_fault': True, 'test_wx_exec': args.test_wx_exec_791} if (args.test_wx_write_791 or args.test_wx_exec_791) else {}),
                             **({'test_owned_low': True} if args.test_owned_low_790 else {}),
                             **({'test_hw_probe_793': True} if args.test_hw_probe_793 else {}),
                             **({'test_pci_probe_794': True} if args.test_pci_probe_794 else {}),
                             **({'test_pci_aperture_795': True} if args.test_pci_aperture_795 else {}),
                             **({'test_pci_topology_796': True} if args.test_pci_topology_796 else {}),
                             **({'test_pci_range_799': True} if args.test_pci_range_799 else {}),
                             **({'test_acpi_osl_818': True} if args.test_acpi_osl_818 else {}),
                             **({'test_acpica_core_821': args.test_acpica_core_821, 'test_acpica_aml_822': args.test_acpica_aml_822, 'acpica_core_lib': args.acpica_core_lib, 'acpica_core_include': args.acpica_core_include} if args.test_acpica_core_821 or args.test_acpica_aml_822 else {}),
                             **({'test_acpica_bridge_819': True} if args.test_acpica_bridge_819 else {}),
                             **({'test_acpi_namespace_816': True} if args.test_acpi_namespace_816 else {}),
                             **({'test_acpi_hostbridge_815': True} if args.test_acpi_hostbridge_815 else {}),
                             **({'test_pci_rootbridge_814': True} if args.test_pci_rootbridge_814 else {}),
                             **({'test_pci_mcfg_813': True} if args.test_pci_mcfg_813 else {}),
                             **({'test_pci_requester_812': True} if args.test_pci_requester_812 else {}),
                             **({'test_pci_requester_811': True} if args.test_pci_requester_811 else {}),
                             **({'test_iommu_810': True} if args.test_iommu_810 else {}),
                             **({'test_vtd_809': True} if args.test_vtd_809 else {}),
                             **({'test_dmar_808': True} if args.test_dmar_808 else {}),
                             **({'test_irq_delivery_807': True} if args.test_irq_delivery_807 else {}),
                             **({'test_acpi_madt_806': True} if args.test_acpi_madt_806 else {}),
                             **({'test_irq_apic_805': True} if args.test_irq_apic_805 else {}),
                             **({'test_cache_804': True} if args.test_cache_804 else {}),
                             **({'test_cache_803': True} if args.test_cache_803 else {}),
                             **({'test_cache_801': True} if args.test_cache_801 else {}),
                             **({'test_cache_800': True} if args.test_cache_800 else {}),
                             **({'test_user_ipc': True} if args.test_user_ipc else {}),
                             **({'test_user_ipc_deny': True} if args.test_user_ipc_deny else {}),
                             **({'test_user_syscall': True} if args.test_user_syscall else {}),
                             **({'test_user_syscall_deny': True} if args.test_user_syscall_deny else {}),
                             **({'test_native_tea': True} if args.test_native_tea else {}),
                             **({'test_native_tea_disk': True} if args.test_native_tea_disk else {}),
                             **({'test_native_tea_session': True} if args.test_native_tea_session else {}),
                             **({'test_desktop_display': True}
                                                   if args.test_desktop_display else {}),
                             **({'desktop_v0': True} if args.desktop_v0 else {}),
                             **({'update_build': args.update_build, 'update_public_key': args.update_public_key,
                                 'test_update_fail': args.test_update_fail} if args.update_build is not None else {})))
    cc = tool('CLANG', ['clang', 'clang-19', 'clang-18'])
    version = subprocess.check_output([cc, '--version'], text=True).splitlines()[0]
    (output / 'manifest.json').write_text(json.dumps({
        'prototype': 'StyleOS guest updates v0.7' if args.update_build is not None else
                     'StyleOS desktop v0' if args.desktop_v0 else
                     'StyleOS desktop display check' if args.test_desktop_display else
                     'StyleOS T-P1.8B' if args.test_disk_app else
                     'StyleOS 770 guest disk .tea with PID/session/window proof' if args.test_native_tea_session else
                     'StyleOS 769 guest disk .tea CPL3 proof' if args.test_native_tea_disk else
                     'StyleOS 768 native .tea hardware proof' if args.test_native_tea else
                     'StyleOS T-P1.8A' if args.test_storage else
                     'StyleOS 773 x86 CPL3 returning window IPC' if (args.test_user_ipc or args.test_user_ipc_deny) else
                     'StyleOS 772 x86 CPL3 trap copy proof' if (args.test_user_syscall or args.test_user_syscall_deny) else
                     'StyleOS T-P1.7' if args.test_user_processes else 'StyleOS T-P1.6',
        **({'update_build': args.update_build, 'test_update_fail': args.test_update_fail}
           if args.update_build is not None else {}),
        'compiler': version,
        'invalid_boot_info': args.invalid_boot_info,
        'invalid_memory_map': args.invalid_memory_map,
        'test_exception': args.test_exception,
        'test_bootpool': args.test_bootpool,
        'test_paging': args.test_paging, 'test_paging_fault': args.test_paging_fault,
        'test_paging_roundtrip': args.test_paging_roundtrip or args.test_frame_grants or args.test_tss_ltr_784 or args.test_ist_hw_786 or args.test_guard_pf_787 or args.test_double_fault_788 or args.test_root_slots_789 or args.test_owned_low_790 or args.test_wx_write_791 or args.test_wx_exec_791,
        'test_wx_write_791': args.test_wx_write_791,
        'test_wx_exec_791': args.test_wx_exec_791,
        'test_tss_ltr_784': args.test_tss_ltr_784,
        'test_ist_hw_786': args.test_ist_hw_786,
        'test_guard_pf_787': args.test_guard_pf_787,
        'test_double_fault_788': args.test_double_fault_788 or args.test_root_slots_789 or args.test_owned_low_790 or args.test_wx_write_791 or args.test_wx_exec_791,
        'test_root_slots_789': args.test_root_slots_789,
        'test_frame_grants': args.test_frame_grants,
        'test_native_tea': args.test_native_tea or args.test_native_tea_disk,
        'test_native_tea_disk': args.test_native_tea_disk or args.test_native_tea_session,
        'test_native_tea_session': args.test_native_tea_session,
        'test_user_syscall': args.test_user_syscall or args.test_user_syscall_deny,
        'test_user_syscall_deny': args.test_user_syscall_deny,
        'test_user_ipc': args.test_user_ipc or args.test_user_ipc_deny,
        'test_user_ipc_deny': args.test_user_ipc_deny,
        'test_ownership_plan': args.test_ownership_plan,
        'test_ownership_plan_invalid': args.test_ownership_plan_invalid,
        'test_tasks': args.test_tasks,
        **({'test_desktop_display': True} if args.test_desktop_display else {}),
        **({'desktop_v0': True} if args.desktop_v0 else {}),
        **({'test_disk_app': True, 'test_storage': False, 'test_user_processes': False,
            'compiler_flags': {arch: json.loads((output / arch / 'disk-app-compiler.json').read_text())['flags']
                               for arch in arches},
            'disk_app_sources': {arch: json.loads((output / arch / 'disk-app-compiler.json').read_text())['sources']
                                 for arch in arches}} if args.test_disk_app else {}),
        **({'test_storage': True, 'test_user_processes': False,
            'compiler_flags': {arch: json.loads((output / arch / 'storage-compiler.json').read_text())['flags']
                               for arch in arches},
            'storage_sources': {arch: json.loads((output / arch / 'storage-compiler.json').read_text())['sources']
                                for arch in arches}} if args.test_storage else {}),
        **({'test_user_processes': True,
            'compiler_flags': {arch: json.loads((output / arch / 'user-processes-compiler.json').read_text())['flags']
                               for arch in arches},
            'user_processes_sources': {arch: json.loads((output / arch / 'user-processes-compiler.json').read_text())['sources']
                                      for arch in arches}} if args.test_user_processes else {}),
        **({'compiler_flags': {arch: json.loads((output / arch / 'task-compiler.json').read_text())['flags']
                              for arch in arches},
            'task_sources': {arch: json.loads((output / arch / 'task-compiler.json').read_text())['sources']
                             for arch in arches}} if args.test_tasks else {}),
        'artifacts': records
    }, indent=2) + '\n')
    print('BUILD PASS:', ', '.join(arches))


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, ValueError, OSError, subprocess.CalledProcessError) as error:
        print('BUILD FAIL:', error, file=sys.stderr)
        sys.exit(1)
