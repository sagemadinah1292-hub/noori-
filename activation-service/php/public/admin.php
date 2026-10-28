<?php
declare(strict_types=1);
require __DIR__.'/bootstrap.php';
if(!is_file($privateDir.'/portal.php')){http_response_code(503);exit('Upload the private portal upgrade first.');}
require_once $privateDir.'/portal.php';
if(!is_file($privateDir.'/installed.marker')){http_response_code(503);exit('Complete setup.php first.');}
nooriSession('noori_owner');
$error='';$rows=[];$publicKey='';$accounts=[];$recoveryCode='';$recoveryEmail='';
try{
 $service=nooriService();$portal=new NooriPortal(nooriDb());
 if(isset($_SESSION['owner'])&&time()-(int)($_SESSION['last_seen']??0)>1800)unset($_SESSION['owner']);
 $_SESSION['csrf']??=bin2hex(random_bytes(32));
 if($_SERVER['REQUEST_METHOD']==='POST'){
  if(!hash_equals($_SESSION['csrf'],(string)($_POST['csrf']??'')))throw new NooriError(403,'This form expired. Refresh the page.');
  $action=(string)($_POST['action']??'');
  if($action==='login'){
   if(!$service->limit('owner-login:'.($_SERVER['REMOTE_ADDR']??'unknown'),5,time()))throw new NooriError(429,'Too many login attempts. Wait one minute.');
   $owner=json_decode(file_get_contents($privateDir.'/owner.json'),true,8,JSON_THROW_ON_ERROR);
   if(!password_verify((string)($_POST['password']??''),$owner['password_hash']))throw new NooriError(403,'Incorrect owner password.');
   session_regenerate_id(true);$_SESSION['owner']=true;$_SESSION['csrf']=bin2hex(random_bytes(32));
  }elseif(!isset($_SESSION['owner']))throw new NooriError(403,'Owner login required.');
  elseif($action==='logout'){$_SESSION=[];session_destroy();header('Location: admin.php');exit;}
  elseif($action==='generate'){
   $email=trim((string)($_POST['email']??''));$keys=$service->issue((int)($_POST['count']??20),(string)($_POST['kind']??'full'),$email===''?null:$email);
   $_SESSION['csrf']=bin2hex(random_bytes(32));session_write_close();
   header('Content-Type: text/csv; charset=utf-8');header('Content-Disposition: attachment; filename="NOORI-private-friend-keys.csv"');
   $out=fopen('php://output','wb');fputcsv($out,['Serial','Type','Email','Owner record ID'],',','"','');foreach($keys as $key)fputcsv($out,[$key['serial'],$key['kind'],$key['email'],$key['id']],',','"','');fclose($out);exit;
  }elseif($action==='recovery'){
   $id=(string)($_POST['id']??'');$account=$portal->account($id);if(!$account)throw new NooriError(404,'Account not found.');$recoveryCode=$portal->recovery($id,time());$recoveryEmail=$account['email'];$_SESSION['csrf']=bin2hex(random_bytes(32));
  }else{$service->change((string)($_POST['id']??''),$action);}
 }
 if(isset($_SESSION['owner'])){$_SESSION['last_seen']=time();$rows=$service->all();$accounts=$portal->accounts();$publicKey=trim(file_get_contents($privateDir.'/public-modulus.txt'));}
}catch(NooriError $e){http_response_code($e->status);$error=$e->getMessage();}
catch(Throwable){http_response_code(503);$error='The owner service is unavailable. Check the private configuration and database.';}
?><!doctype html><html lang="en"><meta charset="utf-8"><title>NOORI friend keys</title>
<style>body{margin:32px;background:#123d40;color:#e5efe7;font:15px system-ui}input,select,button,textarea{padding:10px;margin:5px}button{background:#d9c370;border:0;cursor:pointer}table{border-collapse:collapse;width:100%}td,th{padding:10px;border-bottom:1px solid #527675;text-align:left;vertical-align:top}form.inline{display:inline}textarea{width:95%;height:100px}a{color:#f0df9b}.message{color:#ffe2a3}label{display:inline-block}section{margin:28px 0}</style>
<h1>NOORI — friends-only keys</h1><p class="message"><?=esc($error)?></p>
<?php if(!isset($_SESSION['owner'])):?><form method="post"><input type="hidden" name="csrf" value="<?=esc($_SESSION['csrf']??'')?>"><input type="hidden" name="action" value="login"><label>Owner password <input name="password" type="password" required autocomplete="current-password"></label><button>Sign in</button></form>
<?php else:?>
<form method="post"><input type="hidden" name="csrf" value="<?=esc($_SESSION['csrf'])?>"><button name="action" value="logout">Sign out</button></form>
<section><h2>Download website</h2><p><a href="secure-downloads.php">Review and secure the website upgrade →</a> · <a href="./">Open the friend portal →</a></p><p>Friends register with email, invitation key and a password, then sign in to download. Website registration reserves their email without starting the plugin trial or binding a computer.</p></section>
<section><h2>Friend accounts</h2><?php if($recoveryCode):?><p>One-time recovery code for <?=esc($recoveryEmail)?>. Valid for 15 minutes; send it privately and ask the friend to open reset-password.php. This code is shown once.</p><textarea readonly><?=esc($recoveryCode)?></textarea><?php endif;?><table><thead><tr><th>Email</th><th>Access</th><th>Account created (UTC)</th><th>Recovery</th></tr></thead><tbody><?php foreach($accounts as $account):?><tr><td><?=esc($account['email'])?></td><td><?=esc($account['kind'])?><?=((int)$account['revoked'])?' — disabled':''?></td><td><?=esc(gmdate('Y-m-d H:i',(int)$account['created_at']))?></td><td><form method="post"><input type="hidden" name="csrf" value="<?=esc($_SESSION['csrf'])?>"><input type="hidden" name="id" value="<?=esc($account['id'])?>"><button name="action" value="recovery">Create recovery code</button></form></td></tr><?php endforeach;?></tbody></table></section>
<section><h2>Generate invitations</h2><p>Each key binds to the first registered email and computer. Send one key privately to each friend through Gmail or Live.com. There are no payment steps.</p><p>Download and keep the CSV. Full serials are not stored in the database and cannot be recovered. Refresh this page after downloading.</p>
<form method="post"><input type="hidden" name="csrf" value="<?=esc($_SESSION['csrf'])?>"><input type="hidden" name="action" value="generate"><label>Keys <input name="count" type="number" min="1" max="100" value="20"></label><label>Type <select name="kind"><option value="trial">Three-day trial</option><option value="full">Free full access</option></select></label><label>Assign email (one key only) <input name="email" type="email" placeholder="Optional for a single key"></label><button>Generate and download CSV</button></form></section>
<section><h2>Registered keys</h2><p>Grant full access after a trial, disable a key, or explicitly reset its computer binding. Resetting a computer never restarts a trial. Changes take effect by the next online lease renewal (up to 15 minutes).</p>
<table><thead><tr><th>Key ending / email</th><th>Access</th><th>Computer</th><th>Trial expires (UTC)</th><th>Owner actions</th></tr></thead><tbody>
<?php foreach($rows as $row):?><tr><td><?=esc($row['key_hint'])?><br><?=esc($row['email']??'Not registered yet')?></td><td><?=esc($row['kind'])?><?=((int)$row['revoked'])?' — disabled':''?></td><td><?=esc($row['device_id']?substr($row['device_id'],0,12).'…':'Not activated')?></td><td><?=esc($row['expires_at']===null?'—':gmdate('Y-m-d H:i',(int)$row['expires_at']))?></td><td><?php foreach(['full'=>'Grant full access','revoke'=>'Disable','restore'=>'Enable','reset'=>'Reset computer'] as $action=>$label):?><form class="inline" method="post"><input type="hidden" name="csrf" value="<?=esc($_SESSION['csrf'])?>"><input type="hidden" name="id" value="<?=esc($row['id'])?>"><button name="action" value="<?=esc($action)?>"><?=esc($label)?></button></form><?php endforeach;?></td></tr><?php endforeach;?></tbody></table></section>
<section><h2>Plugin build configuration — public information</h2><p>Activation API: <code><?=esc($config['public_url'])?></code></p><p>This public modulus may be shared for the plugin build. Never share signing-private.pem or config.php.</p><textarea readonly><?=esc($publicKey)?></textarea></section>
<?php endif;?></html>
