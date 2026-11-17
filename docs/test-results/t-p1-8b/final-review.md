# T-P1.8B independent final review

**PASS — no blocking defect found** in the reviewed core format, guest storage-to-EL0 path, AArch64 adapter, fixture safety, binary proof, strict trace parser or the supplied three-VM integration evidence.

This is an independent read-only review of the source hashes in `final-review.json`. No QEMU process, repository write, Git mutation or remote operation was performed. Host test artifacts were written only under `final-audit-tests/`. The initial integration evidence was produced from the frozen but uncommitted source. The appended final audit also verifies the clean implementation pin and its fresh one-command run.

## Source and architecture findings

- `kernel/disk_app.c:21-72` checks complete little-endian header/length/ISA/entry/reserved/CRC/code bounds and every bit of the seven-instruction grammar before publishing info or copying. Copy capacity, alignment, pointer wrap and source/destination/info overlaps refuse atomically. Header fields never select VA/PA. Only marker and exit immediates vary; this is a narrow experimental container, not a production ABI.
- The app is assembled and linked separately (`apps/disk_demo.S`, `apps/disk_demo.ld`, `tools/build_disk_app.py`). The dedicated kernel source list excludes those files, and the ELF proof refuses external app labels or unrelated experimental resources in the boot image. The independently reconstructed app ELF code and container bytes agree with the separate raw/app.bin artifacts.
- `kernel/disk_app_demo.c:91-148` proves the finite owned writable intervals and their RESERVED/Image/disjoint closure before their initialization. Read-only callbacks at lines 169-178 reject WRITE/FLUSH; after the existing transport's one-outstanding request completions, lines 186-205 permanently close reads and verify equal bounded raw avail/used/request/read counts, zero WRITE/FLUSH and stable queue indices. There is no subsequent VirtIO call; no VirtIO page is present in either root.
- `arch/aarch64/disk_app.c:121-174` and the independent Python table walker verify exact reachable raw descriptors and zero unused reserve pages. SAFE has only privileged Image/PL011 leaves; APP adds exactly the fixed user RX code and user RW/NX stack. All other user/fault/device leaves are absent. The privileged Image mapping intentionally remains RWX; this review makes no kernel W^X claim.
- `arch/aarch64/disk_app.c:177-275` verifies the initial MMU/cache-off profile, code-copy visibility (`DSB SY; IC IALLU; DSB SY; ISB`) and the hardware SAFE root/control readbacks. `disk_app_entry.S:9-110` preserves the trusted ABI frame, enters EL0 by ERET, and captures all X0-X30 plus SP/PC/SPSR/ESR/FAR/root/vector before C handling. Only lower-EL AArch64 vector 8 reaches the successful capture path; unexpected privileged exceptions remain terminal diagnostics.
- Capture/return at `arch/aarch64/disk_app.c:284-353` binds exact raw EL0 frames to the APP root and trusted entry-stack frame. SAFE is installed before restoring the original dispatcher stack; finish checks that stack before clearing architecture pointers. `kernel/disk_app_demo.c:215-265` creates a fresh cleared context/stack for arg0 and arg1 and clears the slot only after SAFE/original-stack observations. The fault PC is never resumed.
- New build/profile guards are confined to `--test-disk-app` AArch64 with separate output and mutual exclusion. Git byte reads confirmed all reviewed old storage/transport/boot-format sources, headers, old storage runner/proof and root README remain exactly at the accepted base; hashes are in JSON.

## Independent evidence checks

The reviewed run is `/workspace/OS/build/disk-app-integration-runs/run-20261117T080836.953155Z-djdouyo1` (16/16 recorded PASS).

- Independently re-derived boot ELF/Image/site/manifest and both separate app ELFs/raw/container artifacts; all match saved provenance and current source SHA snapshots. Boot ELF SHA256 is `f65312e68d97689405c0074e58902bc116544e53805efe3a9656936ac0ed2ce3`; raw Image SHA256 is `924a6e934d5b564e9e4a587e3698254185cec7cb3a00cb4071a112ce857b29b6`.
- Parsed all three raw 1-MiB fixture disks independently of the runner. Superblock, metadata, payload and commit CRCs/fields are valid in the unchanged 1.8A format; each disk contains exactly one generation-1 `app.bin` record. The second slot and all remaining unused sectors are zero. Actual current disk identity/SHA equals initial, pre-VM and post-VM state.
- Reparsed all raw UART files and matched saved trace JSON, UART/boot/manifest hashes and intentional-stop lifecycle fields. Both accepted variants prove the complete 4096-byte copied code page (28 code bytes plus zero tail), 14 reads=requests=avail=used, zero WRITE/FLUSH, complete SAFE/APP root closure, fault ESR `0x92000006` at PC `0x10000008`, release under SAFE, a fresh arg1 launch, then SVC ESR `0x56000002` at saved PC `0x1000001c`. Variant A returns X19 `0x1818a001`/X0 `41`; B returns X19 `0x1818b002`/X0 `73` using the same boot bytes. Three PIDs are distinct.
- The reject fixture has a valid store with an invalid executable magic. It proves the same successful read/drain and then MAGIC refusal with no COPY, root activation, RUN or EVENT evidence.
- Every one of 690 required guest-line omissions was rejected. An additional 31 targeted mutations were rejected, including changed full-page tail bytes, wrong read/used/WRITE counts, wrong syndrome/PC/origin/second launch, reset/error suffixes, PASS-only output and a post-rejection launch.

