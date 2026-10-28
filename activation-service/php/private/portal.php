<?php
declare(strict_types=1);

final class NooriPortal {
 public function __construct(private PDO $db) {
  $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);
  $db->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE,PDO::FETCH_ASSOC);
  $db->exec("CREATE TABLE IF NOT EXISTS noori_requests (id VARCHAR(36) PRIMARY KEY,email VARCHAR(254) NOT NULL UNIQUE,password_hash VARCHAR(255) NOT NULL,created_at BIGINT NOT NULL,auth_version INT NOT NULL DEFAULT 1,approved INT NOT NULL DEFAULT 0,notification_status VARCHAR(32) NOT NULL DEFAULT 'not_sent',reset_hash VARCHAR(64) NULL,reset_expires_at BIGINT NULL)");
  if($db->getAttribute(PDO::ATTR_DRIVER_NAME)==='mysql'){
   $db->setAttribute(PDO::ATTR_EMULATE_PREPARES,false);
   $db->exec("CREATE TABLE IF NOT EXISTS noori_accounts (id CHAR(36) PRIMARY KEY,email VARCHAR(254) COLLATE utf8mb4_bin NOT NULL UNIQUE,password_hash VARCHAR(255) NOT NULL,license_id CHAR(36) NOT NULL UNIQUE,created_at BIGINT NOT NULL,last_login_at BIGINT NULL,auth_version INT NOT NULL DEFAULT 1,reset_hash CHAR(64) NULL,reset_expires_at BIGINT NULL) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
  }else{
   $db->exec('CREATE TABLE IF NOT EXISTS noori_accounts (id TEXT PRIMARY KEY,email TEXT NOT NULL UNIQUE,password_hash TEXT NOT NULL,license_id TEXT NOT NULL UNIQUE,created_at INTEGER NOT NULL,last_login_at INTEGER NULL,auth_version INTEGER NOT NULL DEFAULT 1,reset_hash TEXT NULL,reset_expires_at INTEGER NULL)');
  }
 }
 public function pendingRegistration(string $inputEmail,string $password,int $now):array {
  self::password($password);$email=NooriService::email($inputEmail);
  if($email===null||$email===nooriOwnerId())throw new NooriError(400,'Enter a valid personal email.');
  $q=$this->db->prepare('SELECT id FROM noori_accounts WHERE email=?');$q->execute([$email]);if($q->fetch())throw new NooriError(409,'An account already exists. Sign in.');
  $id=NooriService::uuid();try{$q=$this->db->prepare('INSERT INTO noori_requests(id,email,password_hash,created_at) VALUES(?,?,?,?)');$q->execute([$id,$email,password_hash($password,PASSWORD_DEFAULT),$now]);}catch(PDOException $e){if(in_array((string)$e->getCode(),['23000','19'],true))throw new NooriError(409,'An account already exists. Sign in.');throw $e;}
  return $this->account($id)??throw new RuntimeException('Account unavailable.');
 }
 public function requests():array {return $this->db->query('SELECT id,email,created_at,notification_status FROM noori_requests WHERE approved=0 ORDER BY created_at DESC LIMIT 1000')->fetchAll();}
 public function notifyOwner(array $user,array $config):bool {
  $recipient=(string)($config['owner_notification_email']??'shahidulhaq@live.com');
  $sender=(string)($config['notification_from']??'');$sent=false;
  if(filter_var($recipient,FILTER_VALIDATE_EMAIL)&&filter_var($sender,FILTER_VALIDATE_EMAIL)&&!preg_match('/[\r\n]/',$recipient.$sender)){
   $body="A new NOORI account is awaiting your approval.\nEmail: ".$user['email']."\nSign in through your NOORI website to review it.\nNo download access has been granted.\n";
   try{$sent=function_exists('mail')&&@mail($recipient,'NOORI: new account awaiting approval',$body,'From: '.$sender."\r\nContent-Type: text/plain; charset=UTF-8");}catch(Throwable){$sent=false;}
  }
  $q=$this->db->prepare('UPDATE noori_requests SET notification_status=? WHERE id=?');$q->execute([$sent?'accepted_by_mail_server':'not_sent',$user['id']]);return $sent;
 }
 public function redeem(string $id,string $serial,int $now):array {
  $user=$this->account($id);if(!$user||$user['kind']!=='pending')throw new NooriError(403,'No pending request.');
  $proof=$this->invitation($user['email'],$serial,$now);$this->db->beginTransaction();try{
   $q=$this->db->prepare($this->locked('SELECT * FROM noori_requests WHERE id=?'));$q->execute([$id]);$r=$q->fetch();if(!$r||(int)$r['approved']!==0)throw new NooriError(403,'Request already processed.');
   $q=$this->db->prepare($this->locked('SELECT * FROM noori_licenses WHERE key_hash=?'));$q->execute([$proof['key_hash']]);$license=$q->fetch();self::validInvitation($license,$r['email'],$now);
   // Final access keys must be explicitly assigned by the owner to this email.
   if($license['email']!==$r['email'])throw new NooriError(403,'Ask the owner for a final access key assigned to your email.');
   $q=$this->db->prepare('INSERT INTO noori_accounts(id,email,password_hash,license_id,created_at,auth_version) VALUES(?,?,?,?,?,?)');$q->execute([$id,$r['email'],$r['password_hash'],$license['id'],$r['created_at'],$r['auth_version']+1]);
   $q=$this->db->prepare('UPDATE noori_requests SET approved=1 WHERE id=?');$q->execute([$id]);$this->db->commit();return $this->account($id);
  }catch(Throwable $e){if($this->db->inTransaction())$this->db->rollBack();throw $e;}
 }
 private function locked(string $sql):string {return $sql.($this->db->getAttribute(PDO::ATTR_DRIVER_NAME)==='mysql'?' FOR UPDATE':'');}
 private static function password(string $value):void {if(strlen($value)<12||strlen($value)>72)throw new NooriError(400,'Choose a password between 12 and 72 bytes.');}
 public function invitation(string $inputEmail,string $serial,int $now):array {
  $email=NooriService::email($inputEmail);$serial=strtoupper(trim($serial));
  if($email===null||!preg_match('/^NOORI-[TF]-[A-F0-9]{32}$/D',$serial))throw new NooriError(400,'Enter your email and a valid NOORI invitation key.');
  $hash=hash('sha256',$serial);$s=$this->db->prepare('SELECT * FROM noori_licenses WHERE key_hash=?');$s->execute([$hash]);$license=$s->fetch();
  self::validInvitation($license,$email,$now);
  return ['email'=>$email,'key_hash'=>$hash,'license_id'=>$license['id'],'expires'=>$now+600];
 }
 private static function validInvitation(array|false $license,string $email,int $now):void {
  if(!$license||($license['email']!==null&&$license['email']!==$email)||(int)$license['revoked']!==0||($license['kind']==='trial'&&$license['expires_at']!==null&&(int)$license['expires_at']<=$now))throw new NooriError(403,'This invitation is unavailable for this email. Contact NOORI.');
 }
 public function register(string $inputEmail,string $password,string $serial,int $now):array {return $this->completeRegistration($this->invitation($inputEmail,$serial,$now),$password,$now);}
 public function completeRegistration(array $proof,string $password,int $now):array {
  self::password($password);$email=NooriService::email($proof['email']??null);
  if($email===null||!is_string($proof['key_hash']??null)||!preg_match('/^[a-f0-9]{64}$/D',$proof['key_hash'])||(int)($proof['expires']??0)<=$now)throw new NooriError(403,'Registration details expired. Please start again.');
  $passwordHash=password_hash($password,PASSWORD_DEFAULT);$this->db->beginTransaction();
  try{
   $s=$this->db->prepare($this->locked('SELECT * FROM noori_licenses WHERE key_hash=?'));$s->execute([$proof['key_hash']]);$license=$s->fetch();self::validInvitation($license,$email,$now);
   if($license['id']!==($proof['license_id']??''))throw new NooriError(403,'Registration details are invalid.');
   $id=NooriService::uuid();$s=$this->db->prepare('INSERT INTO noori_accounts(id,email,password_hash,license_id,created_at) VALUES(?,?,?,?,?)');$s->execute([$id,$email,$passwordHash,$license['id'],$now]);
   $s=$this->db->prepare('UPDATE noori_licenses SET email=? WHERE id=?');$s->execute([$email,$license['id']]);
   $this->db->commit();return $this->account($id)??throw new RuntimeException('Account unavailable.');
  }catch(Throwable $e){if($this->db->inTransaction())$this->db->rollBack();if($e instanceof PDOException&&in_array((string)$e->getCode(),['23000','19'],true))throw new NooriError(409,'An account or trial already exists. Sign in or contact NOORI.');throw $e;}
 }
 public function login(string $inputEmail,string $password,int $now):array {
  $email=NooriService::email($inputEmail);$s=$this->db->prepare('SELECT id,password_hash FROM noori_accounts WHERE email=?');$s->execute([$email??'']);$row=$s->fetch();
  if(!$row){$s=$this->db->prepare('SELECT id,password_hash FROM noori_requests WHERE email=? AND approved=0');$s->execute([$email??'']);$row=$s->fetch();}
  // Always perform a password check, including for unknown accounts.
  $hash=$row['password_hash']??'$2y$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2uheWG/igi.';
  if(!password_verify($password,$hash)||!$row)throw new NooriError(403,'The email or password is incorrect.');
  $s=$this->db->prepare('UPDATE noori_accounts SET last_login_at=? WHERE id=?');$s->execute([$now,$row['id']]);return $this->account($row['id'])??throw new NooriError(403,'Account unavailable.');
 }
 public function account(string $id):?array {
  $s=$this->db->prepare('SELECT a.id,a.email,a.license_id,a.created_at,a.last_login_at,a.auth_version,l.kind,l.key_hint,l.revoked,l.device_id,l.activated_at,l.expires_at FROM noori_accounts a JOIN noori_licenses l ON l.id=a.license_id WHERE a.id=?');$s->execute([$id]);$row=$s->fetch();if($row)return $row;
  $s=$this->db->prepare('SELECT id,email,created_at,auth_version FROM noori_requests WHERE id=? AND approved=0');$s->execute([$id]);$row=$s->fetch();return $row?array_merge($row,['kind'=>'pending','revoked'=>1,'license_id'=>null,'key_hint'=>'Not issued','device_id'=>null,'activated_at'=>null,'expires_at'=>null]):null;
 }
 public static function mayDownload(array $account,int $now):bool {return $account['kind']!=='pending'&&(int)$account['revoked']===0&&($account['kind']!=='trial'||$account['expires_at']===null||(int)$account['expires_at']>$now);}
 public function changePassword(string $id,string $old,string $new):int {
  self::password($new);
  if(($this->account($id)['kind']??'')==='pending'){
   $s=$this->db->prepare('SELECT password_hash,auth_version FROM noori_requests WHERE id=? AND approved=0');$s->execute([$id]);$r=$s->fetch();if(!$r||!password_verify($old,$r['password_hash']))throw new NooriError(403,'Current password is incorrect.');
   $s=$this->db->prepare('UPDATE noori_requests SET password_hash=?,auth_version=auth_version+1 WHERE id=? AND approved=0 AND auth_version=?');$s->execute([password_hash($new,PASSWORD_DEFAULT),$id,$r['auth_version']]);if($s->rowCount()!==1)throw new NooriError(409,'Account changed. Sign in again.');return (int)$r['auth_version']+1;
  }
  $this->db->beginTransaction();try{
   $s=$this->db->prepare($this->locked('SELECT password_hash,auth_version FROM noori_accounts WHERE id=?'));$s->execute([$id]);$row=$s->fetch();
   if(!$row||!password_verify($old,$row['password_hash']))throw new NooriError(403,'Current password is incorrect.');
   $version=(int)$row['auth_version']+1;$s=$this->db->prepare('UPDATE noori_accounts SET password_hash=?,auth_version=?,reset_hash=NULL,reset_expires_at=NULL WHERE id=?');$s->execute([password_hash($new,PASSWORD_DEFAULT),$version,$id]);$this->db->commit();return $version;
  }catch(Throwable $e){if($this->db->inTransaction())$this->db->rollBack();throw $e;}
 }
 public function recovery(string $id,int $now):string {
  if(!$this->account($id))throw new NooriError(404,'Account not found.');$code=bin2hex(random_bytes(32));$table=($this->account($id)['kind']==='pending')?'noori_requests':'noori_accounts';$s=$this->db->prepare('UPDATE '.$table.' SET reset_hash=?,reset_expires_at=? WHERE id=?');$s->execute([hash('sha256',$code),$now+900,$id]);return $code;
 }
 public function reset(string $inputEmail,string $code,string $password,int $now):void {
  self::password($password);$email=NooriService::email($inputEmail);if($email===null||!preg_match('/^[a-f0-9]{64}$/D',$code))throw new NooriError(403,'Recovery code is invalid or expired.');
  $this->db->beginTransaction();try{
   $s=$this->db->prepare($this->locked('SELECT id,reset_hash,reset_expires_at FROM noori_accounts WHERE email=?'));$s->execute([$email]);$row=$s->fetch();$table='noori_accounts';if(!$row){$table='noori_requests';$s=$this->db->prepare($this->locked('SELECT id,reset_hash,reset_expires_at FROM noori_requests WHERE email=? AND approved=0'));$s->execute([$email]);$row=$s->fetch();}
   if(!$row||!is_string($row['reset_hash'])||!hash_equals($row['reset_hash'],hash('sha256',$code))||(int)$row['reset_expires_at']<=$now)throw new NooriError(403,'Recovery code is invalid or expired.');
   $s=$this->db->prepare('UPDATE '.$table.' SET password_hash=?,auth_version=auth_version+1,reset_hash=NULL,reset_expires_at=NULL WHERE id=?');$s->execute([password_hash($password,PASSWORD_DEFAULT),$row['id']]);$this->db->commit();
  }catch(Throwable $e){if($this->db->inTransaction())$this->db->rollBack();throw $e;}
 }
 public function accounts():array {return $this->db->query('SELECT a.id,a.email,a.created_at,a.last_login_at,l.kind,l.revoked FROM noori_accounts a JOIN noori_licenses l ON l.id=a.license_id ORDER BY a.created_at DESC LIMIT 1000')->fetchAll();}
}

