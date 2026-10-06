# Gozarno VPN 1.2.1 — development preview

Independent Windows x64 client based on OpenConnect GUI. All new features run
on the user's device; existing provider servers require no updates or admin access.

## Included

Version 1.2.1 corrects Windows IPv4 MTU writes (gaming mode error 87), including
restoration on disable/disconnect, and supplies distinct high-DPI connection,
disconnect and cancel button icons. Other interface settings are preserved.

- Redesigned desktop shell: connection dashboard, server library, activity,
  settings, dark/light themes and visible keyboard focus.
- English by default; complete application-string Persian catalog and mirrored
  RTL layouts. Language changes take effect after reopening the application.
- Settings → Components & prerequisites shows bundled files and Windows
  driver/runtime versions and status. Checks are read-only and run on demand.
- Search, favorites, notes, duplication and JSON server catalogs. Exports exclude
  usernames, passwords, keys, certificates and tokens.
- Optional saved credentials. Windows DPAPI protects saved passwords for the
  current Windows account; unattended authentication is a separate preference.
- Per-profile IP/CIDR, exact-domain and application VPN/direct rules. Server
  defaults, full tunnel and selected-traffic modes are available.
- Native Windows route management, gateway preservation and recovery journals.
  Domain addresses refresh every 60 seconds while connected.
- Application TCP/UDP routing through authenticated local SOCKS5 bridges and a
  separate ProxiFyre process, with explicit VPN/physical interface binding.
- Gaming mode reduces only the active tunnel MTU, never above its original value.
  Disable/disconnect restores it. No global TCP, physical-adapter or server tweaks.
- New application, toolbar and tray icons; separate settings, adapter names,
  install folder, shortcuts and uninstall entry.
- NSIS Setup bundles Qt, OpenConnect and Wintun, checks OS/architecture and
  optional app-routing prerequisites, and installs missing VC runtime/packet filter.

## Install and use

Run `GozarnoVPN-1.2.1-Setup-x64.exe` on Windows 10/11 x64. Administrator access
is required for tunnel/routing management. ARM64 is unsupported by this build.
App routing requires .NET 4.7.2+, VC++ x64 runtime and Windows Packet Filter.
Supported Windows normally includes .NET; Setup reports missing .NET and directs
the user to Windows Update. The optional app-routing component can be deselected
for core VPN use. Restart if Setup requests it before using app rules.

Most components are private files inside the Gozarno installation: Qt, GnuTLS,
OpenConnect, Wintun and ProxiFyre. Driver registration and shared Windows
runtimes cannot be replaced with ordinary portable files. Setup installs missing
VC runtime and packet-filter prerequisites quietly, without their own shortcuts.
The .NET Framework check remains explicit; supported Windows normally provides
it, and a missing/obsolete framework requires Windows Update before app routing.
Shared drivers/runtimes are preserved on uninstall because other apps may use them.

For Persian: Settings → General → Language → فارسی, then quit using Help & more
and reopen. The saved language is independent of the Windows display language.
Addresses, executable paths, certificate hashes and protocol data preserve LTR
display. Server-provided authentication banners and low-level third-party logs
remain as provided; they are not rewritten by the application.
Previous Arovan/Qivaryn server profiles and opaque DPAPI credentials migrate
without changing the old stores or mixing existing same-name profile credentials.

Add the provider's HTTPS gateway and existing protocol. Configure account details
through server settings. Add traffic rules before connecting. Gaming mode becomes
available while connected. OTP, authentication challenges and provider policy apply.

Installing beside OpenConnect GUI does not replace its files or profiles. Concurrent
VPN tunnels or conflicting rules can still create OS routing conflicts. Gozarno
allows one custom-routing session at a time. Uninstall preserves saved profiles
and shared prerequisites. An incompatible shared packet-filter driver is reported
without replacing another product's driver.

## Limits

- Domain rules operate on resolved IPs, not URL paths. Shared CDN addresses can
  affect other domains. Application-managed encrypted DNS needs integration checks.
- App rules match executable paths; helper/child processes need separate rules.
  TCP/UDP support does not claim raw ICMP, arbitrary kernel traffic or universal
  game/anti-cheat compatibility.
- MTU tuning does not guarantee lower ping; internet/server routing and congestion
  remain relevant. No latency or country claims are fabricated in the UI.
- No production code-signing certificate or automatic Gozarno updater is configured.

## Validation actually performed

Native Release x64 build with GCC 16.2 and Qt 6.11.2 succeeded. Isolated client tests
report **22 passes, zero failures**, covering rule validation, catalog integrity,
secret exclusion, Windows encryption, fragmented SOCKS requests/authentication and
explicit failure of unavailable backends/tunnels. Real executable screenshots were
inspected in dark/light and compact layouts, including account/routing dialogs.
The packaged executable launched offscreen using bundled runtime DLLs.

Not verified yet: clean-VM install/uninstall and coexistence tests, real provider
login, end-to-end app TCP/UDP routing, DNS/IPv6 leaks, disconnect/reconnect recovery,
or gaming latency/packet-loss comparison. No VPN credentials were supplied. No
network driver was installed on the developer machine. This is a development
preview, not completed production qualification.

## Build and UI review

The portable MSYS2 toolchain is under `.tools/msys64`. Dependencies include Qt6
base/scxml, GCC, CMake, Ninja, GnuTLS, libxml2, stoken, lz4, spdlog/fmt and NSIS.
OpenConnect 9.12 is built as an SDK in `.tools/openconnect-prefix`. Configure PATH,
CMAKE_PREFIX_PATH and PKG_CONFIG_PATH for the toolchain/SDK, then:

```
cmake -S . -B build-arovan -G Ninja -DPROJ_USE_SYSTEM_OPENCONNECT=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-arovan
cmake -S tests -B build-tests -G Ninja
cmake --build build-tests
```

OpenConnect's legacy MinGW include patch is supplied in `tools/patches`.
`tools/Prepare-Dependencies.ps1` stages verified dependencies without installing.
`tools/Package-Windows.ps1` assembles runtime DLLs and compiles Setup.
See `source-build.md` for corresponding dependency sources and build instructions,
`third-party.md` for licenses, and `ui-review.md` for UI QA.

UI capture: `--preview-dir <directory> --preview-sample` uses isolated settings and
fictional servers, saves screenshots, then exits. `--preview-light` and
`--preview-compact` exercise other layouts. Never pass `--server` for UI review.
The preview options do not create VPN connections or install drivers.