## Host checks

- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p 'test_disk_app*.py' -v`: **31/31 PASS**, including synthetic process timeout/reset/natural-exit/late-error refusal, binary/manifest mutations, table closure and unavailable-hardlink/Python compatibility cases. This did not invoke QEMU. Full log: `final-audit-tests/python-tests.log`.
- Compiled the actual fixture packer from current source with `cc -std=c11 -O2 -Wall -Wextra -Werror`; **7/7 PASS**: fresh new disk, existing regular file refused unchanged, symlink leaf refused unchanged, symlink parent refused, existing hardlink refused unchanged, symlink app input refused and directory app input refused. No historical or user disk was opened for writing.
- Earlier independent `safety-audit.md/json` provides separate ASan/UBSan format/FDT checks and source admission review; it was read for context and is not counted as a new VM.

## Follow-through and limits

Root performs the old full/ARM-only/storage regression runs. This audit independently re-read all 46 current/baseline artifact pairs and found every byte equal, including storage ELF/Image. Regression execution itself remains outside this audit. This review does not establish Android/Termux, VirtualBox, physical hardware, hostile DMA-device isolation, general allocation, a production ABI or kernel W^X.


## Final clean-pin run audit

**PASS — no blocking finding.** Repeated the complete evidence/raw-disk/parser audit against `/workspace/OS/build/termux-tp18b/run-20261117T132537.438436Z-7mv31hdj`.

- Provenance records exact implementation SHA `0ef43423de1fc4820c7efece9c3da286879d7b49`, `git_dirty=false`, Python `3.14.10`, Clang/LLD `19.1.7`, QEMU `10.0.13`, and actual Linux host. `run.json` records **16/16 PASS, exit 0**. This path's “termux” name does not change the Linux host result into an Android result.
- Every source SHA in provenance equals both the initially reviewed frozen sources and exact `git show` bytes at the implementation pin. Source-manifest SHA256 (canonical sorted JSON object) is `79a34668a6b792b232963ad66a619ad4937703578fa775cc94fdc8a36af2ce12`. Provenance JSON SHA256 is `5060edc93696c47a374a46b5559a49299dfa3b7448e172d12db229438eb9a044`.
- Boot ELF/Image and independent apps re-derive correctly; boot hashes equal the initial integration run's hashes above. Revalidated three distinct QEMU process identities, intentional halt/stop lifecycle, full raw UART-to-JSON binding, complete code page/table proof, exact fault then fresh exit frames, and byte-identical boot artifacts across replacements.
- All three current raw disks again equal initial/pre/post hashes and identities; their unchanged-format storage structures/CRCs each contain only the matching single `app.bin`. Bad-magic storage remains valid and rejects before COPY/EL0. Repeated **690/690 required-line omission refusals** and **31/31 targeted mutation refusals** on the final actual UART files.
- Independently re-read both sides of all **46** pairs listed in `artifact-comparison.json`; all bytes and saved baseline/current SHA256 values match, including T-P1.8A storage ELF/Image. This verifies the saved comparison, without claiming to have independently rebuilt or rerun the old suites.

| Final VM | File SHA256 | Disk SHA256 | UART SHA256 |
|---|---|---|---|
| variant-a | `db7a90ae66b980e1c1c42e73a06844e6ad54b6f55df25f172dfac219fc8b7417` | `3973cc1666932c8fdd53a85cd7b7651b0d670f2c703c4fb3aade0b1053046c5c` | `1dc4cf17b185d6ade2bd22d44889c0eb08421b09bdd0511b71680e04c891c41a` |
| variant-b | `a4a88ccded6d8c2fd3e17b14b4c535735f62b5ed449e54cc37563e97a44dfd8f` | `7ade1344c6df622352be130602ce38244fdc6ddec85ddade320828a009c261c1` | `78b432c95cf0f1a00a153d801dc59a78cceef8fb12d65537be10aad204f8d79b` |
| reject | `13f36e5a73cc5adf124da66590b5d302a3936317eb6b0dda54c0d665fa96ea73` | `e440ace92c912a3d8b39ecff51331a220e8a38360f6e6fcb11bff9d5186fc90c` | `fdf28f586788bcc8a97ff2076e1027b6934722dbda0f7d58ddf77642c7397d24` |
