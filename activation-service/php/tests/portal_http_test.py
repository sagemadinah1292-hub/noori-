"""Real Apache .htaccess + PHP account/download integration test.
TLS termination is simulated only on the isolated internal PHP hop. No production
credentials, real invitation serials or private keys are used.
"""
from pathlib import Path
import csv,hashlib,http.client,io,json,os,re,shutil,socket,subprocess,tempfile,time
from urllib.parse import urlencode
root=Path(__file__).resolve().parents[1]
def port():
    with socket.socket() as s:s.bind(('127.0.0.1',0));return s.getsockname()[1]
class Browser:
    def __init__(self,port):self.port=port;self.cookies={}
    def call(self,path,fields=None,method=None):
        c=http.client.HTTPConnection('127.0.0.1',self.port,timeout=10)
        headers={'Cookie':'; '.join(k+'='+v for k,v in self.cookies.items())} if self.cookies else {}
        body=None
        if fields is not None:body=urlencode(fields);headers['Content-Type']='application/x-www-form-urlencoded'
        c.request(method or ('POST' if fields is not None else 'GET'),path,body=body,headers=headers)
        r=c.getresponse();data=r.read();headers={k.lower():v for k,v in r.getheaders()}
        for k,v in r.getheaders():
            if k.lower()=='set-cookie':
                name,value=v.split(';',1)[0].split('=',1);self.cookies[name]=value
        result=(r.status,headers,data);c.close();return result

def csrf(body):
    m=re.search(rb'name="csrf" value="([a-f0-9]+)"',body);assert m,'CSRF token missing';return m[1].decode()
