# StyleOS keyboard shortcuts — proposed default contract

Independent implementation, no Microsoft source, icons, manual text or brand assets copied. Use Super as generic system key. Owner requests familiar Windows shortcuts plus StyleOS recovery shortcuts. Reference: https://support.microsoft.com/en-us/accessibility/windows/keyboard-shortcuts-in-windows

| Chord | Behavior | Status |
|---|---|---|
| Ctrl+Shift+Esc | Processes | 68 candidate; real QMP test pending |
| Alt+F4 | Close focused legacy window with dirty guard; native cooperative request | 68 candidate; real QMP test pending |
| F4 / Super | Menu | existing |
| F8 | Processes | existing |
| F10 | Shell view recovery | existing; not a graphics driver reset |
| Alt+Shift | Input language (owner-approved default) | existing; both press orders tested in next CI |
| F6 | Additional language shortcut | existing |
| Super+E | Explorer | planned |
| Super+D | Show desktop / restore | planned |
| Alt+Tab | Switch between visible windows (native keyboard focus and raised surface) | stage 68 candidate; native window activation and visual switcher pending |
| Alt+Shift+Tab | Reverse app switching | planned; avoid conflict with Alt+Shift language switching |
| Super+arrows | Snap/maximize/minimize | planned |
| Super+Space | Optional alternative language shortcut | not assigned by default; future Settings |
| Super+L | Lock session | planned, requires proper authentication |
| Ctrl+C/X/V/A/Z/S | Standard application actions | per-app support, clipboard/undo not yet universal |
| Super+Ctrl+Shift+B | Recover display subsystem | planned, requires graphics recovery mechanism; not a synonym for F10 |
| Ctrl+Alt+F10 | Recovery supervisor command | proposed, separate from ordinary app shortcuts |

Fn is a hardware modifier on many keyboards: firmware may emit F10 when Fn+F10 is pressed. OS binds actual F10 scan code. Do not require Fn.

Reserved recovery commands must bypass app focus and failed shell controller through trusted supervisor; preserve data, no blind device reset or automatic destructive operation. Do not bind unimplemented operations to fake success. Owner decision 2026-11-27: Alt+Shift is the default input-language chord; F6 remains an alternate, Super+Space not enabled by default. Define user-editable bindings in Settings later, keep protected recovery chords separate. Existing PS/2 input supports Ctrl/Shift/Alt; Super chord state and key-up routing need implementation before advertising full Windows-style shortcuts. Alt+F4 must target actual focused native window when ordinary native apps are available; current candidate only handles legacy window focus. Use same stable window capability and unsaved-data policy.

68 native focus extension: Alt+F4 on native focus queues the existing authenticated Alt+F4 key event to the exact focused visible window after controller approval. Full queue, capture, hidden, stale or foreign window refuses; never silently kills the process or closes a legacy window. App must implement graceful handling. Native Alt+Tab cycles visible native keyboard focus; native z-order raise and visual switcher remain separate. Host sanitized regression covers two same-session ports, full queue, wrong target, minimized and locked window and unchanged live process. Actual ordinary installed-app integration is not yet tested.

68 raised surface candidate: explicit compositor-local raised window ID, no moving window registry slots or changing process/channel ownership. Hit testing, drawing and damage share ordered surfaces; reversing order repaints affected rectangles even when frame generation is unchanged. A hidden/foreign/retired window cannot become visible through the raise operation. Host regression covers overlapping real strip-compositor pixels and pointer routing after switch. This is one raised native surface above the remaining registry order, not a full unified legacy/native stacking or task switcher UI.
