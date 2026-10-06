# Desktop UI review — 2026-10-06

| Previous problem | Implemented correction |
|---|---|
| Resized legacy form, large empty center | Connection dashboard with status, server picker, policy summary and session totals |
| Competing menus and old tabs | One rail: Connection, Servers, Activity, Settings; secondary actions under Help & more |
| Everyday fields mixed with advanced options | Account, Authentication and Advanced server-editor tabs |
| Generic routing table | IP/network, domain and executable entry points, input examples and expandable explanation |
| Blank server library | First-server/import guidance, persistent Add server action |
| Six equal-weight buttons | Primary Connect, Edit and Traffic rules; Copy/Remove under More |
| Inconsistent spacing/type/controls | Shared typography, teal accents, consistent cards and visible focus |
| Compact-height overlap/clipping | Scrollable pages preserve readable control sizes |
| Internal executable picker in routine settings | Application-routing setup check, bundled default component |
| Ambiguous inactive controls | Text connection state, selection-dependent actions, gaming unavailable while disconnected |

Screenshots come from the compiled native executable, not a browser mockup.
Fictional profiles use `vpn.example.com`; they are not live sessions. Reviewed
connection, library, activity, settings, add-server, account and routing dialogs.
Full dark layout is 1120 × 830 logical pixels; compact light layout is 940 × 640
with scrolling. A server-selection regression caused by combo placeholder behavior
was fixed in `reload_settings`. Generated evidence is under `build-ui`; final
captures for this revision are under `gozarno-en`, `gozarno-fa`,
`gozarno-fa-light` and `gozarno-fa-compact` when refreshed.

Connected/reconnect/error flows still need a real provider session. Screen-reader,
all keyboard-only paths and 125/150/200-percent monitor scaling have not been
fully qualified. English and Persian UI layouts are implemented and reviewed.

## Gozarno 1.2.0 follow-up

- Replaced the old Activity form with real session totals, sampled rates, a
  60-sample graph, elapsed connection time, addresses and encryption details.
  Empty data has readable states; last-session totals remain after disconnect.
- Explicit checkbox images and a shared table delegate avoid native blue focus,
  disappearing tick borders and accidental checkboxes in the Notes column.
- Hid the legacy status bar so the navigation background reaches the bottom.
- Bounded the log history at 4,096 entries and the visible log at 2,000 entries;
  moved disk writes to a bounded worker queue. Log readers run outside its write
  lock, preventing the direct-reader deadlock. Log UI updates are batched.
- New G branding, uppercase-first wordmark, independent Gozarno settings/setup,
  and migration from prior development names.
- Plain Unicode About avoids UTF-8/Latin-1 corruption of names and punctuation.
- Persian strings include controls, dialogs and application errors; mirrored
  navigation, forms and tables keep technical addresses and file paths LTR.
- Added an on-demand installed-component inventory; no service/driver changes
  or periodic background checks are made by that page.

Validation: 20 QtTest passes, including Persian direction and mixed-text
alignment, checkbox mouse/keyboard input, selection colors, Notes-column flags,
real DPAPI migration, About Unicode, substitution fields, and component checks.
`tools/Validate-Translations.ps1` verifies all extracted application strings and
placeholder fields. Native previews use isolated settings and fictional gateways.
Live provider reconnect, clean-VM installation and long VPN-session resource
measurements remain separate from these local UI and component checks.
