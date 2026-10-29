<?php
declare(strict_types=1);
require __DIR__.'/../private/service.php';
function check(bool $condition,string $message):void {if(!$condition)throw new RuntimeException($message);}
$db=new PDO(getenv('NOORI_TEST_DSN')?:'sqlite::memory:',getenv('NOORI_TEST_USER')?:null,getenv('NOORI_TEST_PASSWORD')?:null,[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION]);
if($db->getAttribute(PDO::ATTR_DRIVER_NAME)==='mysql'){
 foreach(explode(';',file_get_contents(__DIR__.'/../private/schema.mysql.sql')) as $sql)if(trim($sql)!=='')$db->exec($sql);
}else{
 $db->exec("CREATE TABLE noori_licenses(id TEXT PRIMARY KEY,email TEXT,key_hash TEXT UNIQUE,key_hint TEXT,kind TEXT,created_at INTEGER,activated_at INTEGER,expires_at INTEGER,device_id TEXT,revoked INTEGER DEFAULT 0);CREATE UNIQUE INDEX one_trial_per_email ON noori_licenses(email) WHERE kind='trial';CREATE TABLE noori_rate_limits(bucket TEXT PRIMARY KEY,requests INTEGER,reset_at INTEGER)");
}
$key=openssl_pkey_new(['private_key_bits'=>2048,'private_key_type'=>OPENSSL_KEYTYPE_RSA]);openssl_pkey_export($key,$pem);$public=openssl_pkey_get_details($key)['key'];$service=new NooriService($db,$pem);
$keys=$service->issue(20,'trial',null,1700000000);check(count($keys)===20,'20 keys');check(count(array_unique(array_column($keys,'serial')))===20,'20 distinct random serials');
$base=['email'=>'friend@example.com','serial'=>$keys[0]['serial'],'deviceId'=>str_repeat('a',64),'nonce'=>str_repeat('b',32)];$now=1800000000;
$token=$service->activate($base,true,$now);$grant=json_decode(base64_decode($token['payload']),true);check($grant['trialExpiresAt']===$now+259200,'Trial is exactly three days from first activation');check($grant['validUntil']===$now+900,'Online session lease');check($token['algorithm']==='RS256','PHP signature algorithm');check(openssl_verify(base64_decode($token['payload']),base64_decode($token['signature']),$public,OPENSSL_ALGO_SHA256)===1,'Signed grant verifies');
$again=json_decode(base64_decode($service->activate($base,true,$now+200)['payload']),true);check($again['trialExpiresAt']===$grant['trialExpiresAt'],'Reactivation never restarts trial');
function rejected(callable $call,string $message):void {try{$call();}catch(NooriError){return;}throw new RuntimeException($message);}
rejected(fn()=>$service->activate([...$base,'deviceId'=>str_repeat('c',64)],true,$now+201),'Another computer rejected');
rejected(fn()=>$service->activate([...$base,'email'=>'other@example.com'],true,$now+202),'Another email rejected');
rejected(fn()=>$service->activate([...$base,'serial'=>$keys[1]['serial']],true,$now+203),'Second trial for same email rejected');
rejected(fn()=>$service->activate($base,false,$now+259200),'Trial expires at deadline');
$service->change($keys[0]['id'],'reset');$moved=json_decode(base64_decode($service->activate([...$base,'deviceId'=>str_repeat('c',64)],true,$now+204)['payload']),true);check($moved['trialExpiresAt']===$grant['trialExpiresAt'],'Computer reset never extends trial');
$service->change($keys[0]['id'],'full');$full=json_decode(base64_decode($service->activate([...$base,'deviceId'=>str_repeat('c',64)],false,$now+205)['payload']),true);check($full['kind']==='full'&&$full['trialExpiresAt']===0,'Owner grants free full access without changing key');
$service->change($keys[0]['id'],'revoke');rejected(fn()=>$service->activate([...$base,'deviceId'=>str_repeat('c',64)],false,$now+206),'Revocation enforced');
$service->change($keys[0]['id'],'restore');check(count($service->all())===20,'Owner list');
$preassigned=$service->issue(1,'full','assigned@example.com',$now)[0];rejected(fn()=>$service->activate([...$base,'serial'=>$preassigned['serial']],true,$now),'Assigned-email keys reject other emails');
for($i=0;$i<5;$i++)check($service->limit('owner-test',5,$now),'Allowed owner login attempt');check(!$service->limit('owner-test',5,$now),'Owner login rate limit');check($service->limit('owner-test',5,$now+61),'Rate limit resets');
check(openssl_verify(base64_decode($token['payload']).'x',base64_decode($token['signature']),$public,OPENSSL_ALGO_SHA256)!==1,'Tampered grant fails signature');
echo "PASS PHP activation on ".$db->getAttribute(PDO::ATTR_DRIVER_NAME).": 20 keys, email/computer binding, 72-hour expiry, replay-safe reactivation, owner controls and signatures\n";
