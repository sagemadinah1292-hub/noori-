<?php
declare(strict_types=1);

final class NooriPortal {
 public function __construct(private PDO $db) {
  $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);
  $db->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE,PDO::FETCH_ASSOC);
  if($db->getAttribute(PDO::ATTR_DRIVER_NAME)==='mysql'){
   $db->setAttribute(PDO::ATTR_EMULATE_PREPARES,false);
   $db->exec("CREATE TABLE IF NOT EXISTS noori_accounts (id CHAR(36) PRIMARY KEY,email VARCHAR(254) COLLATE utf8mb4_bin NOT NULL UNIQUE,password_hash VARCHAR(255) NOT NULL,license_id CHAR(36) NOT NULL UNIQUE,created_at BIGINT NOT NULL,last_login_at BIGINT NULL,auth_version INT NOT NULL DEFAULT 1,reset_hash CHAR(64) NULL,reset_expires_at BIGINT NULL) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
  }else{
   $db->exec('CREATE TABLE IF NOT EXISTS noori_accounts (id TEXT PRIMARY KEY,email TEXT NOT NULL UNIQUE,password_hash TEXT NOT NULL,license_id TEXT NOT NULL UNIQUE,created_at INTEGER NOT NULL,last_login_at INTEGER NULL,auth_version INTEGER NOT NULL DEFAULT 1,reset_hash TEXT NULL,reset_expires_at INTEGER NULL)');
  }
 }
 private function locked(string $sql):string {return $sql.($this->db->getAttribute(PDO::ATTR_DRIVER_NAME)==='mysql'?' FOR UPDATE':'');}
 private static function password(string $value):void {if(strlen($value)<12||strlen($value)>72)throw new NooriError(400,'Choose a password between 12 and 72 bytes.');}
 public function register(string $inputEmail,string $password,string $serial,int $now):array {
  $email=NooriService::email($inputEmail);self::password($password);$serial=strtoupper(trim($serial));
  if($email===null||!preg_match('/^NOORI-[TF]-[A-F0-9]{32}$/D',$serial))throw new NooriError(400,'Enter your email and a valid NOORI invitation key.');
  $passwordHash=password_hash($password,PASSWORD_DEFAULT);$this->db->beginTransaction();
  try{
   $s=$this->db->prepare($this->locked('SELECT * FROM noori_licenses WHERE key_hash=?'));$s->execute([hash('sha256',$serial)]);$license=$s->fetch();
   if(!$license||($license['email']!==null&&$license['email']!==$email)||(int)$license['revoked']!==0||($license['kind']==='trial'&&$license['expires_at']!==null&&(int)$license['expires_at']<=$now))throw new NooriError(403,'This invitation is unavailable for this email. Contact NOORI.');
   $id=NooriService::uuid();$s=$this->db->prepare('INSERT INTO noori_accounts(id,email,password_hash,license_id,created_at) VALUES(?,?,?,?,?)');$s->execute([$id,$email,$passwordHash,$license['id'],$now]);
   // Reserve the email only. A browser never starts a trial or binds a Windows computer.
   $s=$this->db->prepare('UPDATE noori_licenses SET email=? WHERE id=?');$s->execute([$email,$license['id']]);
   $this->db->commit();return $this->account($id)??throw new RuntimeException('Account unavailable.');
  }catch(Throwable $e){if($this->db->inTransaction())$this->db->rollBack();if($e instanceof PDOException&&in_array((string)$e->getCode(),['23000','19'],true))throw new NooriError(409,'An account or trial already exists. Sign in or contact NOORI.');throw $e;}
 }
 public function login(string $inputEmail,string $password,int $now):array {
  $email=NooriService::email($inputEmail);$s=$this->db->prepare('SELECT id,password_hash FROM noori_accounts WHERE email=?');$s->execute([$email??'']);$row=$s->fetch();
  // Always perform a password check, including for unknown accounts.
  $hash=$row['password_hash']??'$2y$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2uheWG/igi.';
  if(!password_verify($password,$hash)||!$row)throw new NooriError(403,'The email or password is incorrect.');
  $s=$this->db->prepare('UPDATE noori_accounts SET last_login_at=? WHERE id=?');$s->execute([$now,$row['id']]);return $this->account($row['id'])??throw new NooriError(403,'Account unavailable.');
 }
 public function account(string $id):?array {
  $s=$this->db->prepare('SELECT a.id,a.email,a.license_id,a.created_at,a.last_login_at,a.auth_version,l.kind,l.key_hint,l.revoked,l.device_id,l.activated_at,l.expires_at FROM noori_accounts a JOIN noori_licenses l ON l.id=a.license_id WHERE a.id=?');$s->execute([$id]);$row=$s->fetch();return $row?:null;
 }
 public static function mayDownload(array $account,int $now):bool {return (int)$account['revoked']===0&&($account['kind']!=='trial'||$account['expires_at']===null||(int)$account['expires_at']>$now);}
 public function changePassword(string $id,string $old,string $new):int {
  self::password($new);$this->db->beginTransaction();try{
   $s=$this->db->prepare($this->locked('SELECT password_hash,auth_version FROM noori_accounts WHERE id=?'));$s->execute([$id]);$row=$s->fetch();
   if(!$row||!password_verify($old,$row['password_hash']))throw new NooriError(403,'Current password is incorrect.');
   $version=(int)$row['auth_version']+1;$s=$this->db->prepare('UPDATE noori_accounts SET password_hash=?,auth_version=?,reset_hash=NULL,reset_expires_at=NULL WHERE id=?');$s->execute([password_hash($new,PASSWORD_DEFAULT),$version,$id]);$this->db->commit();return $version;
  }catch(Throwable $e){if($this->db->inTransaction())$this->db->rollBack();throw $e;}
 }
 public function recovery(string $id,int $now):string {
  if(!$this->account($id))throw new NooriError(404,'Account not found.');$code=bin2hex(random_bytes(32));$s=$this->db->prepare('UPDATE noori_accounts SET reset_hash=?,reset_expires_at=? WHERE id=?');$s->execute([hash('sha256',$code),$now+900,$id]);return $code;
 }
 public function reset(string $inputEmail,string $code,string $password,int $now):void {
  self::password($password);$email=NooriService::email($inputEmail);if($email===null||!preg_match('/^[a-f0-9]{64}$/D',$code))throw new NooriError(403,'Recovery code is invalid or expired.');
  $this->db->beginTransaction();try{
   $s=$this->db->prepare($this->locked('SELECT id,reset_hash,reset_expires_at FROM noori_accounts WHERE email=?'));$s->execute([$email]);$row=$s->fetch();
   if(!$row||!is_string($row['reset_hash'])||!hash_equals($row['reset_hash'],hash('sha256',$code))||(int)$row['reset_expires_at']<=$now)throw new NooriError(403,'Recovery code is invalid or expired.');
   $s=$this->db->prepare('UPDATE noori_accounts SET password_hash=?,auth_version=auth_version+1,reset_hash=NULL,reset_expires_at=NULL WHERE id=?');$s->execute([password_hash($password,PASSWORD_DEFAULT),$row['id']]);$this->db->commit();
  }catch(Throwable $e){if($this->db->inTransaction())$this->db->rollBack();throw $e;}
 }
 public function accounts():array {return $this->db->query('SELECT a.id,a.email,a.created_at,a.last_login_at,l.kind,l.revoked FROM noori_accounts a JOIN noori_licenses l ON l.id=a.license_id ORDER BY a.created_at DESC LIMIT 1000')->fetchAll();}
}

