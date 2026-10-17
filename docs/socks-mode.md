# Isolated SOCKS mode: implementation preview

This mode sends SOCKS5 TCP and UDP through a private lwIP IPv4 stack and the
OpenConnect CSTP/TLS transport. It creates no Windows network adapter and never
runs vpnc-script, VpnRoutes, AppRouter, system kill-switch or physical MTU tuning.
Windows routes, adapter DNS and system-proxy settings are not modified.

On Connection, choose **SOCKS proxy · other connections stay direct**, then use
the ordinary provider profile and Connect. The endpoint appears after the tunnel
and listener are ready. Defaults: `127.0.0.1:1080`, loopback only, no authentication.
Configure applications with SOCKS5 and remote/proxy DNS (for example `socks5h`).
The program does not enable the Windows system proxy.

Settings → SOCKS proxy controls the port, LAN checkbox, selected LAN IPv4 address
and separate proxy credentials. LAN requires an 8+ character password and accepts
only peers on the selected adapter subnet. Allow this application's chosen TCP
port and UDP relay traffic on the private LAN in Windows Firewall if necessary.
This preview deliberately does not create firewall exceptions. Listener/port
settings apply to the next connection; they are locked during automatic recovery.

Only IPv4 tunnel addresses and destination connections are implemented in this
preview. IPv6 destinations fail through SOCKS; they never fall back to direct
connections. IPv6 traffic from unrelated applications remains unchanged. The
SOCKS listener itself uses IPv4. Domain resolution uses raw DNS packets over the
private tunnel stack, not Windows DNS; when the server supplies no usable IPv4
DNS resolver, `1.1.1.1` is used inside the tunnel. TCP DNS/truncated-response
fallback is not implemented. An unavailable resolver makes the request fail.

SOCKS network I/O runs in the existing VPN worker thread. The earlier architecture
proposed a separate worker process; no extra executable, service or driver was
introduced for this implementation. lwIP is statically linked into the client.
On tunnel failure, proxy connections do not get a direct fallback. Existing
OpenConnect recovery and approved fresh-session retries are retained; clients
may have to reconnect after transport/session failure. Windows adapter-based
health checks and system protection tests are disabled for SOCKS mode.

A normal VPN session must be stopped before selecting another connection mode.
Changing an already connected full-system VPN can itself change network routes;
the isolation guarantee starts when connecting from the disconnected state in
SOCKS mode. Provider/ISP interruptions and shared-link congestion are independent
of local route isolation; throughput can still affect gaming latency.

## Reproducible build

The unmodified lwIP 2.2.1 source and BSD notice are under `external/lwip`.
`src/client/lwip` supplies the application configuration. CMake compiles it into
a static library. Build instructions in `source-build.md` otherwise apply.

Apply `tools/patches/openconnect-9.12-packet-io.patch` to OpenConnect 9.12 in
addition to the existing MinGW compatibility patch. Rebuild the entire DLL
(clean first, because dependency tracking can be disabled), then install its
library/import library into the local SDK. The private versioned symbol
`gozarno_openconnect_packet_io_v1` must be exported alongside the normal API.
The client resolves it at runtime and refuses SOCKS mode if absent; it never
falls back to an OS tunnel. The packaging script checks this export too.
The private ABI patch guards reconnect scripts and OS-tunnel shutdown as well
as initial setup, and limits packet polling to a bounded 10 ms idle interval.

## Validation

`tests/proxy_tests.cpp` exercises synthetic TCP handshake, upload/download,
SOCKS UDP association/relay, in-tunnel domain resolution, authentication,
unsupported IPv6 failure, occupied ports and release on shutdown. It compares
Windows IPv4/IPv6 routes, DNS servers, physical MTUs and user system-proxy
settings around start/stop while an existing direct TCP connection remains open.
These tests do not require a provider account or install drivers.

The compiled DLL's private ABI was also initialized/freed and checked to leave
its OS interface name unset. This is a smoke check, not a real-provider tunnel.
Real provider interoperability, remote-device LAN access, prolonged gaming,
transport reconnect under packet loss and packet-capture leak validation remain
to be performed before production qualification.

## Setup 1.4.0

The x64 Setup includes the normal VPN, SOCKS mode, runtime libraries and optional
per-application routing prerequisites. The optional packet-filter/runtime section
is unchecked by default; SOCKS does not need it. Existing profiles and settings
are preserved. Quit the existing client normally before upgrading; Setup refuses
to overwrite a running client instead of terminating it.

## 1.4.1 interface and validation

Click an address card to copy only that IP:port. Select SOCKS5 in the consuming
application; this endpoint is not an HTTP/HTTPS proxy. Leave the consuming app's
proxy username/password empty unless a proxy password was explicitly configured.
Enable remote DNS for proxy destinations. Windows proxy fields that configure an
HTTP proxy do not turn it into SOCKS5.

The private --validate-socks report-path diagnostic uses the saved provider in
batch mode, a temporary loopback port and the isolated stack. The report excludes account details,
provider addresses, destination responses and raw logs. Ordinary application logs
remain separate and can contain provider information; profile settings are not saved.
A real-provider HTTPS request succeeded with this mode; the original external-app
failure still requires that application's configuration to reproduce precisely.
Language and theme preferences now update the existing window immediately.

### Reliability correction

A repeated real-provider request reproduced a hostname lookup failure: one DNS
packet was sent and no answer arrived. The resolver now retransmits and then tries
the next provider IPv4 resolver, with a public resolver as a final in-tunnel
fallback. It never asks the Windows resolver for SOCKS destinations. Regression
coverage deliberately drops the first resolver and verifies a later in-tunnel
answer and successful TCP connection while Windows network state stays unchanged.

SOCKS mode selects reliable CSTP/TLS before authentication, avoiding the old path
that could fail during UDP transport setup and then attempt to disable DTLS too
late. Inner SOCKS5 UDP association/relay still works over the TLS tunnel. Normal
VPN transport preferences are unchanged. This trades outer UDP transport for
reliable delivery; proxy traffic can experience TCP head-of-line blocking during
loss. Direct applications continue to use their existing Windows sockets/routes.
