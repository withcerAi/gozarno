"""Collect matching MSYS2 source packages for a frozen Gozarno installer payload.

Does not install packages, execute PKGBUILDs, or collect untracked user files.
Uses standard-library Python and a pre-extracted installer payload.
"""
import argparse, concurrent.futures, gzip, hashlib, json, pathlib, re, urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
DB = ROOT / '.tools/msys64/var/lib/pacman/local'
OUTPUT = ROOT / 'dist/sources/native-sources'

def description(path):
    parts = re.split(r'^%([^%]+)%\s*$', path.read_text(encoding='utf-8'), flags=re.M)
    return {parts[i]: parts[i+1].strip().splitlines() for i in range(1,len(parts),2)}

def inventory(payload):
    packages = {}
    by_leaf = {}
    for directory in DB.iterdir():
        if not (directory/'desc').exists(): continue
        desc=description(directory/'desc')
        name=desc['NAME'][0]
        if not name.startswith('mingw-w64-x86_64-'): continue
        packages[name]={'name':name,'base':desc.get('BASE',desc['NAME'])[0],
                        'version':desc['VERSION'][0],'license':desc.get('LICENSE',[]),
                        'upstream':(desc.get('URL') or [''])[0],'database':directory}
        blob=(directory/'mtree').read_bytes()
        text=(gzip.decompress(blob) if blob.startswith(b'\x1f\x8b') else blob).decode()
        for line in text.splitlines():
            path=line.split(' ',1)[0]
            if not path.lower().endswith(('.dll','.exe')): continue
            digest=re.search(r'sha256digest=([a-f0-9]{64})',line)
            if digest: by_leaf.setdefault(path.rsplit('/',1)[-1].lower(),[]).append((name,path[2:],digest.group(1)))
    result=[]; unowned=[]
    for file in sorted(payload.rglob('*')):
        if not file.is_file() or file.suffix.lower() not in ('.dll','.exe'): continue
        relative=file.relative_to(payload).as_posix()
        if relative.startswith(('$','backend/')) or file.name.lower() in ('gozarnovpn.exe','wintun.dll','libopenconnect-5.dll','uninstall.exe'): continue
        digest=hashlib.sha256(file.read_bytes()).hexdigest()
        candidates=[p for p in by_leaf.get(file.name.lower(),[]) if p[2]==digest]
        if len(candidates)!=1:
            unowned.append({'file':relative,'sha256':digest}); continue
        owner,path,_=candidates[0];p=packages[owner]
        result.append({'file':relative,'sha256':digest,'package':owner,'package_file':path})
    selected={row['package'] for row in result}
    # Header-only implementations are compiled into the client itself.
    selected.update(('mingw-w64-x86_64-spdlog','mingw-w64-x86_64-fmt',
                     'mingw-w64-x86_64-nsis'))
    records=[]
    for name in sorted(selected):
        p=packages[name]
        records.append({k:v for k,v in p.items() if k!='database'})
    return {'files':result,'packages':records,'unowned':unowned}

def fetch(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Gozarno-source-collector'}),timeout=60) as response:
        return response.read()

def download(source):
    path=OUTPUT/source['filename']
    if not path.exists():
        temporary=path.with_suffix(path.suffix+'.partial')
        for attempt in range(3):
            try:
                with urllib.request.urlopen(source['url'],timeout=60) as response, temporary.open('wb') as target:
                    while True:
                        chunk=response.read(1024*1024)
                        if not chunk: break
                        target.write(chunk)
                temporary.replace(path); break
            except Exception:
                if attempt==2: raise
    source['size']=path.stat().st_size
    source['sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
    print('Collected:',source['filename'],source['size'],flush=True)
    return source

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--download',action='store_true')
    parser.add_argument('--use-cached-sources', action='store_true',
                        help='Inventory exact-version source archives already downloaded; no network')
    args=parser.parse_args();OUTPUT.mkdir(parents=True,exist_ok=True)
    payload=ROOT/'dist/privacy-audit/payload-1.2.1'
    report=inventory(payload)
    (OUTPUT/'runtime-inventory.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('Mapped',len(report['files']),'runtime binaries to',len(report['packages']),'packages; unowned:',report['unowned'],flush=True)
    if report['unowned']: raise RuntimeError('Unknown binaries; inspect before source collection')
    if not args.download and not args.use_cached_sources: return
    if args.use_cached_sources:
        links={p.name for p in OUTPUT.glob('*.src.tar.*') if not p.name.endswith('.partial')}
    else:
        index=fetch('https://repo.msys2.org/mingw/sources/').decode()
        links=set(re.findall(r'href="([^"/]+\.src\.tar\.(?:zst|gz|xz))"',index))
    sources={}
    for package in report['packages']:
        prefix=package['base']+'-'+package['version']+'.src.tar.'
        matches=sorted(name for name in links if name.startswith(prefix))
        if len(matches)!=1: raise RuntimeError('Matching source package unavailable: '+prefix)
        name=matches[0]
        sources[name]={'filename':name,'url':'https://repo.msys2.org/mingw/sources/'+name,'packages':[]}
    for package in report['packages']:
        name=next(name for name in sources if name.startswith(package['base']+'-'+package['version']+'.src.tar.'))
        sources[name]['packages'].append(package['name'])
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        completed=list(pool.map(download,sources.values()))
    report['source_packages']=completed
    (OUTPUT/'runtime-inventory.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('All exact-version source packages collected.',flush=True)

if __name__=='__main__': main()
