# Licenses and source notices

Gozarno preserves OpenConnect GUI copyright notices and GPL-2.0-or-later source
headers. Modification/commercial distribution must respect applicable licenses,
including notices and corresponding source. Renaming does not remove obligations.

| Component | Version | Source / notices |
|---|---|---|
| OpenConnect GUI foundation | f210cb5 | https://gitlab.com/openconnect/openconnect-gui ; root LICENSE.txt and source headers |
| OpenConnect library | 9.12, MinGW include patch | https://gitlab.com/openconnect/openconnect/-/tree/v9.12 ; LGPL-2.1-or-later notices |
| Qt shared libraries | 6.11.2 | https://code.qt.io/qt/ ; Qt notices under licenses/MSYS2; https://doc.qt.io/qt-6/lgpl.html |
| QtSingleApplication / Qt Solutions | 777e95ba69952f11eaec0adfb0cb987fabcdecb3 | https://code.qt.io/qt-solutions/qt-solutions.git ; component source headers |
| ProxiFyre, separate process | 2.6.1 | https://github.com/wiresock/proxifyre/tree/v2.6.1 ; AGPL-3.0 |
| NDISAPI user-mode wrapper | 3.6.2 | https://github.com/wiresock/ndisapi/tree/v3.6.2 ; MIT (does not license the kernel driver) |
| Windows Packet Filter kernel driver | 3.6.2.1 unmodified MSI | Proprietary freeware for personal/noncommercial and educational use; see `licenses/Windows-Packet-Filter-3.6.2.1-EULA.txt`. Commercial distribution requires the author's prior written permission. |
| Wintun signed DLL | 0.14.1 | https://www.wintun.net/ ; bundled binary license licenses/Wintun.txt |
| GnuTLS and transitive runtimes | MSYS2 packages | Included notices under licenses/MSYS2; matching recipes at https://github.com/msys2/MINGW-packages |
| Microsoft VC++ redistributable | 14.44.35211 x64 | Unmodified signed installer; Microsoft redistributable terms |
| NSIS installer / compression modules | 3.13 | Source and recipe in dependency-source bundle; zlib/bzip2/CPL-1.0 with the NSIS LZMA linking exception; original notice under licenses/MSYS2/nsis |

Release source archives cover Gozarno, patched OpenConnect, Qt Solutions and the
complete ProxiFyre tree (including its tracked native projects). The matching
dependency-source archive supplies exact-version MSYS2 source packages with
upstream source, PKGBUILDs and patches, plus a SHA256 binary-to-package inventory.
See `source-build.md` for build instructions and verification limits. Download
these archives alongside the installer from the same GitHub release; GitHub's
automatic source ZIP alone does not include the third-party dependency sources.

This release is distributed free of charge for noncommercial use. The unmodified
Windows Packet Filter installer retains its embedded EULA; a readable copy also
accompanies this release. The EULA permits inclusion in free/nonprofit packages,
but not distribution to clients by commercial entities without prior written
permission. See https://www.ntkernel.com/windows-packet-filter/licensing/.
Do not describe the proprietary driver as MIT or as having publicly available
kernel source. Wintun's official signed binary uses its bundled prebuilt-binary
license; it is not distributed here under the alternative GPL source license.

Preserve library notices and keep shared Qt DLLs replaceable. Source headers
permit GPL-2.0-or-later; when using Qt modules requiring GPLv3, distribute the
combined client under that later version. The full GPLv3 text is supplied in
`licenses/GPL-3.0.txt`; original GPLv2 notices remain intact. Qt's own notices
identify its applicable licenses and exceptions. This is a development preview.

Extra build-tool notices may be present in licenses/MSYS2. Each component retains
its original license; the AGPL routing executable is not described as covered by
Gozarno's GPL-2.0 notice.
