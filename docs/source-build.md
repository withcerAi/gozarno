# Gozarno 1.2.1: corresponding source and build instructions

Download these assets from the same release as Setup:

- `GozarnoVPN-1.2.1-source.zip`: client, tests, resources, installer and scripts.
- `OpenConnect-9.12-patched-source.zip`: OpenConnect v9.12, including the MinGW
  standard-library include correction already applied to `compat.c`.
- `QtSolutions-source.zip`: Qt Solutions commit
  `777e95ba69952f11eaec0adfb0cb987fabcdecb3`, including QtSingleApplication.
- `ProxiFyre-2.6.1-source.zip`: upstream v2.6.1, commit
  `e04e7decd6ce4926580dc3c8d2078e32071a2091`, including `socksify`, `netlib`,
  `ndisapi` and `ndisapi.lib` native projects. These are tracked source directories,
  not missing submodules.
- `GozarnoVPN-1.2.1-dependency-sources.zip`: exact MSYS2 source packages,
  package build metadata, runtime inventory, notices and the deployed VPN script.
- `SHA256SUMS.txt`: checksums of the distributed assets.

The dependency inventory maps each of the 55 packaged MSYS2 EXE/DLL files to the
SHA256 of an exact package file. It also includes spdlog and fmt, whose code is
compiled into the client, and NSIS 3.13 including its installer compression module.
Two split GCC runtime packages share one GCC source
package. Sources are downloaded from `https://repo.msys2.org/mingw/sources/`;
the archive contains the upstream source tarballs, PKGBUILD and local patches,
not merely links to a changing repository. `.PKGINFO` and `.BUILDINFO` record the
original binary package versions and build dependencies where available.

## Building MSYS2 dependencies

Install MSYS2 and use its **MINGW64** shell, matching this x64 build (not MSYS or
UCRT64). Install the build prerequisites:

```sh
pacman -S --needed base-devel git mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
```

For each matching `*.src.tar.zst`, extract it to its own build directory and run
the supplied recipe. For example:

```sh
tar -xf mingw-w64-qt6-base-6.11.2-2.src.tar.zst
cd mingw-w64-qt6-base
MINGW_ARCH=mingw64 makepkg-mingw -s --noconfirm
pacman -U ./mingw-w64-x86_64-qt6-base-6.11.2-2-any.pkg.tar.zst
```

Repeat using each package's own PKGBUILD. Sources and patches are already next
to it. `makepkg-mingw` validates the recipe's source checksums. Build prerequisites
may require a network connection; `.BUILDINFO` documents original versions, while
ordinary `pacman -S` retrieves currently available versions. To reconstruct an
older build environment, obtain the recorded versions from the MSYS2 package
archive instead. GCC's source package supplies both libgcc and libstdc++.
Runtime license exceptions and component licenses remain in their source trees.

## Building OpenConnect and the client

Extract the client into a workspace. The packaging scripts expect MSYS2 under
`.tools/msys64`, the patched OpenConnect source under `.tools/openconnect`, and
its installed SDK under `.tools/openconnect-prefix`. An existing MSYS2 installation
can be used with adjusted paths. Install GnuTLS, libxml2, stoken, lz4, Qt6
base/scxml, spdlog, fmt and NSIS through the matching packages/recipes above.
OpenConnect's autoconf bootstrap also needs autoconf, automake, libtool and gettext.

In the MINGW64 shell, from the patched OpenConnect source:

```sh
autoreconf -fi
mkdir build-local
cd build-local
../configure --prefix="$(pwd)/../../openconnect-prefix" \
  --disable-dependency-tracking --disable-nls --with-gnutls \
  --without-openssl --without-libpskc --without-libproxy \
  --with-vpnc-script=vpnc-script.js
make -j2
make install
```

The source archive already contains the patch: do not apply it again. The patch
file is also available in `tools/patches` for building from pristine v9.12.
From the client workspace in PowerShell:

