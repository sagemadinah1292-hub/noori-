"""Bundle the public NOORI website with real, tested Windows downloads.

Binary inputs must come from a successful Windows Actions build. No activation
configuration or private files are copied into the public upload archive.
"""
from pathlib import Path
import argparse
import hashlib
import html
import json
import re
import struct
import zipfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--installer',type=Path,required=True)
parser.add_argument('--vst3-package',type=Path,required=True)
parser.add_argument('--build-info',type=Path,required=True)
parser.add_argument('--installer-tests',type=Path,required=True)
parser.add_argument('--preview',type=Path)
parser.add_argument('--output',type=Path,default=Path('dist-activation'))
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
public=root/'php/public'
info=json.loads(args.build_info.read_text(encoding='utf-8-sig'))
evidence=json.loads(args.installer_tests.read_text(encoding='utf-8-sig'))
if not (info.get('product')=='NOORI CHOIR' and info.get('platform')=='Windows x64' and info.get('format')=='VST3' and info.get('automated_tests')=='passed' and info.get('pe_amd64_verified') is True):
    raise SystemExit('Refusing website bundle: Windows VST3 build verification is missing.')
for key in ['installer_compiled','silent_install_tested','installed_binary_hash_verified','silent_uninstall_tested','unrelated_files_preserved']:
    if evidence.get(key) is not True:raise SystemExit('Refusing website bundle: installer evidence is missing: '+key)
if args.installer.read_bytes()[:2]!=b'MZ':raise SystemExit('Installer is not a Windows executable.')
with zipfile.ZipFile(args.vst3_package) as package:
    modules=[n for n in package.namelist() if n.endswith('/Contents/x86_64-win/NOORI CHOIR.vst3')]
    if len(modules)!=1:raise SystemExit('Expected one Windows x64 VST3 module.')
    module=package.read(modules[0])
    if hashlib.sha256(module).hexdigest().lower()!=info.get('sha256','').lower():raise SystemExit('VST3 does not match the tested build metadata.')
    pe=struct.unpack_from('<I',module,0x3c)[0]
    if module[:2]!=b'MZ' or module[pe:pe+4]!=b'PE\0\0' or struct.unpack_from('<H',module,pe+4)[0]!=0x8664:raise SystemExit('VST3 is not AMD64 PE.')
    if not any(n.endswith('/Source/voice-choir/Source/Plugin.cpp') for n in package.namelist()):raise SystemExit('VST3 archive must include project sources.')

version=str(info['version']);protected=info.get('licensing_enforced') is True
if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+',version):raise SystemExit('Unexpected release version.')
page=(public/'index.html').read_text(encoding='utf-8')
def replace_region(text,start,end,replacement):
    pattern=re.escape(start)+r'.*?'+re.escape(end)
    result,count=re.subn(pattern,lambda _:start+replacement+end,text,flags=re.S)
    if count!=1:raise SystemExit('Website template marker missing: '+start)
    return result

def download_link(kind,path,label):
    global page
    pattern=r'<a class="([^"]+)" data-download="'+kind+r'" href="[^"]+">.*?</a>'
    page,count=re.subn(pattern,lambda m:'<a class="'+m[1]+'" data-download="'+kind+'" href="'+path+'" download>'+label+' <span aria-hidden="true">↓</span></a>',page,flags=re.S)
    if count!=1:raise SystemExit('Download link marker missing: '+kind)

download_link('installer','downloads/NOORI_CHOIR_Setup_x64.exe','Download Windows installer (.exe)')
download_link('vst3','downloads/NOORI-CHOIR-Windows-x64.zip','Manual VST3 + source (.zip)')
page=page.replace('<span id="download-version">0.4.0</span>','<span id="download-version">'+html.escape(version)+'</span>')
if protected:
    notice='<div class="build-notice" role="note"><strong>Registration required</strong><p>This installer requires your NOORI email and invitation key. Trial keys last 72 hours from first activation. Keep your internet connection on for registration checks.</p></div>'
    page=replace_region(page,'<!-- BUILD_NOTICE_START -->','<!-- BUILD_NOTICE_END -->',notice)
    page=page.replace('The current development preview does not enforce a trial.','Your owner controls trial and free full-access invitations.')
else:
    # The shipped page must never advertise an unprotected build as a locked trial.
    if 'This download does not enforce serial activation or a three-day trial.' not in page:raise SystemExit('Unprotected build notice is missing.')
