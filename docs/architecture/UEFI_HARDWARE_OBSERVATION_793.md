# StyleOS 793 — UEFI GOP and processor/device observation (terminal only)

## Why this stage exists

792 established that a caller-supplied dependency inventory never authorizes
production CR3. A software manifest is not evidence of GOP physical coverage,
cache semantics or interrupts. 793 adds a **separate terminal EFI** invoked via
`python3 tools/build.py --arch x86_64 --test-hw-probe-793 --output ...`.
The image uses an OVMF GOP handle before `ExitBootServices`, retains only the
validated `desktop_surface` in private image memory, then consumes the real
UEFI descriptor buffer after `ExitBootServices`. The `boot_info v1` ABI stays
byte-identical, and ordinary Desktop takes none of these paths.

## Measurements produced in real QEMU (subject to CI)

- GOP framebuffer base/size, resolution, pitch, pixel format and page-rounded
  address range; independent EFI memory descriptor intersection and contiguous
  coverage measurement; conflicting/overlapping descriptors are rejected.
- Count and type of overlapping descriptors, union of advertised UEFI memory
  attributes, and count of firmware runtime-related descriptors. These are
  firmware-reported *attributes*, not proof of actual CPU memory type.
- Live CPU `CR0`, `CR3`, `RFLAGS`, CPUID MTRR support, raw `IA32_PAT` and
  conditional `IA32_MTRR_DEF_TYPE` MSRs. Do not interpret those alone as
  effective framebuffer WC/UC: PTE/PAT index, fixed/variable MTRRs, aliases,
  firmware attributes and architecture-specific precedence must be audited.
- Legacy PIC mask bytes at I/O ports 0x21/0xA1 and disabled IF. Report COM1,
  PS/2 and ATA as **source-used port addresses**, not a hardware enumeration
  or proof that drivers are operational. PCI BAR enumeration and IRQ routing
  remain unresolved.

The terminal image ends via QEMU ISA debug-exit (code 33), never returns to
UEFI. It does **not** modify page tables, interrupt routes, device caches or
framebuffer contents and never installs a production CR3. Missing GOP, absent
memory map, malformed descriptors and invalid CPU features all fail closed.

## Validation and limits

`tools/test_hardware_probe_793_host.py` runs ASAN/UBSAN and COFF compilation on
the real UEFI descriptor analyzer, including malformed, fragmented, overlapping
and overflow maps. `tools/test_hardware_probe_793_negative.py` rejects forged
UART claims (synthetic positive fixtures are parser tests, never hardware).
`tools/test_hardware_probe_793.py` **requires real QEMU/OVMF**, a genuine
instrumented EFI and an exact guest debug-exit 33; it persists the original UART
and a structured observation report. CI also retains all prior regressions.

Even successful hardware observation does **not** prove effective PAT/MTRR
cache policy, all BARs/MMIO, IRQ routing, runtime services lifecycle, or the
production Desktop address-space closure. **Production CR3 activation remains
disallowed.** Follow-up should explicitly audit actual device BAR ranges,
interrupt routing and firmware runtime services before considering a
production-map integration experiment.

## First QEMU observation (run 45042996026, NOT accepted)

In real OVMF with `-vga std` the GOP aperture was `0x80000000` for
`0x003e8000` bytes (1280x800) but **no corresponding descriptor appeared in
GetMemoryMap** (`overlaps=0`, `covered=0`, 128 EFI descriptors). The original
test failed and timed out because it treated that absence as fatal. This is a
real and important gap, not evidence that the framebuffer does not exist. The
probe was changed to emit a mandatory `GAP` marker while keeping every
production-memory authorization denied. QEMU observation PASS may establish
that the gap was *observed*, never that its missing MMIO coverage is resolved.
The complete physical BAR and cache provenance remain for stage 794.