```powershell
$workspaceRoot = (Get-Location).Path
$env:PATH = "$workspaceRoot/.tools/msys64/mingw64/bin;$workspaceRoot/.tools/msys64/usr/bin;$env:PATH"
$env:CMAKE_PREFIX_PATH = "$workspaceRoot/.tools/msys64/mingw64;$workspaceRoot/.tools/openconnect-prefix"
$env:PKG_CONFIG_PATH = "$workspaceRoot/.tools/openconnect-prefix/lib/pkgconfig;$workspaceRoot/.tools/msys64/mingw64/lib/pkgconfig"
cmake -S . -B build-arovan -G Ninja -DPROJ_USE_SYSTEM_OPENCONNECT=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-arovan --parallel 2
cmake -S tests -B build-tests -G Ninja
cmake --build build-tests --parallel 2
```

Qt Solutions is obtained by the external-project recipe. For an archived-source
build, extract `QtSolutions-source.zip` to `build-arovan/external/src/qt-solutions-master`;
replace that external project's Git download with `DOWNLOAD_COMMAND ""` and
`SOURCE_DIR` pointing to the extracted directory. Its `PATCH_COMMAND` copies the
supplied `CMake/Includes/CMakeLists_qt-solutions.cmake.in` before compilation.
The exact revision is recorded above; a newer `master` is not the matching source.
The deployed `vpnc-script.js` is included as readable source in the dependency
bundle. The external recipe pins upstream revision
`ce9e961bd0f6b867e1c7c35f78f6fb973f6ff101`.

## ProxiFyre and managed dependencies

Use Visual Studio 2022/MSBuild 17, C++ desktop tools and .NET Framework 4.7.2
targeting pack. The complete upstream `.github/workflows/main.yml` supplies the
build procedure. It restores NuGet packages listed in the project `packages.config`
files, installs `ms-gsl:x64-windows` and `boost-pool:x64-windows` through vcpkg, and runs:

```powershell
nuget restore ProxiFyre/packages.config -PackagesDirectory packages -NonInteractive
nuget restore ProxiFyre.Configuration/packages.config -PackagesDirectory packages -NonInteractive
msbuild socksify.sln -t:rebuild -p:Configuration=Release -p:Platform=x64 -p:Version=2.6.1
```

NuGet runtime versions are NLog 5.2.3 (BSD-3-Clause), Topshelf 4.3.0 (Apache-2.0),
and Newtonsoft.Json 13.0.3 (MIT). Their notices accompany the source bundle. The
inventory checks the deployed managed DLLs against those official NuGet packages.
ProxiFyre's own AGPL source/build scripts remain in its separate source archive.
The upstream workflow does not pin its vcpkg checkout; its native dependency
build environment has not been reconstructed for byte-identical output.

## Packaging

`tools/Prepare-Dependencies.ps1` downloads the verified official ProxiFyre payload,
unmodified Wintun signed binary, Windows Packet Filter MSI and Microsoft VC runtime.
It does not install a driver on the build machine. Place rebuilt ProxiFyre runtime
files under `build-dependencies/backend` to package a modified backend instead.
Run `tools/Package-Windows.ps1` to stage the runtime, notices and compile the NSIS
installer from `installer/GozarnoVPN.nsi`. The setup script is included in the client
archive; no proprietary installer authoring tool is needed.

Wintun's prebuilt-binary terms, Microsoft's runtime terms and the packet-filter
EULA are separate from the open-source library licenses. The driver EULA permits
this free/noncommercial distribution; it does not grant commercial redistribution
or access to kernel-driver source. Preserve the unmodified MSI and its EULA.

The source collector and bundle verification ran against the actual 1.2.1 payload.
The client was built and its 22 tests passed. We have not rebuilt every upstream
dependency or demonstrated a bit-for-bit reproducible installer. Signing keys,
personal VPN settings and credentials are not part of these source archives.
