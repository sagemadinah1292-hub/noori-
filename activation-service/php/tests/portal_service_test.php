<?php
declare(strict_types=1);
require __DIR__.'/../private/service.php';require __DIR__.'/../private/portal.php';
function ok(bool $value,string $message):void{if(!$value)throw new RuntimeException($message);}
function denied(callable $call,int $status):void{try{$call();throw new RuntimeException('Expected rejection.');}catch(NooriError $e){ok($e->status===$status,'Expected HTTP '.$status.', got '.$e->status);}}
$dsn=getenv('NOORI_TEST_DSN')?:'sqlite::memory:';$db=new PDO($dsn,getenv('NOORI_TEST_USER')?:null,getenv('NOORI_TEST_PASSWORD')?:null);$db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);
if($db->getAttribute(PDO::ATTR_DRIVER_NAME)==='mysql'){foreach(explode(';',file_get_contents(__DIR__.'/../private/schema.mysql.sql')) as $q)if(trim($q)!=='')$db->exec($q);}
else{$db->exec('CREATE TABLE noori_licenses(id TEXT PRIMARY KEY,email TEXT NULL,key_hash TEXT UNIQUE,key_hint TEXT,kind TEXT,created_at INTEGER,activated_at INTEGER NULL,expires_at INTEGER NULL,device_id TEXT NULL,revoked INTEGER DEFAULT 0);CREATE UNIQUE INDEX one_trial_per_email ON noori_licenses(email) WHERE kind=\'trial\';CREATE TABLE noori_rate_limits(bucket TEXT PRIMARY KEY,requests INTEGER,reset_at INTEGER)');}
$portal=new NooriPortal($db);$db->exec('DELETE FROM noori_requests;DELETE FROM noori_accounts;DELETE FROM noori_licenses;DELETE FROM noori_rate_limits');
$key=openssl_pkey_new(['private_key_bits'=>2048,'private_key_type'=>OPENSSL_KEYTYPE_RSA]);openssl_pkey_export($key,$pem);$service=new NooriService($db,$pem);$now=time();$password='Friend-password-123!';
$invitation=$service->issue(1,'trial',null,$now)[0];
denied(fn()=>$portal->register('friend@example.com',$password,'NOORI-T-'.str_repeat('F',32),$now),403);
$proof=$portal->invitation(' Friend@Example.com ',$invitation['serial'],$now);
ok(!isset($proof['serial'])&&$db->query('SELECT COUNT(*) FROM noori_accounts')->fetchColumn()==0,'First step stores no raw serial or account.');
denied(fn()=>$portal->completeRegistration($proof,$password,$now+600),403);
$account=$portal->completeRegistration($proof,$password,$now);ok($account['email']==='friend@example.com','Email normalized.');ok($account['device_id']===null&&$account['activated_at']===null&&$account['expires_at']===null,'Website signup must not bind a Windows machine or start a trial.');
ok(NooriPortal::mayDownload($account,$now),'Unused trial invitation can download.');
$hash=$db->query('SELECT password_hash FROM noori_accounts')->fetchColumn();ok($hash!==$password&&password_verify($password,$hash),'Only password hash stored.');
denied(fn()=>$portal->register('other@example.com',$password,$invitation['serial'],$now),403);
denied(fn()=>$portal->register('friend@example.com',$password,$invitation['serial'],$now),409);
denied(fn()=>$portal->login('friend@example.com','wrong',$now),403);ok($portal->login('FRIEND@example.com',$password,$now)['id']===$account['id'],'Login works.');
$body=['email'=>'friend@example.com','serial'=>$invitation['serial'],'deviceId'=>str_repeat('a',64),'nonce'=>str_repeat('b',32)];
denied(fn()=>$service->activate([...$body,'email'=>'other@example.com'],true,$now),403);
$grant=$service->activate($body,true,$now);$payload=json_decode(base64_decode($grant['payload']),true);ok($payload['trialExpiresAt']===$now+259200,'Plugin activation starts original 72-hour trial.');
$service->change($invitation['id'],'revoke');ok(!NooriPortal::mayDownload($portal->account($account['id']),$now),'Revocation blocks an existing web session.');$service->change($invitation['id'],'restore');
ok(!NooriPortal::mayDownload($portal->account($account['id']),$now+259200),'Expired trial blocks download.');$service->change($invitation['id'],'full');ok(NooriPortal::mayDownload($portal->account($account['id']),$now+999999),'Free full access remains downloadable.');
denied(fn()=>$portal->changePassword($account['id'],'wrong','New-password-456!'),403);$version=$portal->changePassword($account['id'],$password,'New-password-456!');ok($version===2,'Password change invalidates old sessions.');denied(fn()=>$portal->login('friend@example.com',$password,$now),403);
$code=$portal->recovery($account['id'],$now);ok(!str_contains((string)$db->query('SELECT reset_hash FROM noori_accounts')->fetchColumn(),$code),'Recovery code stored as hash.');denied(fn()=>$portal->reset('friend@example.com',$code,'Reset-password-789!',$now+900),403);
$code=$portal->recovery($account['id'],$now+901);$portal->reset('friend@example.com',$code,'Reset-password-789!',$now+902);denied(fn()=>$portal->reset('friend@example.com',$code,'Another-password-123!',$now+903),403);ok($portal->login('friend@example.com','Reset-password-789!',$now+903)['auth_version']===3,'One-time password recovery increments session version.');
echo 'PASS portal on '.$db->getAttribute(PDO::ATTR_DRIVER_NAME).": invitation-only signup, hashed passwords, login, no premature trial/device binding, revocation/expiry, password changes and one-time recovery\n";

$config=[];
$pending=$portal->pendingRegistration('pending@example.com',$password,$now);
ok(!NooriPortal::mayDownload($pending,$now),'New signup denied downloads.');
ok($portal->login('pending@example.com',$password,$now)['kind']==='pending','Pending account can sign in.');
$unassigned=$service->issue(1,'full',null,$now)[0];denied(fn()=>$portal->redeem($pending['id'],$unassigned['serial'],$now),403);
$wrong=$service->issue(1,'full','different@example.com',$now)[0];denied(fn()=>$portal->redeem($pending['id'],$wrong['serial'],$now),403);
$final=$service->issue(1,'full','pending@example.com',$now)[0];
$approved=$portal->redeem($pending['id'],$final['serial'],$now);ok(NooriPortal::mayDownload($approved,$now),'Only owner email-assigned key unlocks download.');
ok($approved['device_id']===null&&$approved['activated_at']===null,'Approval does not bind device.');
ok($portal->login('pending@example.com',$password,$now)['id']===$approved['id'],'Password and account preserved.');
denied(fn()=>$portal->redeem($pending['id'],$final['serial'],$now),403);
echo "PASS pending account approval, wrong/unassigned key denial, final key redemption and preserved login\n";

ok(!$portal->notifyOwner($portal->pendingRegistration('notice@example.com',$password,$now),[]),'No configured sender is reported as not sent, never delivered.');
$requests=$portal->requests();ok(count($requests)===1&&$requests[0]['notification_status']==='not_sent','Pending notification failure is visible to owner.');
$reset=$portal->recovery($requests[0]['id'],$now);$portal->reset('notice@example.com',$reset,'Pending-reset-123!',$now+1);ok($portal->login('notice@example.com','Pending-reset-123!',$now+1)['kind']==='pending','Pending recovery never approves downloads.');
echo "PASS notification failure status and pending password recovery\n";