base='/noori-activation'
with tempfile.TemporaryDirectory(prefix='noori-portal-apache-') as tmp:
    tmp=Path(tmp);public=tmp/'public_html';app=public/'noori-activation';private=tmp/'noori-private'
    shutil.copytree(root/'public',app);shutil.copytree(root/'private',private)
    bootstrap=app/'bootstrap.php';bootstrap.write_text(bootstrap.read_text().replace('/CHANGE_TO_YOUR_HOME_DIRECTORY/noori-private',str(private)))
    if os.environ.get('NOORI_TEST_DSN','').startswith('sqlite:'):(private/'schema.mysql.sql').write_text('') # Tables already created by service tests.
    token='test-setup-'+'a'*64;password='Owner-password-12345!'
    (private/'config.php').write_text("<?php return ['dsn'=>getenv('NOORI_TEST_DSN'),'db_user'=>getenv('NOORI_TEST_USER'),'db_password'=>getenv('NOORI_TEST_PASSWORD'),'public_url'=>'https://activation.test/noori-activation','setup_token'=>'"+token+"','access_policy'=>'online'];")
    # The preceding SQL/service tests have created the legacy tables.
    (tmp/'clear.php').write_text("<?php $p=new PDO(getenv('NOORI_TEST_DSN'),getenv('NOORI_TEST_USER'),getenv('NOORI_TEST_PASSWORD'));$p->exec('DELETE FROM noori_requests;DELETE FROM noori_accounts;DELETE FROM noori_licenses;DELETE FROM noori_rate_limits');")
    subprocess.run(['php',str(tmp/'clear.php')],check=True)
    backend_port=port();apache_port=port();logs=[];processes=[]
    router=tmp/'router.php'
    router.write_text("<?php $_SERVER['HTTPS']='on';$route=parse_url($_SERVER['REQUEST_URI'],PHP_URL_PATH);$name=basename($route);$allowed=['index.php','home.php','login.php','register.php','create-password.php','verify-email.php','account.php','dashboard.php','download.php','logout.php','change-password.php','reset-password.php','setup.php','admin.php','secure-downloads.php'];if(!in_array($name,$allowed)){http_response_code(404);exit;}$original=explode(' ',$_SERVER['HTTP_X_NOORI_TEST_REQUEST']??'');if(isset($original[1]))$_SERVER['REQUEST_URI']=$original[1];$_SERVER['SCRIPT_NAME']='/noori-activation/'.$name;require "+repr(str(app))+".'/'.$name;")
    backend_log=open(tmp/'php.log','w+');logs.append(backend_log)
    capture_file=tmp/'mail.txt';sendmail=tmp/'capture-mail.sh';sendmail.write_text('#!/bin/sh\ncat >> '+str(capture_file)+'\n');sendmail.chmod(0o700)
    processes.append(subprocess.Popen(['php','-d','sendmail_path='+str(sendmail),'-S',f'127.0.0.1:{backend_port}',str(router)],cwd=app,stdout=backend_log,stderr=backend_log))
    # This test-only proxy rule runs after the shipped deny and route rules.
    with (app/'.htaccess').open('a') as f:f.write(f'\nRewriteRule ^(.*\\.php)$ http://127.0.0.1:{backend_port}/noori-activation/$1 [P,L]\n')
    module_dir=os.environ.get('NOORI_APACHE_MODULES','/usr/lib/apache2/modules')
    modules=['mpm_event','authz_core','authz_host','dir','mime','rewrite','proxy','proxy_http','headers']
    (tmp/'run').mkdir()
    conf=tmp/'apache.conf'
    conf.write_text('\n'.join([f'ServerRoot "{tmp}"',f'DefaultRuntimeDir "{tmp}/run"',f'PidFile "{tmp}/apache.pid"',f'Listen 127.0.0.1:{apache_port}','ServerName localhost',*[f'LoadModule {m}_module {module_dir}/mod_{m}.so' for m in modules],f'ErrorLog "{tmp}/apache-error.log"','LogLevel warn','TypesConfig /etc/mime.types',f'DocumentRoot "{public}"',f'<Directory "{public}">','AllowOverride All','Require all granted','</Directory>', 'ProxyRequests Off','RequestHeader set X-Noori-Test-Request "expr=%{THE_REQUEST}"'])+'\n')
    apache_log=open(tmp/'apache.log','w+');logs.append(apache_log)
    processes.append(subprocess.Popen([os.environ.get('NOORI_APACHE_BIN','apache2'),'-f',str(conf),'-DFOREGROUND'],stdout=apache_log,stderr=apache_log))
    owner=Browser(apache_port);friend=Browser(apache_port);anonymous=Browser(apache_port)
    try:
        for _ in range(100):
            try:
                if owner.call(base+'/setup.php')[0]==200:break
            except OSError:pass
            time.sleep(.05)
        else:raise AssertionError('Apache/PHP fixture did not start')
        status,_,body=owner.call(base+'/setup.php',{'token':token,'password':password});assert status==200 and (private/'installed.marker').exists(),'Owner setup'
        (app/'setup.php').unlink()
        before={n:hashlib.sha256((private/n).read_bytes()).hexdigest() for n in ['config.php','signing-private.pem','owner.json','installed.marker']}
        payload=private/'downloads';payload.mkdir()
        installer=b'MZ_NOORI_TEST_FIXTURE_NOT_A_REAL_INSTALLER';vst=b'NOORI_TEST_VST3_ARCHIVE'
        (payload/'NOORI_CHOIR_Setup_x64.exe').write_bytes(installer);(payload/'NOORI-CHOIR-Windows-x64.zip').write_bytes(vst)
        (payload/'release-info.txt').write_text(json.dumps({'product':'NOORI CHOIR','version':'0.4.1','licensingEnforced':True}))
        (payload/'INSTALLATION.txt').write_text('NOORI test guide');(payload/'SHA256SUMS.txt').write_text('fixture checksums')
        # Representative old public copies, plus unrelated hosting files that must survive.
        for parent in [public,app]:
            (parent/'NOORI-Website-With-Plugin.zip').write_bytes(b'OLD_PUBLIC_ARCHIVE')
            old=parent/'downloads';old.mkdir(exist_ok=True);(old/'NOORI_CHOIR_Setup_x64.exe').write_bytes(b'OLD_INSTALLER')
        shutil.copytree(app/'assets',public/'assets');shutil.copytree(app/'assets',app/'assets.4267')
        (public/'downloads.5773').mkdir();(public/'downloads.5773/NOORI_CHOIR_Setup_x64.exe').write_bytes(b'OLD_COPY')
        (public/'index.html').write_text('<title>NOORI CHOIR</title>')
        (public/'admin.php').write_text("<?php session_name('noori_owner');")
        (public/'bootstrap.php').write_text('<?php function nooriService(){}')
        (public/'index.php').write_text('<?php nooriService();')
        (public/'default.php').write_text('HOSTINGER_UNRELATED');(public/'llms.txt').write_text('UNCHANGED');(public/'.private').mkdir();(public/'.htaccess').write_text('Options -Indexes\nDirectoryIndex index.html\n')
        (public/'unlisted-copy.zip').write_bytes(b'UNLISTED_COPY')
        status,headers,page=anonymous.call(base+'/');assert status==200 and b'Create my account' in page and b'Owner sign in' not in page and b'downloads/NOORI_CHOIR' not in page,'Public home has account access, no direct installer link'
        assert anonymous.call(base+'/downloads/NOORI_CHOIR_Setup_x64.exe')[0]==403,'Apache denies legacy public download path'
        assert anonymous.call(base+'/downloads.5773/NOORI_CHOIR_Setup_x64.exe')[0] in [403,404],'Apache denies suffixed download folders'
        assert anonymous.call(base+'/NOORI-Website-With-Plugin.zip')[0]==403,'Apache denies uploaded website archive'
        assert anonymous.call(base+'/bootstrap.php')[0]==403,'Apache denies helper source'
        result=anonymous.call(base+'/download.php?file=installer');assert result[0]==303,('Anonymous file endpoint requires sign-in',result)
        assert anonymous.call(base+'/dashboard.php')[0]==303,'Anonymous account endpoint requires sign-in'
        assert owner.call(base+'/admin.php')[0]==303,'Owner route redirects to common sign-in'
        status,headers,page=owner.call(base+'/login.php')
        status,headers,page=owner.call(base+'/login.php',{'csrf':csrf(page),'email':'owner','password':password});assert status==303 and headers['location']=='admin.php','Owner login selects owner dashboard'
        status,headers,page=owner.call(base+'/admin.php');assert status==200 and b'owner dashboard' in page and b'Generate invitations' in page,'Owner full controls'
        assert owner.call(base+'/download.php?file=installer')[2]==installer,'Owner has full download access without a friend invitation'
        assert anonymous.call('/NOORI-Website-With-Plugin.zip')[0]==403 and anonymous.call('/downloads/NOORI_CHOIR_Setup_x64.exe')[0]==403,'Root public downloads blocked automatically on owner login before cleanup'
        assert anonymous.call('/unlisted-copy.zip')[0]==403,'Unknown public ZIP filename cannot bypass the root block'
        _,_,mail_settings=owner.call(base+'/admin.php');assert owner.call(base+'/admin.php',{'csrf':csrf(mail_settings),'action':'notification-settings','sender':'no-reply@example.com'})[0]==200
        _,_,page=owner.call(base+'/admin.php')
        status,_,csv_body=owner.call(base+'/admin.php',{'csrf':csrf(page),'action':'generate','count':'1','kind':'trial','email':'friend@example.com'});assert status==200
        issued=list(csv.DictReader(io.StringIO(csv_body.decode())))[0];key=issued['Serial'];friend_license=issued['Owner record ID']
        status,headers,page=friend.call(base+'/register.php');assert status==200 and 'secure' in headers['set-cookie'].lower(),'Secure friend cookie'
        assert friend.call(base+'/register.php',{'csrf':'bad','email':'friend@example.com','serial':key,'password':'Friend-password-123!','confirm':'Friend-password-123!'})[0]==403,'Signup CSRF'
        fields={'csrf':csrf(page),'email':'friend@example.com','serial':key}
        status,headers,_=friend.call(base+'/register.php',fields);assert status==303 and headers['location']=='create-password.php','Registration details lead to password step'
        assert friend.call(base+'/download.php?file=installer')[0]==303,'Step one alone does not authorize downloads'
        assert friend.call(base+'/admin.php')[0]==303,'Pending registration is not an owner session'
        assert anonymous.call(base+'/create-password.php')[0]==303,'Password step requires verified registration details'
        status,_,page=friend.call(base+'/create-password.php');assert status==200 and b'STEP 2 OF 2' in page and key.encode() not in page,'Password step contains no raw serial'
        status,headers,_=friend.call(base+'/create-password.php',{'csrf':csrf(page),'password':'Friend-password-123!','confirm':'Friend-password-123!','role':'owner'});assert status==303 and headers['location']=='dashboard.php','Completed registration selects user dashboard, ignoring forged role'
        _,_,pending=friend.call(base+'/dashboard.php');assert b'Confirm your email for automatic trial' in pending
        assert friend.call(base+'/download.php?file=installer')[0]==403,'Registered account cannot download before owner key'
        _,_,review=owner.call(base+'/admin.php');assert b'friend@example.com' in review and b'accepted_by_mail_server' in review,'Owner sees pending request and honest unsent notification'
        assert friend.call(base+'/admin.php',{'csrf':csrf(pending),'action':'notification-settings','sender':'intruder@example.com'})[0]==403,'Only owner can set mail sender'
        assert friend.call(base+'/dashboard.php',{'csrf':csrf(pending),'serial':'NOORI-F-'+'F'*32})[0]==403,'Invented key cannot approve account'
        _,_,pending=friend.call(base+'/dashboard.php')
        assert friend.call(base+'/dashboard.php',{'csrf':csrf(pending),'serial':key})[0]==303,'Owner email-assigned key unlocks account'

        assert friend.call(base+'/admin.php')[0]==403 and friend.call(base+'/secure-downloads.php')[0]==403,'User cannot open owner pages'
        _,_,user_page=friend.call(base+'/dashboard.php')
        assert friend.call(base+'/admin.php',{'csrf':csrf(user_page),'action':'generate','count':'20','kind':'full'})[0]==403,'User cannot execute owner actions with their own valid CSRF token'
        status,_,page=friend.call(base+'/dashboard.php');assert status==200 and b'Ready to activate' in page and b'Starts on first plugin activation' in page and b'Generate invitations' not in page and b'Owner dashboard' not in page,'Limited user dashboard'
        assert key.encode() not in page,'Raw serial never exposed in dashboard'
        status,headers,data=friend.call(base+'/download.php?file=installer');assert status==200 and data==installer and 'private' in headers['cache-control'] and 'no-store' in headers['cache-control'],'Authenticated private installer streaming'
        assert friend.call(base+'/download.php?file=../../config.php')[0]==404,'Download allowlist blocks traversal'
        assert anonymous.call('/noori-private/signing-private.pem')[0] in [403,404],'Private directory outside web root'
        status,_,page=owner.call(base+'/secure-downloads.php');assert status==200 and b'NOORI-Website-With-Plugin.zip' in page,'Owner upgrade previews public copies'
        assert owner.call(base+'/secure-downloads.php',{'csrf':'bad','reviewed':'yes'})[0]==403,'Upgrade CSRF enforced'
        status,_,page=owner.call(base+'/secure-downloads.php');status,_,page=owner.call(base+'/secure-downloads.php',{'csrf':csrf(page),'reviewed':'yes'});assert status==200 and b'Upgrade completed' in page,'Owner reversible cleanup'
        assert not (public/'NOORI-Website-With-Plugin.zip').exists() and not (app/'NOORI-Website-With-Plugin.zip').exists(),'Public archives quarantined'
        assert not (public/'downloads').exists() and not (app/'downloads').exists() and not (public/'downloads.5773').exists(),'Legacy binary folders quarantined'
        assert anonymous.call('/NOORI-Website-With-Plugin.zip')[0] in [403,404] and anonymous.call('/downloads/NOORI_CHOIR_Setup_x64.exe')[0] in [403,404],'Root download bypass closed'
        assert (public/'default.php').read_text()=='HOSTINGER_UNRELATED' and (public/'llms.txt').read_text()=='UNCHANGED' and (public/'.private').is_dir(),'Unrelated hosting files preserved'
        assert all(hashlib.sha256((private/n).read_bytes()).hexdigest()==h for n,h in before.items()),'Existing config, signing key, owner password and install marker preserved'
        assert list((private/'legacy-public-backup').glob('*/files/NOORI-Website-With-Plugin.zip')),'Old files recoverable from backup'
        # Automatic trial: no owner-issued key, confirm email, retain original password.
        auto=Browser(apache_port);_,_,page=auto.call(base+'/register.php');auto.call(base+'/register.php',{'csrf':csrf(page),'email':'autoweb@example.com'});_,_,page=auto.call(base+'/create-password.php');auto.call(base+'/create-password.php',{'csrf':csrf(page),'password':'Auto-password-123!','confirm':'Auto-password-123!'})
        assert auto.call(base+'/download.php?file=installer')[0]==403,'Unverified email cannot download'
        mail=capture_file.read_text();match=re.search(r'verify-email.php\?email=autoweb%40example.com&code=([a-f0-9]{64})',mail);assert match,'Verification automatically mailed'
        link=base+'/verify-email.php?email=autoweb%40example.com&code='+match[1]
        _,_,confirm=auto.call(link);assert auto.call(base+'/download.php?file=installer')[0]==403,'Mail scanner GET does not issue access'
        status,_,_=auto.call(base+'/verify-email.php',{'csrf':csrf(confirm),'email':'autoweb@example.com','code':match[1]});assert status==303
        _,_,page=auto.call(base+'/dashboard.php');assert b'NOORI-T-' in page and b'Three-day trial' in page,'Automatic serial and trial visible'
        assert auto.call(base+'/download.php?file=installer')[0]==200,'Confirmed user downloads without owner waiting'
        auto.call(base+'/logout.php',{'csrf':csrf(page)})
        for _ in range(2):
            _,_,page=auto.call(base+'/login.php');assert auto.call(base+'/login.php',{'csrf':csrf(page),'email':'autoweb@example.com','password':'Auto-password-123!'})[0]==303,'Repeat login uses original password';_,_,page=auto.call(base+'/dashboard.php');auto.call(base+'/logout.php',{'csrf':csrf(page)})
        _,_,page=auto.call(base+'/reset-password.php');auto.call(base+'/reset-password.php',{'csrf':csrf(page),'action':'request','email':'autoweb@example.com'})
        mail=capture_file.read_text();reset=re.search(r'reset-password.php\?email=autoweb%40example.com&code=([a-f0-9]{64})',mail);assert reset,'Self-service reset automatically mailed'
        _,_,page=auto.call(base+'/reset-password.php');assert auto.call(base+'/reset-password.php',{'csrf':csrf(page),'email':'autoweb@example.com','code':reset[1],'password':'Auto-new-password-123!','confirm':'Auto-new-password-123!'})[0]==200
        # Another session is invalidated when the password changes.
        second=Browser(apache_port);_,_,page=second.call(base+'/login.php');assert second.call(base+'/login.php',{'csrf':csrf(page),'email':'friend@example.com','password':'Friend-password-123!'})[0]==303
        _,_,page=friend.call(base+'/change-password.php');assert friend.call(base+'/change-password.php',{'csrf':csrf(page),'current':'Friend-password-123!','password':'New-password-456!','confirm':'New-password-456!'})[0]==200
        assert second.call(base+'/download.php?file=installer')[0]==303,'Password change revokes other web sessions'
        # Owner can revoke an invitation after login; a copied download link does not bypass it.
        _,_,page=owner.call(base+'/admin.php')
        assert owner.call(base+'/admin.php',{'csrf':csrf(page),'action':'revoke','id':friend_license})[0]==200
        assert friend.call(base+'/download.php?file=installer')[0]==403,'Owner disable blocks signed-in downloader'
        print('PASS real Apache/PHP portal: registration/login, private downloads, legacy URL/archive denial, CSRF, session invalidation, revocation and reversible root cleanup with secret preservation')
    except Exception:
        for f in logs:f.flush()
        for name in ['apache-error.log','php.log']:
            p=tmp/name
            if p.exists():print(p.read_text()[-8000:])
        raise
    finally:
        for p in reversed(processes):p.terminate();p.wait(timeout=10)
        for f in logs:f.close()
