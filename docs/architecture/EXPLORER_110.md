# Stage 110 — Explorer.tea: design intake and real data contract

Accepted base: b82a044a4436e915d4cd2380675728de75a067b4. 108 CI 46141175090, 43/43 SUCCESS;
109 CI 46141401846, 43/43 SUCCESS. Guest 140967926816 confirmed
explicit installation, original removal, same-session and cold CPL3 launch,
retirement and zero cold writes. This is bounded QEMU acceptance, not
production readiness or VirtualBox/physical acceptance.

Owner 28.11.2026: first product apps Explorer and Settings. Explorer reference
is the newly uploaded StyleOS-Explorer-Mockups.pdf (21 pages, 20 screens,
1672×941, SHA256 1dc73831a00ddc8ea582a87c487f5e2575ea8bd8ed6ce53e1198f8be728796bc).
The source remains in the owner's files; no binary in Git. Settings awaits
its separate mockup. The new PDF governs Explorer layout; keep the existing
system palette as default and warm graphite as an alternative. No Desktop
palette change. Mockup imperfections may be corrected with owner agreement.

## Screen mapping
| Screen / PDF page | Actual data / operation |
|---|---|
| 01 / 2 Быстрый доступ | Real pinned directories and actual recent files; empty state when none |
| 02–04 / 3–5 Документы, Проекты, Музыка | Actual user directories, metadata and navigation |
| 05 / 6 Изображения | Actual files and bounded thumbnails when decoding supported |
| 06 / 7 Обновления | Actual local packages and update logs; no auto-install on presence |
| 07 / 8 Мой компьютер | Actual mounted volumes and attached supported devices; capacity/free values from service |
| 08–11 / 9–12 Диск 1, Диск 2, USB, Архив | Actual volume roots and destination folder; no hardcoded device count or name |
| 12 / 13 Поиск | Real cancellable scoped search, name and containing path |
| 13 / 14 Пустая папка | True empty directory; distinct loading, denied, unavailable states |
| 14 / 15 Свойства | Actual path, size, type and available timestamps |
| 15–17 / 16–18 Копирование, отключение, подключение | Real progress, pause/cancel, stable volume identity, explicit retry, source retained |
| 18–20 / 19–21 Конфликт, завершение, обе версии | Preserve both / replace / skip; publish only committed copy; actual resulting directory |

All filenames, folders, sizes, dates, capacities, counts, thumbnails, times,
progress and disk labels in the example are illustrative. No sample content
in product paths. Test fixtures must remain separate. The mockup's special
copy-to-Disk-2 action is contextual, not a hardcoded volume name.

## Implementation boundary
Explorer.tea must be an actual isolated user-mode process using the SDK,
not a new built-in shell screen. Retain existing Explorer path as fallback
until the independent migration passes; no deletion of historical code or
main update. Current sdk/native/include/styleos/native_v1.h exposes file
grants, read/write and stream operations (0x130–0x13f), but not public
volume enumeration, directory enumeration, namespace mutations or search.
Existing trusted live StyleFS navigation and installation are not public
Explorer SDK APIs. Do not bypass the process boundary by linking kernel
internals or passing raw kernel pointers. Do not grant arbitrary applications
Explorer authority.

Proposed implementation order:
1. Audited, versioned scoped volume/directory API backed by the real
   mounted-volume service and StyleFS object snapshots. Enumerate in bounded
   pages; identify mount/generation/object/revision; reject stale or foreign
   handles; distinguish unsupported media from empty directories. Define
   Explorer privilege through trusted process identity and session authority.
   Verify actual CPL3 enumeration, changing contents, denial, stale handles,
   absent volume and resource release. No UI placeholders.
2. Independent Explorer.tea window and real navigation; preserve design layout
   (toolbar, breadcrumbs, search, sidebar, list/grid, footer, properties).
   Use reusable theme tokens; inspect actual screen output at supported sizes.
3. Real create/rename/delete, clipboard, search, properties and actual pinned/
   recent items. Integrate trusted launch/install and update entry points.
   Explicit install; ordinary applications launch without reboot.
4. Real multi-volume topology and bounded streaming operations (including
   100+ GB where supported), cancellation, safe conflict handling and device
   removal/reconnect. Do not advertise USB hotplug before verified driver and
   service support. Preserve source on copy failure; do not expose partial
   destination; retry only after explicit action and revalidation.
5. Integrate generalized Deep Drop contract, verify independence, shell restart,
   error paths and responsiveness, then replace legacy Explorer entry only
   after full acceptance. Settings separately after its mockup.

No runtime code changed in this intake stage. The next engineering increment
is real scoped enumeration. Memory/idle CPU target is not measured here.
