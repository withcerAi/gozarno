"""Verify and package the frozen release's dependency source, never user settings.

Run Collect-Release-Sources.py --use-cached-sources first. No network access or
package build/installation is performed here. Requires MSYS2 bsdtar and the
official pinned NuGet packages under dist/source-compliance/nuget.
"""
import hashlib
import json
import os
import pathlib
import re
import subprocess
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCES = ROOT / 'dist/sources/native-sources'
TAR = ROOT / '.tools/msys64/usr/bin/bsdtar.exe'
os.environ['PATH'] = str(TAR.parent) + os.pathsep + os.environ.get('PATH', '')


def read_tar(path, name):
    return subprocess.check_output([str(TAR), '-xOf', str(path), name])


def main():
    report = json.loads((SOURCES / 'runtime-inventory.json').read_text())
    verified = []
    metadata = {}
    for source in report['source_packages']:
        archive = SOURCES / source['filename']
        if hashlib.sha256(archive.read_bytes()).hexdigest() != source['sha256']:
            raise RuntimeError('Source archive checksum changed: ' + archive.name)
        entries = subprocess.check_output([str(TAR), '-tf', str(archive)]).decode().splitlines()
        srcinfo = next(n for n in entries if n.endswith('/.SRCINFO'))
        prefix = srcinfo.rsplit('/', 1)[0]
        info = read_tar(archive, srcinfo)
        recipe = read_tar(archive, prefix + '/PKGBUILD')
        fields = {}
        for line in info.decode().splitlines():
            match = re.match(r'\s*(\w+) = (.+)$', line)
            if match:
                fields.setdefault(match[1], []).append(match[2])
        expected = [p for p in report['packages'] if p['name'] in source['packages']]
        version = fields['pkgver'][0] + '-' + fields['pkgrel'][0]
        if any(p['base'] != fields['pkgbase'][0] or p['version'] != version for p in expected):
            raise RuntimeError('Recipe does not match installed package: ' + archive.name)
        # Verify every source with a recorded cryptographic checksum. Git sources
        # and detached signatures with SKIP remain part of the original recipe.
        checked = 0
        for key, urls in fields.items():
            if key != 'source' and not key.startswith('source_'):
                continue
            suffix = key[len('source'):]
            algorithm = next((alg for alg in ('sha256', 'sha512', 'sha1', 'md5')
                              if alg + 'sums' + suffix in fields), None)
            if not algorithm:
                continue
            hashes = fields[algorithm + 'sums' + suffix]
            if len(hashes) != len(urls):
                raise RuntimeError('Source/hash count mismatch: ' + archive.name)
            for url, digest in zip(urls, hashes):
                if digest == 'SKIP':
                    continue
                name = url.split('::', 1)[0] if '::' in url else url.rsplit('/', 1)[-1]
                if 'git+' in url:
                    # makepkg hashes the pinned Git tree as a tar archive, not
                    # the bare repository directory stored in the source package.
                    revision = re.search(r'#(?:commit|tag)=([^&]+)', url)
                    if not revision or prefix + '/' + name + '/' not in entries:
                        raise RuntimeError('Pinned Git source is absent: ' + name)
                    destination = ROOT / 'dist/source-compliance/extracted-git-sources'
                    destination.mkdir(parents=True, exist_ok=True)
                    subprocess.check_call([str(TAR), '-xf', str(archive), '-C', str(destination),
                                           prefix + '/' + name])
                    content = subprocess.check_output(['git', '-c', 'core.abbrev=no', '-C',
                        str(destination / prefix / name), 'archive', '--format', 'tar', revision[1]])
                    if hashlib.new(algorithm, content).hexdigest() != digest:
                        raise RuntimeError('Pinned Git source checksum mismatch: ' + name)
                    checked += 1
                    continue
                if prefix + '/' + name not in entries:
                    raise RuntimeError('Checksummed source is absent: ' + name)
                content = read_tar(archive, prefix + '/' + name)
                if hashlib.new(algorithm, content).hexdigest() != digest:
                    raise RuntimeError('Upstream source checksum mismatch: ' + name)
                checked += 1
        metadata['recipes/' + prefix + '/.SRCINFO'] = info
        metadata['recipes/' + prefix + '/PKGBUILD'] = recipe
        verified.append({'source_package': archive.name, 'version': version,
                         'checksummed_inputs_verified': checked})
        print('Verified source and recipe:', archive.name, flush=True)

    missing_metadata = []
    for package in report['packages']:
        candidates = list((ROOT / '.tools/msys64/var/cache/pacman/pkg').glob(
            package['name'] + '-' + package['version'] + '-*.pkg.tar.*'))
        candidates = [p for p in candidates if not p.name.endswith('.sig')]
        if len(candidates) != 1:
            missing_metadata.append(package['name'])
            continue
        for name in ('.PKGINFO', '.BUILDINFO'):
            metadata['binary-package-metadata/' + package['name'] + '/' + name] = read_tar(candidates[0], name)

    managed = []
    for name, version, dll in (('nlog', '5.2.3', 'NLog.dll'),
                               ('topshelf', '4.3.0', 'Topshelf.dll'),
                               ('newtonsoft.json', '13.0.3', 'Newtonsoft.Json.dll')):
        package = ROOT / 'dist/source-compliance/nuget' / (name + '.' + version + '.nupkg')
        deployed = ROOT / 'dist/privacy-audit/payload-1.2.1/backend' / dll
        digest = hashlib.sha256(deployed.read_bytes()).hexdigest()
        with zipfile.ZipFile(package) as z:
            matches = [n for n in z.namelist() if n.lower().endswith('/' + dll.lower())
                       and hashlib.sha256(z.read(n)).hexdigest() == digest]
            if not matches:
                raise RuntimeError('Managed runtime differs from pinned official package: ' + dll)
            nuspec = next(n for n in z.namelist() if n.endswith('.nuspec'))
            metadata['managed-package-metadata/' + nuspec] = z.read(nuspec)
            if name == 'newtonsoft.json':
                (ROOT / 'docs/licenses/Newtonsoft.Json-MIT.txt').write_bytes(z.read('LICENSE.md'))
        managed.append({'file': 'backend/' + dll, 'sha256': digest,
                        'nuget_package': name, 'version': version,
                        'matching_package_files': matches})

    report['managed_runtime'] = managed
    report['verification'] = {'sources': verified, 'missing_binary_build_metadata': missing_metadata,
                              'dependencies_rebuilt': False}
    (SOURCES / 'runtime-inventory.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    output = ROOT / 'dist/sources/GozarnoVPN-1.2.1-dependency-sources.zip'
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_STORED, allowZip64=True) as z:
        z.write(SOURCES / 'runtime-inventory.json', 'runtime-inventory.json')
        for source in report['source_packages']:
            z.write(SOURCES / source['filename'], 'native-sources/' + source['filename'])
        for name, content in sorted(metadata.items()):
            z.writestr(name, content)
        for name in ('source-build.md', 'third-party.md'):
            z.write(ROOT / 'docs' / name, name)
        for path in sorted((ROOT / 'docs/licenses').glob('*')):
            if path.is_file():
                z.write(path, 'licenses/' + path.name)
        for path in sorted((ROOT / 'dist/privacy-audit/payload-1.2.1/licenses').rglob('*')):
            if path.is_file():
                relative = path.relative_to(ROOT / 'dist/privacy-audit/payload-1.2.1/licenses')
                # New corrected notices supersede the old descriptive table.
                if relative.as_posix() != 'third-party.md':
                    z.write(path, 'original-payload-notices/' + relative.as_posix())
        z.write(ROOT / 'dist/privacy-audit/payload-1.2.1/vpnc-script.js', 'runtime-source/vpnc-script.js')
    with zipfile.ZipFile(output) as z:
        if z.testzip() is not None:
            raise RuntimeError('Source bundle ZIP verification failed')
    print('Bundle verified:', output.name, output.stat().st_size, 'bytes', flush=True)
    print('Missing package build metadata:', missing_metadata, flush=True)


if __name__ == '__main__':
    main()
