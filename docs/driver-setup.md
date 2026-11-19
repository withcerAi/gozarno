# Application routing setup from Gozarno

When application-rule preflight finds a missing or stopped Windows Packet Filter
service, the client offers **Install or repair** and **Not now**, in English or
Persian. Other configuration errors retain their own messages. The same flow is
available from Settings > Check application routing setup.

After the connection worker finishes and the VPN is disconnected, accepting runs
the bundled `GozarnoDriverSetup.exe` with Windows administrator consent. The UI
waits asynchronously. Cancellation keeps the client open. The helper uses the
same NSIS prerequisite instructions as the full installer: check .NET, install
the bundled Visual C++ runtime if necessary, install the signed packet-filter MSI
if absent, or start a compatible existing driver. An incompatible shared driver
is preserved and reported instead of replaced.

The client restarts only after a successful helper exit and a second successful
preflight. The new process waits for the old process to exit before acquiring the
single-instance lock. Shutdown uses the normal cleanup path. No credentials or
auto-connect arguments are passed to the new process; saved profiles are kept.
A required Windows reboot is reported separately; the client does not reboot
Windows automatically. Missing helper files and setup failures keep the client
open with an actionable message.

## Build

Use the existing `tools/Prepare-Dependencies.ps1` and
`tools/Package-Windows.ps1` workflow. Packaging builds the helper before generating
the uninstall inventory, so Setup installs and uninstalls it with the client.
The helper embeds the pinned upstream prerequisite binaries; it needs no network
connection. Keep the existing bundled third-party notices and source materials.

## Validation and remaining checks

The helper and full-Setup scripts were compiled with NSIS 3.13 (the latter with a
minimal staging directory, for script validation only). The two embedded payloads
match the SHA256 values pinned by Prepare-Dependencies.ps1. Persian translation
coverage and placeholder checks pass. The preflight regression test now checks
that a missing ProxiFyre executable and a policy without application rules do not
request driver installation.

The 1.4.2 client and complete installer were subsequently built using the existing
MSYS2/Qt/OpenConnect toolchain. All 43 client tests and 15 proxy tests pass, as does
the packet-I/O ABI smoke test. The 63 packaged third-party EXE/DLL files match the
previous release's staging payload byte for byte. Upstream prerequisite signatures
are valid. The release is a prerelease because clean Windows 10/11 installation
and the following end-to-end driver setup checks remain unverified:

- Missing driver: accept setup, verify service and one client restart.
- Installed but stopped compatible driver: verify it starts without MSI replacement.
- Decline setup or deny UAC: client stays open; no automatic connection.
- Setup failure or incompatible shared driver: show failure, preserve that driver.
- Exit 3010: request a Windows restart without restarting Windows automatically.
- English/Persian prompts, saved profiles, and normal VPN cleanup after restart.
- SOCKS and profiles without application rules do not offer this setup.

No driver was installed on the editing machine during validation.