page=replace_region(page,'<!-- DOWNLOAD_INFO_START -->','<!-- DOWNLOAD_INFO_END -->','<p class="download-help"><a href="downloads/INSTALLATION.txt">Installation guide</a> · <a href="downloads/SHA256SUMS.txt">SHA-256 checksums</a> · <a href="downloads/release-info.txt">Build details</a></p>')
if evidence.get('code_signed') is True:page=page.replace('The Windows installer is unsigned. Verify the file before running it.','The Windows installer is code signed. Verify its publisher before running it.')
files={
    '.htaccess':(public/'.htaccess').read_bytes(),
    'assets/site.css':(public/'assets/site.css').read_bytes(),
    'assets/noori-logo.svg':(public/'assets/noori-logo.svg').read_bytes(),
    'downloads/NOORI_CHOIR_Setup_x64.exe':args.installer.read_bytes(),
    'downloads/NOORI-CHOIR-Windows-x64.zip':args.vst3_package.read_bytes(),
    'downloads/BUILD-INFO.txt':args.build_info.read_bytes(),
    'downloads/INSTALLER-TEST-RESULTS.txt':args.installer_tests.read_bytes(),
}
if args.preview:
    image=args.preview.read_bytes()
    if image[:8]!=b'\x89PNG\r\n\x1a\n':raise SystemExit('Plugin preview is not a PNG.')
    files['assets/plugin-preview.png']=image
    page=page.replace('<!-- PLUGIN_PREVIEW -->','<figure class="plugin-preview"><img src="assets/plugin-preview.png" alt="NOORI CHOIR plugin showing vocal parts, waveforms, playback and export controls" width="720" height="1000" loading="lazy"><figcaption>NOORI CHOIR · the plugin results view</figcaption></figure>')
else:page=page.replace('<!-- PLUGIN_PREVIEW -->','')
manifest={'product':'NOORI CHOIR','version':version,'platform':'Windows x64','format':'VST3','licensingEnforced':protected,'activationServiceDeploymentVerified':info.get('activation_service_deployment_verified') is True,'sourceCommit':info.get('commit'),'builtUtc':info.get('built_utc'),'codeSigned':evidence.get('code_signed') is True,'cubase15Verified':info.get('cubase_15_tested') is True,'aiModelIncluded':info.get('ai_model') is True,'downloads':[]}
for name in ['downloads/NOORI_CHOIR_Setup_x64.exe','downloads/NOORI-CHOIR-Windows-x64.zip']:
    data=files[name];manifest['downloads'].append({'file':name.removeprefix('downloads/'),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
files['downloads/release-info.txt']=(json.dumps(manifest,indent=2)+'\n').encode()
files['downloads/SHA256SUMS.txt']=''.join(d['sha256']+'  '+d['file']+'\n' for d in manifest['downloads']).encode()
status='Requires a registered email and invitation key.' if protected else 'DEVELOPMENT PREVIEW: serial activation and trial enforcement are disabled in this build.'
guide=f'''NOORI CHOIR {version} - Windows x64 VST3
{status}

1. Close Cubase before running NOORI_CHOIR_Setup_x64.exe.
2. The default install location is C:\\Program Files\\Common Files\\VST3.
3. Reopen Cubase and locate NOORI CHOIR among VST3 effects. Rescan plugins if needed.
4. Add the plugin to a vocal track and import a WAV.
5. Protected builds: open Settings > Registration / Trial, enter your email and
   invitation key, and activate online. Development previews do not need a key.

Manual install: extract NOORI-CHOIR-Windows-x64.zip and copy the entire
NOORI CHOIR.vst3 folder into the standard VST3 directory while Cubase is closed.
Keep the full .vst3 folder structure intact. Project sources and licenses are
included in the archive.

Verify downloads in PowerShell with:
Get-FileHash .\\NOORI_CHOIR_Setup_x64.exe -Algorithm SHA256
Compare the result with SHA256SUMS.txt from the same website download.

The installer is {'code signed' if manifest['codeSigned'] else 'unsigned'}.
Automated Windows VST3 and installer smoke tests passed; this is not a statement
that this build has been verified inside Cubase Pro 15. The engine uses DSP;
no AI singer model is included. Cubase file drag-and-drop still needs host testing.
'''
files['downloads/INSTALLATION.txt']=guide.encode()
files['index.html']=page.encode()
args.output.mkdir(parents=True,exist_ok=True)
archive=args.output/'NOORI-Website-With-Plugin.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for name,data in files.items():z.writestr(name,data)
print('Created',archive,'with real Windows downloads; licensing enforced:',protected)
print('No private activation configuration or signing keys included. Existing PHP service files are not replaced.')
