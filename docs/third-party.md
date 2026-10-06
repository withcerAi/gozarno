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
| Windows Packet Filter / NDISAPI | 3.6.2.1 installer | https://github.com/wiresock/ndisapi/tree/v3.6.2 ; MIT |
| Wintun signed DLL | 0.14.1 | https://www.wintun.net/ ; bundled binary license licenses/Wintun.txt |
| GnuTLS and transitive runtimes | MSYS2 packages | Included notices under licenses/MSYS2; matching recipes at https://github.com/msys2/MINGW-packages |
| Microsoft VC++ redistributable | 14.44.35211 x64 | Unmodified signed installer; Microsoft redistributable terms |

Local source archives cover Gozarno, patched OpenConnect, Qt Solutions and ProxiFyre.
Before public distribution supply complete matching source/build recipes for the
actual binary set, including required ProxiFyre/native submodules and applicable
MSYS2 library sources. This table alone is not a substitute for source obligations.
Preserve library notices and keep shared Qt DLLs replaceable. The locally built
installer is a development preview and has not been published.

Extra build-tool notices may be present in licenses/MSYS2. Each component retains
its original license; the AGPL routing executable is not described as covered by
Gozarno's GPL-2.0 notice.
