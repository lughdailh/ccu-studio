#!/usr/bin/env python3
"""Reject thin binaries, development-machine linkage, missing assets and bad signatures."""
import json, plistlib, re, subprocess, sys
from pathlib import Path
bundle = Path(sys.argv[1]).resolve()
version = sys.argv[2]
def run(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)
plist = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
assert plist['CFBundleShortVersionString'] == version, plist
binary = bundle / 'Contents/MacOS' / plist['CFBundleExecutable']
architectures = run('lipo', '-archs', str(binary)).split()
assert set(architectures) == {'arm64', 'x86_64'}, architectures
assert not (bundle / 'Contents/Frameworks').exists(), 'Do not bundle a second copy of OBS/Qt'
resources = bundle / 'Contents/Resources'
for locale in ['ca-ES.ini', 'en-US.ini']:
    assert (resources / 'locale' / locale).is_file(), locale
if bundle.stem == 'ccu-studio':
    for asset in ['ccu-color.effect', 'ccu-compare.effect', 'CCUstudio.svg']:
        assert (resources / asset).is_file(), asset
    assert list((resources / 'icons').glob('*.png'))
checks = {}
for arch in architectures:
    links = [line.strip().split(' (compatibility')[0] for line in run('otool', '-arch', arch, '-L', str(binary)).splitlines()[1:] if line.startswith('\t')]
    assert any('libobs.framework/' in link for link in links), links
    assert any('obs-frontend-api' in link for link in links), links
    assert any('QtWidgets.framework/' in link for link in links), links
    for link in links:
        assert link.startswith(('@rpath/', '/System/Library/', '/usr/lib/')), link
    loads = run('otool', '-arch', arch, '-l', str(binary))
    rpaths = re.findall(r'cmd LC_RPATH\n.*?path (.*?) \(offset', loads, re.S)
    assert '@executable_path/../Frameworks' in rpaths, rpaths
    assert all(p.startswith(('@executable_path/', '@loader_path/')) for p in rpaths), rpaths
    symbols = run('nm', '-arch', arch, '-gU', str(binary))
    for symbol in ['_obs_module_load', '_obs_module_set_pointer', '_obs_module_ver', '_obs_module_unload']:
        assert re.search(r'\b' + symbol + r'$', symbols, re.M), symbol
    minimum = re.search(r'cmd LC_BUILD_VERSION\n.*?minos ([\d.]+)', loads, re.S)
    assert minimum and minimum.group(1) == '13.0', minimum
    checks[arch] = {'linkedLibraries': links, 'rpaths': rpaths, 'minimumMacOS': minimum.group(1)}
run('codesign', '--verify', '--deep', '--strict', '--verbose=2', str(bundle))
print(json.dumps({'plugin':bundle.name, 'version':version, 'obsVersion':'33.0.3',
                  'dependencies':'2026-10-23', 'commit':run('git','rev-parse','HEAD').strip(),
                  'architectures':architectures, 'staticChecks':checks,
                  'signature':'ad-hoc', 'notarized':False, 'runtimeTested':False}, indent=2))
