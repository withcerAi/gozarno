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
  RTL layouts. Language changes take effect immediately without reconnecting.
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

For Persian: Settings → General → Language → فارسی. The interface updates immediately. The saved language is independent of the Windows display language.
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

## Client connection protection (development preview)

Settings → General now includes tunnel checks (default on), automatic reconnect
(default on), and a Windows kill switch (opt-in, applies to the next connection).
All three operate on the client with the provider's existing protocol/server.
No provider agent, server API, or server configuration changes are required.

Every 15 seconds the client opens TCP connections to two numeric addresses on
port 443, binding both the source address and Windows interface to the VPN.
One successful endpoint marks the tunnel healthy. Three failed rounds indicate
an interruption, update the main badge, and request OpenConnect transport recovery
with the current session cookie. Recovery is paced at 30, 60, 120, 240 and at most
300 seconds. Low traffic/idle sessions alone never trigger recovery. Checks may
be disabled or addresses changed for providers that restrict the default
1.1.1.1 / 8.8.8.8 endpoints. The check establishes TCP reachability; it does not
guarantee every application, DNS server, or destination works.

After an established session terminates, up to three fresh attempts use the same
saved profile (5, 10, 20 seconds). Manual disconnect/quit stops recovery; explicit
authentication cancellation and access errors stop further automatic attempts.
Fresh authentication still respects provider OTP/password/certificate prompts.
The automatic-reconnect preference controls the added watchdog/fresh-session
retries; OpenConnect's existing protocol-level reconnection remains in place.

Kill switch uses a dedicated, temporary Windows Filtering Platform sublayer with
atomic transactions, outbound IPv4/IPv6 block rules, and narrow permits for local
loopback, the active tunnel interface, and this executable reaching resolved VPN
gateway addresses. It activates before Connected is reported. It requires admin
rights and refuses to claim protection if filtering fails. Direct traffic rules
are blocked while enabled. System-proxy profiles are refused with a clear message.
Protection remains through transport recovery, worker exit and fresh retries;
manual Disconnect / Release protection releases it after tunnel cleanup. Quit
also releases it. Rules belong to a dynamic session: Windows removes them when
the client process exits, including crashes. This is not permanent crash/reboot
protection. No machine firewall defaults or persistent firewall rules are changed.

DNS outside the tunnel is deliberately blocked while protected. If a provider
changes gateway addresses or requires new DNS during a reconnect, recovery may
fail safely and require an explicit disconnect/reconnect. DHCP/network changes
may also require manual recovery. These behaviors need provider/VM validation.

Build/regression tests cover policy thresholds, reset/backoff, disabled recovery,
monitor cancellation and invalid gateway rejection without installing WFP rules.
Real-provider reconnects, OTP flows, Windows packet/DNS/IPv6 leak tests, sleep,
network changes and interaction with other security products remain unverified.

## Other limits

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

## Connection recovery and public IP (2026-10-08)

The client now coalesces Windows resume and physical-network/default-route changes,
checks tunnel TCP and provider DNS separately, and distinguishes local network,
adapter, DNS, authentication and transport failures. Approved fallback profiles
are ordered per primary profile, use their own credentials and traffic policy,
and do not loop. Authentication failures stop automatic switching.

Diagnostic export contains only bounded structured allowlisted events. It excludes
raw logs, credentials, profile/provider names, addresses and user paths. The raw
connection log remains separate and should not be treated as sanitized.

Public-IP lookup uses https://api.country.is/?fields=city with verified TLS 1.2+,
no proxy, bounded responses, and locally bundled country flags. Startup lookup
uses a default interface; connected lookup binds the socket to the VPN interface
and fails closed. Results are memory-only and excluded from diagnostic reports.
Requests occur at startup, connection and manual refresh, with a ten-second
refresh cooldown. Missing/blocked service or missing tunnel access shows Unknown;
the saved pre-connection IP is not overwritten by the VPN exit IP. IP geography
is approximate; it is neither GPS nor the provider gateway/tunnel's private IP.

Country.is states no key, no cost, no daily/monthly quota and no request logging,
but enforces 10 requests/second/IP. No hosted service is literally unlimited or
guaranteed reachable from every restricted network. Flags originate from
https://github.com/hampusborgos/country-flags (public domain per upstream README),
converted to bounded 40x30 PNGs and embedded; no flag lookup requests are made.

Real stored-profile validation on 2026-10-08 successfully established a VPN
session and provider DNS resolution. Temporary WFP filter setup, explicit direct
IPv4 TCP denial, explicit denial after removing the test tunnel permit, and
release with restored physical TCP passed. Direct DNS, physical IPv6, tunnel
TCP success and full recovery were unverified in this run. Timeouts are never
reported as proof of blocking. Test filters are owned by a temporary dynamic
session and removed during cancellation/exit. This is socket-level evidence,
not a packet-capture guarantee for every app, protocol or provider.

