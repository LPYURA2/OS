# Recovery 730 — actual second IDE data volume

Base commit: `782583f8cc015f0255e93e265b0a493ef1548191`. This step extends the same canonical source branch from Build 709.

The ATA driver explicitly selects secondary IDE master (Data-1, QEMU index=2) or slave (Data-2, index=3) for IDENTIFY, read, write and flush. An extra kernel storage service owns a separate StyleFS volume, workspace and mapping, mounts the second disk without automatic formatting or writes, and checks a real root file entry. A QEMU guest test makes independent disposable formatted StyleFS v2 images, boots the unchanged desktop, verifies both mounts and the second file lookup, then attaches a foreign Data-2 image and proves it remains untouched. Existing Data-1 and ISO byte hashes are compared.

**Scope:** a second independent service mount, not a navigable Explorer volume, not an installer, not integrated file copy UI. No changes to `kernel/desktop_shell.c`, `kernel/desktop_draw.c`, palette or approved UI. The simulated test topology is not yet the final installed system/data disk contract.

Native apps are exclusively `.tea` by owner decision. Legacy `.sapp` is *not* a native alternative. Full user-mode process isolation is pending and must not be claimed from F5 session PIDs.
