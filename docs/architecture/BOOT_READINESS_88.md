# StyleOS boot readiness — owner requirement / implementation boundary

## Goal
An interactive desktop must not depend on starting the first app to activate a mandatory service. Readiness means usable, not just a drawn framebuffer.

## Proposed dependency contract
1. Boot image and platform checks; initialize kernel memory, interrupts, scheduler.
2. Admit available device drivers and system volume; verify filesystem and permission services, IPC and lifecycle management.
3. Initialize mandatory window/compositor/input/application runtime and SDK services (SDK itself is a library, not a resident service). Validate recovery configuration.
4. Login/authorization; prepare selected user's profile and capabilities. No access from inactive/locked sessions.
5. Publish interactive-ready only when mandatory dependencies for that profile and session are satisfied. Optional unavailable services (external volumes, network, optional apps) have explicit status, never silently imply availability.

Critical failures require error state and safe recovery rather than an apparently ready desktop. No implicit formatting or mounting unapproved media. Prefer bounded initialization, avoid unnecessary app autostarts, measure boot duration and idle RAM on target 4 GiB/20 GB VM. Offline functionality remains usable without network.

## Current 88.2 implementation scope
Existing owned-shell live-volume profile only. At first no-event desktop observation, the admitted live StyleFS object broker is reconciled before the interactive S863 READY receipt. File broker READY must occur exactly once if admitted volume is usable; foreign external media yields unavailable boot-state, no broker grant. Existing zero-write and guest tests retained. Earlier generic DESKTOP READY is a display marker, not a complete readiness claim. Later work must define a general mandatory/optional service registry, login and recovery states, user-facing availability and full cold-boot acceptance. Do not claim unimplemented services ready.

## Acceptance
Cold boot with no input/no app, exact ordered receipts, no duplicate init, accepted/foreign or absent external media, no disk writes in RO profile, no privilege escalation. Future complete acceptance covers mandatory-service failures, optional-device absence, repeated cold boot, user login/switch/lock, recovery, measurements and physical/VM platform tests.
