"""Isolated owner/API integration test. Uses an internal HTTP hop to simulate TLS termination."""
from pathlib import Path
import base64,csv,http.client,io,json,os,re,shutil,socket,subprocess,tempfile,time

root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='noori-php-http-') as temp:
    temp=Path(temp);private=temp/'private';public=temp/'public';shutil.copytree(root/'private',private);shutil.copytree(root/'public',public)
    bootstrap=public/'bootstrap.php';bootstrap.write_text(bootstrap.read_text().replace('/CHANGE_TO_YOUR_HOME_DIRECTORY/noori-private',str(private)))
    token='setup-test-'+'c'*64;password='Owner-test-password-123!'
    (private/'config.php').write_text("<?php return ['dsn'=>getenv('NOORI_TEST_DSN'),'db_user'=>getenv('NOORI_TEST_USER'),'db_password'=>getenv('NOORI_TEST_PASSWORD'),'public_url'=>'https://activation.test','setup_token'=>'"+token+"','access_policy'=>'online'];")
    (temp/'clear.php').write_text("<?php $p=new PDO(getenv('NOORI_TEST_DSN'),getenv('NOORI_TEST_USER'),getenv('NOORI_TEST_PASSWORD'));$p->exec('DELETE FROM noori_licenses;DELETE FROM noori_rate_limits');")
    subprocess.run(['php',str(temp/'clear.php')],check=True)
    router=temp/'router.php'
    router.write_text("<?php $_SERVER['HTTPS']=str_starts_with($_SERVER['REQUEST_URI'],'/__insecure')?'off':'on';$p=parse_url($_SERVER['REQUEST_URI'],PHP_URL_PATH);if(str_starts_with($p,'/__insecure'))$p=substr($p,11);$name=in_array($p,['/admin.php','/setup.php'])?basename($p):'index.php';if(!in_array($p,['/admin.php','/setup.php','/v1/activate','/v1/check','/health'])){http_response_code(404);exit;}require "+repr(str(public)).replace('\\','/')+".'/'.$name;")
    sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
    log=open(temp/'php.log','w+')
    server=subprocess.Popen(['php','-S',f'127.0.0.1:{port}',str(router)],cwd=public,stdout=log,stderr=log)
    cookie=''
    def call(path,fields=None,raw=None,content_type=None):
        nonlocal_dummy=None
        connection=http.client.HTTPConnection('127.0.0.1',port,timeout=5)
        headers={}
        if cookie:headers['Cookie']=cookie
        if fields is not None:
            import urllib.parse
            raw=urllib.parse.urlencode(fields);content_type='application/x-www-form-urlencoded'
        if raw is not None:headers['Content-Type']=content_type or 'application/json'
        connection.request('POST' if raw is not None else 'GET',path,body=raw,headers=headers)
        response=connection.getresponse();data=response.read();result=(response.status,dict(response.getheaders()),data)
        connection.close();return result
    def csrf(data):
        match=re.search(rb'name="csrf" value="([a-f0-9]+)"',data)
        assert match,'CSRF token missing';return match.group(1).decode()
    try:
        for _ in range(100):
            try:call('/health');break
            except OSError:time.sleep(.05)
        else:raise AssertionError('PHP server did not start')
        status,_,_=call('/__insecure/admin.php');assert status==403,'HTTPS required'
        status,_,_=call('/setup.php',{'token':'wrong','password':password});assert not (private/'installed.marker').exists(),'Setup token enforced'
        status,_,data=call('/setup.php',{'token':token,'password':password});assert status==200 and (private/'installed.marker').exists(),'First setup completes'
        assert call('/setup.php')[0]==403,'Setup cannot run twice'
        status,_,data=call('/health');assert status==200 and json.loads(data)['activationReady'],'Ready health'
        status,headers,data=call('/admin.php');assert status==200
        cookie=headers['Set-Cookie'].split(';')[0];assert 'secure' in headers['Set-Cookie'].lower() and 'httponly' in headers['Set-Cookie'].lower() and 'samesite=strict' in headers['Set-Cookie'].lower(),'Secure owner session'
        first_csrf=csrf(data)
        assert call('/admin.php',{'csrf':'wrong','action':'login','password':password})[0]==403,'CSRF enforced'
        status,headers,data=call('/admin.php',{'csrf':first_csrf,'action':'login','password':password});assert status==200 and b'Generate invitations' in data,'Owner login'
        cookie=headers.get('Set-Cookie',cookie).split(';')[0];current_csrf=csrf(data)
        status,headers,data=call('/admin.php',{'csrf':current_csrf,'action':'generate','count':'20','kind':'trial','email':''})
        assert status==200 and headers.get('Content-Type','').startswith('text/csv'),'CSV generation'
        keys=list(csv.DictReader(io.StringIO(data.decode())));assert len(keys)==20 and len({x['Serial'] for x in keys})==20,'20 unique keys downloaded'
        status,_,page=call('/admin.php');assert all(k['Serial'].encode() not in page for k in keys),'Full keys not shown in owner list'
        assert call('/admin.php',{'csrf':current_csrf,'action':'generate','count':'20','kind':'trial'})[0]==403,'Old form cannot replay key generation'
        current_csrf=csrf(call('/admin.php')[2])
        body={'email':'friend@example.com','serial':keys[0]['Serial'],'deviceId':'a'*64,'nonce':'b'*32}
        status,_,data=call('/v1/activate',raw=json.dumps(body));assert status==200,'Real API activation'
        signed=json.loads(data);grant=json.loads(base64.b64decode(signed['payload']));assert signed['algorithm']=='RS256' and grant['kind']=='trial' and grant['trialExpiresAt']-grant['issuedAt']==259200,'Trial signed for exactly 72h'
        status,_,_=call('/v1/activate',raw=json.dumps({**body,'deviceId':'c'*64}));assert status==403,'Second computer rejected'
        status,_,_=call('/v1/activate',raw=json.dumps({**body,'email':'other@example.com'}));assert status==403,'Second email rejected'
        assert call('/v1/activate')[0]==405,'Method enforced'
        assert call('/v1/activate',raw='x',content_type='text/plain')[0]==415,'Content type enforced'
        assert call('/v1/activate',raw='x'*5000)[0]==413,'Body size enforced'
        assert call('/admin.php',{'csrf':current_csrf,'action':'full','id':keys[0]['Owner record ID']})[0]==200,'Owner grants free full access'
        status,_,data=call('/v1/check',raw=json.dumps(body));assert status==200 and json.loads(base64.b64decode(json.loads(data)['payload']))['kind']=='full','Full access applies to existing key'
        assert call('/admin.php',{'csrf':current_csrf,'action':'revoke','id':keys[0]['Owner record ID']})[0]==200,'Owner disables key'
        assert call('/v1/check',raw=json.dumps(body))[0]==403,'Disabled key rejected'
        assert call('/signing-private.pem')[0]==404,'Private key not publicly served'
        print('PASS PHP HTTP setup, HTTPS requirement, secure owner session, CSRF, 20-key CSV, email/computer binding and owner grant/revoke')
    finally:
        server.terminate();server.wait(timeout=5);log.close()
