<?php
declare(strict_types=1);
require __DIR__.'/bootstrap.php';
if(!is_file($privateDir.'/installed.marker')){http_response_code(503);exit('Complete setup.php first.');}
ini_set('session.use_strict_mode','1');session_name('noori_owner');session_set_cookie_params(['secure'=>true,'httponly'=>true,'samesite'=>'Strict','path'=>(parse_url($config['public_url'],PHP_URL_PATH)?:'/')]);session_start();
$error='';$rows=[];$publicKey='';
try{
 $service=nooriService();
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
  }else{$service->change((string)($_POST['id']??''),$action);}
 }
 if(isset($_SESSION['owner'])){$_SESSION['last_seen']=time();$rows=$service->all();$publicKey=trim(file_get_contents($privateDir.'/public-modulus.txt'));}
}catch(NooriError $e){http_response_code($e->status);$error=$e->getMessage();}
catch(Throwable){http_response_code(503);$error='The owner service is unavailable. Check the private configuration and database.';}
?><!doctype html><html lang="en"><meta charset="utf-8"><title>NOORI friend keys</title>
<style>body{margin:32px;background:#123d40;color:#e5efe7;font:15px system-ui}input,select,button,textarea{padding:10px;margin:5px}button{background:#d9c370;border:0;cursor:pointer}table{border-collapse:collapse;width:100%}td,th{padding:10px;border-bottom:1px solid #527675;text-align:left;vertical-align:top}form.inline{display:inline}textarea{width:95%;height:100px}a{color:#f0df9b}.message{color:#ffe2a3}label{display:inline-block}section{margin:28px 0}</style>
<h1>NOORI — friends-only keys</h1><p class="message"><?=esc($error)?></p>
<?php if(!isset($_SESSION['owner'])):?><form method="post"><input type="hidden" name="csrf" value="<?=esc($_SESSION['csrf']??'')?>"><input type="hidden" name="action" value="login"><label>Owner password <input name="password" type="password" required autocomplete="current-password"></label><button>Sign in</button></form>
<?php else:?>
<form method="post"><input type="hidden" name="csrf" value="<?=esc($_SESSION['csrf'])?>"><button name="action" value="logout">Sign out</button></form>
<section><h2>Generate invitations</h2><p>Each key binds to the first registered email and computer. Send one key privately to each friend through Gmail or Live.com. There are no payment steps.</p><p>Download and keep the CSV. Full serials are not stored in the database and cannot be recovered. Refresh this page after downloading.</p>
<form method="post"><input type="hidden" name="csrf" value="<?=esc($_SESSION['csrf'])?>"><input type="hidden" name="action" value="generate"><label>Keys <input name="count" type="number" min="1" max="100" value="20"></label><label>Type <select name="kind"><option value="trial">Three-day trial</option><option value="full">Free full access</option></select></label><label>Assign email (one key only) <input name="email" type="email" placeholder="Optional for a single key"></label><button>Generate and download CSV</button></form></section>
<section><h2>Registered keys</h2><p>Grant full access after a trial, disable a key, or explicitly reset its computer binding. Resetting a computer never restarts a trial. Changes take effect by the next online lease renewal (up to 15 minutes).</p>
<table><thead><tr><th>Key ending / email</th><th>Access</th><th>Computer</th><th>Trial expires (UTC)</th><th>Owner actions</th></tr></thead><tbody>
<?php foreach($rows as $row):?><tr><td><?=esc($row['key_hint'])?><br><?=esc($row['email']??'Not registered yet')?></td><td><?=esc($row['kind'])?><?=((int)$row['revoked'])?' — disabled':''?></td><td><?=esc($row['device_id']?substr($row['device_id'],0,12).'…':'Not activated')?></td><td><?=esc($row['expires_at']===null?'—':gmdate('Y-m-d H:i',(int)$row['expires_at']))?></td><td><?php foreach(['full'=>'Grant full access','revoke'=>'Disable','restore'=>'Enable','reset'=>'Reset computer'] as $action=>$label):?><form class="inline" method="post"><input type="hidden" name="csrf" value="<?=esc($_SESSION['csrf'])?>"><input type="hidden" name="id" value="<?=esc($row['id'])?>"><button name="action" value="<?=esc($action)?>"><?=esc($label)?></button></form><?php endforeach;?></td></tr><?php endforeach;?></tbody></table></section>
<section><h2>Plugin build configuration — public information</h2><p>Activation API: <code><?=esc($config['public_url'])?></code></p><p>This public modulus may be shared for the plugin build. Never share signing-private.pem or config.php.</p><textarea readonly><?=esc($publicKey)?></textarea></section>
<?php endif;?></html>