function nooriSession(string $name):void {
 global $config;
 ini_set('session.use_strict_mode','1');ini_set('session.use_only_cookies','1');session_name($name);
 session_set_cookie_params(['secure'=>true,'httponly'=>true,'samesite'=>'Strict','path'=>(parse_url($config['public_url'],PHP_URL_PATH)?:'/')]);session_start();
 $_SESSION['csrf']??=bin2hex(random_bytes(32));
}
function nooriCsrf():void {if(!hash_equals($_SESSION['csrf']??'',(string)($_POST['csrf']??'')))throw new NooriError(403,'This form expired. Reload the page and try again.');}
function nooriAccount(NooriPortal $portal):?array {
 if(!isset($_SESSION['friend_id']))return null;
 if(time()-(int)($_SESSION['friend_seen']??0)>1800||time()-(int)($_SESSION['friend_since']??0)>43200){unset($_SESSION['friend_id']);return null;}
 $user=$portal->account((string)$_SESSION['friend_id']);
 if(!$user||(int)$user['auth_version']!==(int)($_SESSION['friend_version']??0)){unset($_SESSION['friend_id']);return null;}
 $_SESSION['friend_seen']=time();return $user;
}
function nooriSignIn(array $user):void {session_regenerate_id(true);$_SESSION['friend_id']=$user['id'];$_SESSION['friend_version']=(int)$user['auth_version'];$_SESSION['friend_since']=$_SESSION['friend_seen']=time();$_SESSION['csrf']=bin2hex(random_bytes(32));}
function nooriRedirect(string $path):never {header('Location: '.$path,true,303);exit;}
function nooriFormToken():void {echo '<input type="hidden" name="csrf" value="'.esc($_SESSION['csrf']).'">';}
function nooriPageStart(string $title,?array $user=null):void {
 ?><!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><meta name="theme-color" content="#0a3437"><title><?=esc($title)?> — NOORI CHOIR</title><link rel="stylesheet" href="assets/site.css"><link rel="icon" href="assets/noori-logo.svg" type="image/svg+xml"></head><body><a class="skip" href="#main">Skip to content</a><header class="site-header wrap"><a class="brand" href="./"><img src="assets/noori-logo.svg" width="104" height="55" alt="NOORI — Shahid Ul Haq"><span>CHOIR</span></a><nav aria-label="Main navigation"><a href="./">Home</a><?php if($user):?><a href="dashboard.php">My account</a><form method="post" action="logout.php"><?php nooriFormToken();?><button class="text-button">Sign out</button></form><?php else:?><a href="login.php">Sign in</a><a class="owner-link" href="register.php">Create account ↗</a><?php endif;?></nav></header><main id="main">
 <?php
}
function nooriPageEnd():void {?></main><footer class="wrap site-footer"><p>NOORI CHOIR · Shared with friends.</p><a href="admin.php">Owner sign in ↗</a></footer></body></html><?php }
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
