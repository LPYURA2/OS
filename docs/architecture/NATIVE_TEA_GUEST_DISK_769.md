# StyleOS 769: verified native .tea read from a guest disk

Base: stage 768 commit `4e15acc37e7ab09f3cb73f4685ce283f5dd9f9a9`.
Task branch: `feature/styleos-guest-disk-tea-cpl3-769`.

## Actual mechanism

- New **dedicated** `--test-native-tea-disk` x86-64 UEFI/CPL3 test profile.
- Host produces the exact canonical `.tea` using the unchanged 768 fixture;
  writes it **unchanged**, starting at LBA 1, to a 128-sector 64-KiB raw disk.
  Sector 0 has pinned disk magic, version, byte count and SHA-256 of the ZIP.
- EFI contains only a pinned full ZIP digest, expected canonical manifest,
  ELF digest and instruction offsets, **not** ELF or `.tea` bytes.
- After the inherited x86 page/PE range proof, the guest independently reads
  a *separate*, read-only primary IDE disk over legacy ATA PIO. No DMA, IRQ,
  write command, user pointer, dynamic allocation or read of candidate RAM.
- Guest code checks header, ZIP EOCD and exactly two stored members, local
  headers, directory offsets, filenames, nonoverlap, exact manifest, CRC32,
  SHA256 of whole archive and ELF, then calls the strict x64 one-page ELF
  loader into a verified image-owned page. Refusals cannot publish a PID.
- The **same** kernel CPL3 hardware path from stage 768 then registers a real
  PID, runs `YIELD`/resume/`EXIT`, observes an isolated second-context fault
  and proves normal cleanup.
- A second QEMU run attaches a tampered external disk and requires a kernel
  error **before** any guest CPL3 frame or PID registration.

## Limits, security and nonclaims

- Not a general `.tea` installer, filesystem file reader, signed application
  trust chain, production disk service, or independent Desktop scheduler.
  The pin is a test allowlist; it does not authenticate a software publisher.
- Not an ELF runtime/ABI for arbitrary apps; only a one-page static ELF64
  with a fixed entry and existing test `INT 80h` operations. No system calls
  callable by real Desktop applications, preemption or memory accounting.
- The on-guest binary is **disk-sourced**, unlike the 768 boot-embedded ELF.
  This test does **not** establish the complete 767 native launch transaction:
  still no user session/window after the physical program starts.
- All regular Desktop variants, two-volume data behavior, home/profile files
  and application UI stay unchanged. Do not merge to `main` without approval.

## Acceptance

```sh
python3 tools/test_native_tea_769_host.py --output build/tea769-host
python3 tools/build.py --arch x86_64 --test-native-tea-disk --output build/tea769
python3 tools/test_native_tea_769_guest.py --output build/tea769-qemu
```

Guest acceptance requires genuine QEMU/OVMF with the i440fx/IDE channel,
strict original stage 768 CPL3 trace validation, a separate raw package disk
that is NOT embedded in the EFI, and a tampered-disk refusal. No emulator
or fake serial response is a substitute for this.

The next stage integrates actual on-guest package checking with the
`native_launch_stage` 767 PID→owner→session→window transaction and works
through safely restoring the Desktop mapping and independent app lifetime.
