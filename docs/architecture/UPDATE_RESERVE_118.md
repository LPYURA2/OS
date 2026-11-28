# StyleOS update reserve — owner requirement, stage 118

Owner 28 November 2026: the system must reserve some space for updates and a saved working version for rollback. This complements the previously agreed update-from-file and rollback flow. Proposed implementation, pending acceptance of concrete sizing/installation format.

## Existing implementation and limitation
- include/styleos/system_update.h defines a legacy fixed 139264-sector disk, UP_SLOT_SECTORS=2048 (1 MiB each), two slots and redundant journal.
- kernel/system_update.c checks signature and digest, stages to inactive slot, verifies readback, records trial and accept/rollback. boot/x86_64/desktop_update.c attempts trial then returns to prior active or base EFI.
- This is not proof that the current full StyleOS kernel/desktop/apps can be upgraded using that format. Its fixed size and EFI payload cap must not be adopted as final installer capacity.

## Target contract
- Installer provisions protected boot/recovery metadata, two system slots (A and B) and a separate persistent data area for user files, settings and installed applications. The slot for the running/last-known-good version is never overwritten during preparation or trial of its successor.
- Capacity planning uses checked 64-bit aligned sizes from actual signed system-image bounds. Each system slot must fit a complete bootable system generation with an explicitly configured growth allowance; journal/recovery and any separate temporary download/staging capacity are counted separately. No fixed percentage or arbitrary GiB amount until measured image sizes and installer constraints are known. 4 GiB RAM / 20 GB VM remains the target, not a guarantee that all future update payloads fit.
- Enforce separation at the block service/partition and allocation layer: normal app and data-volume grants cannot overlap protected system ranges. Do not expose raw update writes or overwrite the active slot. Reserved capacity is not ordinary user-free space; no double subtraction from an already partitioned data volume.
- Verify signature, compatibility, declared lengths, available space and readback before trial activation. If the inactive slot is too small, refuse safely and provide a resizing/recovery path, not an automatic overwrite of the working slot.
- Persist atomic boot selector and trial state with redundant records. Boot candidate, validate required initialization and functional health, then accept. On failed or unconfirmed trial select prior working slot at next boot. Recover from interrupted staging, torn receipt, and power loss. Watchdog/reset behavior must be designed and tested separately.
- After acceptance the former active slot remains the previous generation until the next authorized update overwrites the inactive slot. Explicit rollback of an accepted release requires a validated selector/recovery path. A/B alone is not a permanent backup archive. Preserve data and handle migration compatibility or refuse rollback safely; user data must not be silently rolled back.
- Explorer reports actual mounted volume capacity and actual allocator free space. A future Settings/Updates page may show separately verified reserve capacity and slot status. No fictional disks, no fake reserved/free values.
- Tests before release: 20 GB target layout; larger disks and arithmetic overflow; insufficient room; ordinary app unable to access reserve; invalid signatures; interrupted writes and trial failure; reboot into previous image; data unchanged; accepted rollback/migration; repeat update; recovery when one selector record is damaged. Confirm under QEMU and later real supported hardware.

Stage 118 only records this requirement and corrects diagnostic code size; the protected full-system reserve is NOT yet provisioned or enforced in the current live profile. Implement during installer/update integration after Explorer and Settings foundations; do not weaken the existing StyleFS grant checks.