The validation process exited and the temporary VPN interface was disconnected.
The current client tests pass, including malformed API response rejection,
DNS-response parsing, fallback account separation and diagnostic secret exclusion.
Final verification: 33 tests passed, none failed or skipped. A real Country.is
lookup over the default interface and verified HTTPS passed after explicit user
approval; only success status was printed, with no IP/response persisted. Persian
compact connection and settings previews were visually inspected. Country.is
lookup over an actual connected VPN was not separately exercised in this run.

## Build and UI review

Installer 1.2.2 supports in-place replacement at the registered installation path
and preserves user profiles/settings. Upgrades leave the optional shared-driver
and runtime section unchecked by default. New installs retain prerequisite setup;
the runtime check also recognizes compatible DLL versions when registry inventory
is missing. File-lock preflight checks the client and routing helper before any
replacement, with Retry/Cancel for interactive setup and exit code 2 for silent
setup. It does not forcibly terminate a connected VPN. Actual NSIS preflight was
tested with a locked workspace fixture (blocked before writes) and after release
(passed). Full elevated upgrade of the existing system installation was not run.
The packaging script reads the release number from CMake and rejects stale builds.

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

### Gaming and compact interface (1.3.0)

**Superseded in 1.3.1:** gaming now has only an on/off control. Game selection,
discovery, manual profiles, manual MTU, background-app throttling and server
comparison dialogs are removed from the shipping interface. Legacy stored game
profiles are ignored by both the session and connection routing. The mode uses
the existing connectivity destination automatically and binds measurements to
the active tunnel. If ICMP is filtered, it falls back to TCP port 443 measurement;
MTU discovery requires sufficient verified ICMP evidence and otherwise retains
the negotiated size. Deterioration restores the original MTU. Switching servers
automatically remains suppressed while enabled. Existing traffic rules are kept.
No application scanning, arbitrary upload limit, game endpoint inference or
server-side change is needed. The following describes the earlier 1.3.0 UI.

The main pages now use compact layouts without outer scroll areas. Settings are
grouped into Appearance & startup, Connection & protection, Gaming & routing,
Diagnostics, and Components. Gaming configuration has separate Game & detection,
Measurements & MTU, Background traffic, and Server comparison tabs. Lists and
tables retain scrolling when the user has more entries than fit on screen.

Game discovery scans local Steam, Epic and GOG libraries plus explicitly chosen
folders, with bounded traversal and cancellation. Candidates stay on the device;
the user chooses the executable. Full executable paths identify running selected
applications. No injection or anti-cheat hooks are used. Gaming must be enabled
before connecting to apply application routes; turning it off restores base rules.
Native OpenConnect recovery remains available, while forced server switching is
suppressed during protected gaming sessions.

Optional numeric destinations enable tunnel-pinned ICMP or TCP measurements,
median/p95/jitter and failed-probe summaries, and recent same-network comparisons.
These are probe measurements, not measured game packet loss. Verified IPv4 ICMP
DF responses may inform MTU reduction; no positive evidence retains negotiated
MTU. This is not complete UDP DPLPMTUD. Temporary per-executable Windows ActiveStore
QoS policies limit only selected background uploads. They cannot manage downloads
or other devices; cleanup runs on disable/exit and crash recovery on next launch.

The automated client suite passed after these changes. A separate elevated native
check successfully created and removed a QoS policy for its own test executable
and confirmed an empty recovery journal. Actual game latency improvements and
upload throttling rates have not been measured against a user-provided game server.
Implementation references: [OpenConnect transports](https://www.infradead.org/openconnect/technical.html),
[RFC 8899](https://www.rfc-editor.org/rfc/rfc8899.html),
[RFC 8290](https://www.rfc-editor.org/rfc/rfc8290.html), and
[Windows QoS](https://learn.microsoft.com/en-us/windows-server/networking/technologies/qos/qos-policy-top).

## Isolated SOCKS preview (2026-10-17)

Connection mode now offers a private IPv4 SOCKS5 TCP/UDP stack, loopback by
default, with optional authenticated same-subnet LAN sharing. No OS tunnel,
routes, adapter DNS, system proxy or global kill-switch is used by this mode.
See `socks-mode.md` for build instructions, scope and outstanding real-provider
validation. The lwIP implementation is embedded; it installs no extra service.

## SOCKS diagnostics and live preferences (1.4.1)

SOCKS addresses are individual copy-on-click cards. HTTP/HTTPS proxy requests,
unsupported IPv6 destinations and incorrect proxy authentication produce clear
client notices instead of a silent closed connection. No direct fallback exists.

Language changes retranslate the existing controls and layout direction without
rebuilding the window, resetting form values or reconnecting the VPN. Theme changes
apply immediately, including the connection card in light mode.

A saved-provider SOCKS-only validation on 2026-10-17 returned HTTPS status 200,
with one in-tunnel DNS query and bidirectional private-stack packets. The engine
matched the installed 1.4.0 DLL; a later request reproduced an in-tunnel DNS timeout, subsequently corrected with
retries/fallback. SOCKS now chooses reliable TLS before authentication to avoid
intermittent UDP transport startup failures. No adapter, route, DNS or global firewall changes were made by
this SOCKS test. This is request-level evidence, not a packet-capture guarantee.
