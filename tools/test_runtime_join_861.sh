#!/usr/bin/env bash
set -euo pipefail
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
service=(runtime_entry_858 runtime_tick_860 desktop_process_runtime process_scheduler process_runtime window_service user_session_runtime native_window_channel)
roots=(runtime_roots_861 desktop_gop_850 desktop_root_850 desktop_display pci_aperture_795 cache_policy_800 cache_pagewalk_801 apic_root_842 paging user_root_846 physical_arena_844)
for cc in gcc clang; do
  args=(-std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-pie -no-pie -ffunction-sections -fdata-sections -Wl,--gc-sections -DSTYLEOS_TEST_RUNTIME_TICK_860 -DSTYLEOS_TEST_RUNTIME_JOIN_861 -Iinclude)
  src=(); for f in "${service[@]}" "${roots[@]}"; do src+=("kernel/$f.c"); done
  "$cc" "${args[@]}" tests/test_runtime_combined_861.c "${src[@]}" -o "$out/combined"
  "$out/combined"
  src=(); for f in "${service[@]}"; do src+=("kernel/$f.c"); done
  "$cc" "${args[@]}" tests/test_runtime_join_861.c "${src[@]}" -o "$out/join"
  "$out/join"
  src=(); for f in "${roots[@]}"; do src+=("kernel/$f.c"); done
  "$cc" "${args[@]}" tests/test_runtime_roots_861.c "${src[@]}" -o "$out/roots"
  "$out/roots"
done