function nooriSession():void {
 global $config;
 ini_set('session.use_strict_mode','1');ini_set('session.use_only_cookies','1');session_name('noori_access');
 session_set_cookie_params(['secure'=>true,'httponly'=>true,'samesite'=>'Strict','path'=>(parse_url($config['public_url'],PHP_URL_PATH)?:'/')]);if(!@session_start()){http_response_code(503);exit('Secure sessions are temporarily unavailable.');}
 $_SESSION['csrf']??=bin2hex(random_bytes(32));
}
function nooriCsrf():void {if(!hash_equals($_SESSION['csrf']??'',(string)($_POST['csrf']??'')))throw new NooriError(403,'This form expired. Reload the page and try again.');}
function nooriAccount(NooriPortal $portal):?array {
 if(($_SESSION['role']??'')!=='friend'||!isset($_SESSION['friend_id']))return null;
 if(time()-(int)($_SESSION['friend_seen']??0)>1800||time()-(int)($_SESSION['friend_since']??0)>43200){unset($_SESSION['friend_id']);return null;}
 $user=$portal->account((string)$_SESSION['friend_id']);
 if(!$user||(int)$user['auth_version']!==(int)($_SESSION['friend_version']??0)){unset($_SESSION['friend_id']);return null;}
 $_SESSION['friend_seen']=time();return $user;
}
function nooriSignIn(array $user):void {unset($_SESSION['owner'],$_SESSION['last_seen'],$_SESSION['owner_since'],$_SESSION['pending_registration']);session_regenerate_id(true);$_SESSION['role']='friend';$_SESSION['friend_id']=$user['id'];$_SESSION['friend_version']=(int)$user['auth_version'];$_SESSION['friend_since']=$_SESSION['friend_seen']=time();$_SESSION['csrf']=bin2hex(random_bytes(32));}
function nooriOwner():bool {
 if(($_SESSION['role']??'')!=='owner'||($_SESSION['owner']??false)!==true)return false;
 if(time()-(int)($_SESSION['last_seen']??0)>1800||time()-(int)($_SESSION['owner_since']??0)>43200){unset($_SESSION['owner'],$_SESSION['role']);return false;}
 $_SESSION['last_seen']=time();return true;
}
function nooriOwnerId():string {global $config;return strtolower(trim((string)($config['owner_login']??'owner')));}
function nooriOwnerSignIn(string $password):void {
 global $privateDir;$owner=json_decode((string)file_get_contents($privateDir.'/owner.json'),true,8,JSON_THROW_ON_ERROR);
 if(!password_verify($password,$owner['password_hash']))throw new NooriError(403,'The login or password is incorrect.');
 unset($_SESSION['friend_id'],$_SESSION['friend_version'],$_SESSION['pending_registration']);session_regenerate_id(true);$_SESSION['role']='owner';$_SESSION['owner']=true;$_SESSION['last_seen']=$_SESSION['owner_since']=time();$_SESSION['csrf']=bin2hex(random_bytes(32));
}
function nooriRedirect(string $path):never {header('Location: '.$path,true,303);exit;}
function nooriFormToken():void {echo '<input type="hidden" name="csrf" value="'.esc($_SESSION['csrf']).'">';}
function nooriPageStart(string $title,?array $user=null):void {
 ?><!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><meta name="theme-color" content="#0a3437"><title><?=esc($title)?> — NOORI CHOIR</title><link rel="stylesheet" href="assets/site.css"><link rel="icon" href="assets/noori-logo.svg" type="image/svg+xml"></head><body><a class="skip" href="#main">Skip to content</a><header class="site-header wrap"><a class="brand" href="./"><img src="assets/noori-logo.svg" width="104" height="55" alt="NOORI — Shahid Ul Haq"><span>CHOIR</span></a><nav aria-label="Main navigation"><a href="./">Home</a><?php if($user||nooriOwner()):?><a href="account.php"><?=nooriOwner()?'Owner dashboard':'My account'?></a><form method="post" action="logout.php"><?php nooriFormToken();?><button class="text-button">Sign out</button></form><?php else:?><a href="login.php">Sign in</a><a class="owner-link" href="register.php">Create account ↗</a><?php endif;?></nav></header><main id="main">
 <?php
}
function nooriPageEnd():void {?></main><footer class="wrap site-footer"><p>NOORI CHOIR · Shared with friends.</p><a href="account.php">My dashboard →</a></footer></body></html><?php }
function nooriErrorText(Throwable $e):string {if($e instanceof NooriError){http_response_code($e->status);return $e->getMessage();}http_response_code(503);return 'The service is temporarily unavailable. Please contact NOORI.';}
function nooriRate(NooriService $service,string $action,int $maximum=10):void {if(!$service->limit('portal:'.$action.':'.($_SERVER['REMOTE_ADDR']??'unknown'),$maximum,time()))throw new NooriError(429,'Too many attempts. Please wait one minute.');}
function nooriDownloads():array {
 global $privateDir;
 $dir=realpath($privateDir.'/downloads');if($dir===false)throw new NooriError(503,'The owner is preparing the protected download. Please try again later.');
 $raw=@file_get_contents($dir.'/release-info.txt');if($raw===false)throw new NooriError(503,'The protected download is being prepared.');
 $info=json_decode($raw,true,16,JSON_THROW_ON_ERROR);
 if(($info['licensingEnforced']??false)!==true)throw new NooriError(503,'A protected installer is not available yet.');
 return ['directory'=>$dir,'release'=>$info];
}

function nooriNotificationSettings():array {
 global $config,$privateDir;$settings=['owner_notification_email'=>$config['owner_notification_email']??'shahidulhaq@live.com','notification_from'=>$config['notification_from']??''];
 $file=$privateDir.'/notification-settings.json';if(is_file($file)){$saved=json_decode((string)file_get_contents($file),true,8,JSON_THROW_ON_ERROR);$settings['notification_from']=(string)($saved['notification_from']??'');}return $settings;
}
function nooriSaveNotificationSender(string $sender):void {
 global $privateDir;if(!filter_var($sender,FILTER_VALIDATE_EMAIL)||preg_match('/[\r\n]/',$sender))throw new NooriError(400,'Enter your authorized hosting sender email.');
 $file=$privateDir.'/notification-settings.json';if(file_put_contents($file,json_encode(['notification_from'=>$sender],JSON_THROW_ON_ERROR),LOCK_EX)===false)throw new NooriError(503,'Cannot save notification settings.');@chmod($file,0600);
}
