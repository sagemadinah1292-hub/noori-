"""Bundle the authenticated NOORI portal with private, tested Windows downloads.

Binary inputs must come from a successful Windows Actions build. No activation
configuration or private files are copied into the public upload archive.
"""
from pathlib import Path
import argparse
from datetime import datetime, timezone
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

version=str(info['version'])
if info.get('licensing_enforced') is not True:
    raise SystemExit('The final portal only publishes protected plugin builds.')
if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+',version):raise SystemExit('Unexpected release version.')
public_names=['index.php','index.html','home.php','login.php','account.php','register.php','create-password.php','dashboard.php','download.php','logout.php','change-password.php','reset-password.php','secure-downloads.php','portal-bootstrap.php','admin.php','.htaccess','assets/site.css','assets/noori-logo.svg']
public_files={name:(public/name).read_bytes() for name in public_names}
if args.preview:
    preview=args.preview.read_bytes()
    if preview[:8]!=b'\x89PNG\r\n\x1a\n':raise SystemExit('Invalid plugin preview PNG.')
    public_files['assets/plugin-preview.png']=preview
private_files={name:(root/'php/private'/name).read_bytes() for name in ['portal.php','portal-deploy.php']}
private_files.update({
    'downloads/NOORI_CHOIR_Setup_x64.exe':args.installer.read_bytes(),
    'downloads/NOORI-CHOIR-Windows-x64.zip':args.vst3_package.read_bytes(),
    'downloads/BUILD-INFO.txt':args.build_info.read_bytes(),
    'downloads/INSTALLER-TEST-RESULTS.txt':args.installer_tests.read_bytes(),
    'downloads/.htaccess':b'Require all denied\n',
})
manifest={'product':'NOORI CHOIR','version':version,'platform':'Windows x64','format':'VST3','licensingEnforced':True,'activationServiceDeploymentVerified':info.get('activation_service_deployment_verified') is True,'sourceCommit':info.get('commit'),'builtUtc':info.get('built_utc'),'codeSigned':evidence.get('code_signed') is True,'cubase15Verified':info.get('cubase_15_tested') is True,'aiModelIncluded':info.get('ai_model') is True,'downloads':[]}
for name in ['downloads/NOORI_CHOIR_Setup_x64.exe','downloads/NOORI-CHOIR-Windows-x64.zip']:
    data=private_files[name];manifest['downloads'].append({'file':name.removeprefix('downloads/'),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
private_files['downloads/release-info.txt']=(json.dumps(manifest,indent=2)+'\n').encode()
private_files['downloads/SHA256SUMS.txt']=''.join(d['sha256']+'  '+d['file']+'\n' for d in manifest['downloads']).encode()
private_files['downloads/INSTALLATION.txt']=f"""NOORI CHOIR {version} — protected Windows x64 VST3

Close Cubase and run NOORI_CHOIR_Setup_x64.exe. The default location is
C:\\Program Files\\Common Files\\VST3. Reopen Cubase and locate NOORI CHOIR
among VST3 effects. Rescan if needed.

Open Settings > Registration / Trial in the plugin. Enter the same email and
original invitation key used for your website account. Website registration
does not bind a computer or start the trial: the first plugin activation does.
Keep an internet connection for periodic registration checks.

For manual installation extract NOORI-CHOIR-Windows-x64.zip and copy the entire
NOORI CHOIR.vst3 folder to the VST3 directory. Sources and licenses are included.

Verify the installer in PowerShell:
Get-FileHash .\\NOORI_CHOIR_Setup_x64.exe -Algorithm SHA256
Compare with SHA256SUMS.txt downloaded from your account.

The installer is {'code signed' if manifest['codeSigned'] else 'unsigned'}.
Automated Windows plugin and installer tests passed. Live Cubase Pro 15 operation
and drag-and-drop remain separate host checks. DSP processing is used; no AI
singer model is included. Do not share your password or invitation serial.
""".encode('utf-8')

guide="""NOORI FINAL PORTAL UPGRADE — preserve your existing setup

1. Back up your existing noori-private folder and activation database privately.
2. Upload/extract NOORI-Final-Private-Upgrade.zip INSIDE the existing noori-private
   folder OUTSIDE public_html. It adds portal code and private downloads only.
   It does not contain or replace config.php, owner.json, signing-private.pem,
   public-modulus.txt or installed.marker. Do not rerun initial setup.
3. Upload/extract NOORI-Final-Public-Upgrade.zip INSIDE
   public_html/noori-activation. It adds the account pages and updates the owner
   page and .htaccess. Your already configured bootstrap.php is not replaced.
4. Open the normal website and choose Sign in. Owner ID is "owner" by default;
   use the EXISTING owner password from setup. This opens the OWNER dashboard.
   Friend email/password opens a separate USER dashboard. Old browser sessions
   need to sign in again after this update; passwords/keys are not reset.
   Opening the owner dashboard installs the root public-file download block,
   preserving a private backup of the previous .htaccess. If it reports an error,
   resolve it before sharing the portal. Website management contains the backup/
   cleanup controls; no separate upgrade link needs to be given to visitors.
5. Review the exact public-file list and apply the upgrade. Identified old NOORI
   public installers, ZIPs, duplicate assets and root PHP copies move into a dated
   private backup. Nothing is deleted. Unrelated .private, default.php and llms.txt
   are preserved. The root .htaccess is backed up and receives a NOORI archive guard.
   Resolve any reported errors before allowing friends to download.
6. In an incognito window register your email, then create a password. Your
   account must show Awaiting owner approval and downloads must return 403.
   In the owner dashboard set your authorized hosting sender email under
   Registration notifications. Recipient defaults to shahidulhaq@live.com.
   Retry notification and verify real inbox receipt; server acceptance is not
   delivery confirmation. Generate ONE key assigned to the requesting email,
   keep its CSV private, and enter that final key in the user dashboard.
   Only then should the installer download. Use the same key in the plugin.
7. While signed out verify dashboard.php and download.php?file=installer ask for
   sign-in. Old /downloads/...exe and NOORI-Website-With-Plugin.zip links at BOTH
   the domain root and /noori-activation must return 403 or 404.
8. Test the protected plugin in Cubase. Only after these checks create invitations
   for your friends and share the portal address.

Users can change their password. If forgotten, the owner creates a one-time
15-minute recovery code in Friend accounts and sends it privately. No automatic
email sender is required or claimed. Website signup reserves the invitation's
email without starting the trial or binding a Windows computer.

Expected final layout:
noori-private/ (outside every public_html)
  config.php, service.php, signing-private.pem, owner.json, installed.marker
  portal.php, portal-deploy.php
  downloads/ (protected installer and VST3 payloads)
  legacy-public-backup/ (recoverable old public copies)
public_html/
  .htaccess (existing rules plus managed NOORI guard)
  index.html (redirect to /noori-activation if an old NOORI homepage was found)
  noori-activation/
    index.php, index.html (redirect), home.php, login.php, account.php
    register.php, create-password.php, dashboard.php, download.php, logout.php, change-password.php, reset-password.php
    admin.php, secure-downloads.php, portal-bootstrap.php, bootstrap.php, .htaccess
    assets/

Do not place the private ZIP, private downloads, config or signing key anywhere
inside public_html. Do not delete other applications or unknown hosting files.
"""
args.output.mkdir(parents=True,exist_ok=True)
def archive(name,files):
    path=args.output/name
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
        stamp=datetime.now(timezone.utc).timetuple()[:6]
        for filename,data in files.items():
            entry=zipfile.ZipInfo(filename,stamp);entry.create_system=3;entry.external_attr=0o100644<<16;entry.compress_type=zipfile.ZIP_DEFLATED;z.writestr(entry,data)
    return path
pub=archive('NOORI-Final-Public-Upgrade.zip',public_files)
priv=archive('NOORI-Final-Private-Upgrade.zip',private_files)
(args.output/'NOORI-FINAL-UPLOAD-GUIDE.txt').write_text(guide,encoding='utf-8')
(args.output/'NOORI-EMERGENCY-DOWNLOAD-GUARD.txt').write_text('<FilesMatch \"(?i)\\.(?:exe|zip|vst3|dll)$\">\n  Require all denied\n</FilesMatch>\n',encoding='utf-8')
print('Created',pub,'and',priv,'with protected private payloads.')
print('Public ZIP contains no installer, serial, production config or private signing key.')
