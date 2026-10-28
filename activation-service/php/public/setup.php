<?php
declare(strict_types=1);
require __DIR__.'/bootstrap.php';
$error='';$completed=false;
if(is_file($privateDir.'/installed.marker')){http_response_code(403);exit('Setup is already complete. Use admin.php.');}
if($_SERVER['REQUEST_METHOD']==='POST'){
 try{
  $expected=$config['setup_token']??'';
  if(strlen($expected)<32||str_starts_with($expected,'CHANGE')||!hash_equals($expected,(string)($_POST['token']??'')))throw new RuntimeException('Incorrect or unconfigured setup token.');
  $password=(string)($_POST['password']??'');if(strlen($password)<14)throw new RuntimeException('Choose an owner password of at least 14 characters.');
  $lock=fopen($privateDir.'/install.lock','c');if(!$lock||!flock($lock,LOCK_EX))throw new RuntimeException('Cannot lock the private directory.');
  if(is_file($privateDir.'/installed.marker'))throw new RuntimeException('Setup has already completed.');
  $pdo=nooriDb();foreach(explode(';',file_get_contents($privateDir.'/schema.mysql.sql')) as $statement)if(trim($statement)!=='')$pdo->exec($statement);
  $key=openssl_pkey_new(['private_key_bits'=>2048,'private_key_type'=>OPENSSL_KEYTYPE_RSA]);if(!$key||!openssl_pkey_export($key,$pem))throw new RuntimeException('Server OpenSSL key generation failed.');
  $details=openssl_pkey_get_details($key);if(!isset($details['rsa']['n'])||strlen($details['rsa']['n'])!==256)throw new RuntimeException('Invalid RSA key.');
  if(file_put_contents($privateDir.'/signing-private.pem',$pem,LOCK_EX)===false)throw new RuntimeException('Cannot save private key.');chmod($privateDir.'/signing-private.pem',0600);
  if(file_put_contents($privateDir.'/owner.json',json_encode(['password_hash'=>password_hash($password,PASSWORD_DEFAULT)],JSON_THROW_ON_ERROR),LOCK_EX)===false)throw new RuntimeException('Cannot save owner credentials.');chmod($privateDir.'/owner.json',0600);
  if(file_put_contents($privateDir.'/public-modulus.txt',bin2hex($details['rsa']['n'])."\n",LOCK_EX)===false)throw new RuntimeException('Cannot save public key.');
  if(file_put_contents($privateDir.'/installed.marker','installed',LOCK_EX)===false)throw new RuntimeException('Cannot finish setup.');
  flock($lock,LOCK_UN);fclose($lock);$completed=true;
 }catch(Throwable $e){$error='Setup failed. Check the setup token, database settings, OpenSSL and private-directory permissions.';}
}
?><!doctype html><html lang="en"><meta charset="utf-8"><title>NOORI activation setup</title>
<style>body{max-width:720px;margin:50px auto;background:#123d40;color:#e2eee7;font:17px system-ui}label,input,button{display:block;margin:16px 0}input{width:95%;padding:12px}button{padding:14px;background:#d9c370;border:0}p{line-height:1.6}</style>
<h1>NOORI — private friends activation</h1>
<?php if($completed):?><p>Setup completed. Delete setup.php from the public server. Keep the private signing key and database backed up.</p><p>Open <a href="admin.php">owner administration</a>. The owner page displays the public key needed to compile the protected plugin. Never share the private key or database password.</p>
<?php else:?><p>This first-time setup generates the server signing key and your owner login. It does not send emails or charge anyone.</p><p><?=esc($error)?></p><form method="post" autocomplete="off"><label>Setup token from your private config<input name="token" type="password" required></label><label>Create owner password (14+ characters)<input name="password" type="password" minlength="14" required autocomplete="new-password"></label><button>Create activation service</button></form><?php endif;?></html>
