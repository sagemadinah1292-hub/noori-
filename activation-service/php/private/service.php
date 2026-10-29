<?php
declare(strict_types=1);
final class NooriError extends RuntimeException {
 public function __construct(public readonly int $status, string $message) { parent::__construct($message); }
}
final class NooriService {
 public const TRIAL_SECONDS=259200;
 public const LEASE_SECONDS=900;
 public function __construct(private PDO $db, private string $privateKey, private string $policy='online') {
  $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);
  $db->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE,PDO::FETCH_ASSOC);
  if($db->getAttribute(PDO::ATTR_DRIVER_NAME)==='mysql')$db->setAttribute(PDO::ATTR_EMULATE_PREPARES,false);
 }
 public static function email(mixed $input):?string {
  if(!is_string($input))return null;$value=strtolower(trim($input));
  return strlen($value)<=254&&filter_var($value,FILTER_VALIDATE_EMAIL)!==false?$value:null;
 }
 public static function uuid():string {
  $b=random_bytes(16);$b[6]=chr((ord($b[6])&15)|64);$b[8]=chr((ord($b[8])&63)|128);$h=bin2hex($b);
  return substr($h,0,8).'-'.substr($h,8,4).'-'.substr($h,12,4).'-'.substr($h,16,4).'-'.substr($h,20);
 }
 public function limit(string $actor,int $maximum,int $now):bool {
  $bucket=hash_hmac('sha256',$actor,$this->privateKey);
  if($this->db->getAttribute(PDO::ATTR_DRIVER_NAME)==='mysql'){
   $sql='INSERT INTO noori_rate_limits(bucket,requests,reset_at) VALUES(?,1,?) ON DUPLICATE KEY UPDATE requests=IF(reset_at<=?,1,requests+1),reset_at=IF(reset_at<=?,VALUES(reset_at),reset_at)';
  }else{
   $sql='INSERT INTO noori_rate_limits(bucket,requests,reset_at) VALUES(?,1,?) ON CONFLICT(bucket) DO UPDATE SET requests=CASE WHEN reset_at<=? THEN 1 ELSE requests+1 END,reset_at=CASE WHEN reset_at<=? THEN excluded.reset_at ELSE reset_at END';
  }
  $s=$this->db->prepare($sql);$s->execute([$bucket,$now+60,$now,$now]);
  $s=$this->db->prepare('SELECT requests FROM noori_rate_limits WHERE bucket=?');$s->execute([$bucket]);return (int)$s->fetchColumn()<=$maximum;
 }
 public function automaticSerial(string $id,string $email):string {return 'NOORI-T-'.strtoupper(substr(hash_hmac('sha256','noori-auto-trial-v1:'.$id.':'.$email,$this->privateKey),0,32));}
 public function issue(int $count,string $kind,?string $email=null,?int $now=null):array {
  if($count<1||$count>100||!in_array($kind,['trial','full'],true))throw new NooriError(400,'Choose 1–100 trial or full-access keys.');
  if($email!==null){$email=self::email($email);if($email===null||$count!==1)throw new NooriError(400,'An assigned email needs exactly one key.');}
  $now??=time();$keys=[];$this->db->beginTransaction();
  try{
   $insert=$this->db->prepare('INSERT INTO noori_licenses(id,email,key_hash,key_hint,kind,created_at) VALUES(?,?,?,?,?,?)');
   for($i=0;$i<$count;$i++){
    $serial='NOORI-'.($kind==='trial'?'T':'F').'-'.strtoupper(bin2hex(random_bytes(16)));
    $id=self::uuid();$insert->execute([$id,$email,hash('sha256',$serial),substr($serial,-8),$kind,$now]);
    $keys[]=['id'=>$id,'serial'=>$serial,'email'=>$email??'','kind'=>$kind];
   }
   $this->db->commit();return $keys;
  }catch(Throwable $e){if($this->db->inTransaction())$this->db->rollBack();if($e instanceof PDOException&&$e->getCode()==='23000')throw new NooriError(409,'A trial already exists for this email.');throw $e;}
 }
 public function activate(array $input,bool $activate,int $now):array {
  $email=self::email($input['email']??null);$serial=is_string($input['serial']??null)?strtoupper(trim($input['serial'])):'';
  $device=$input['deviceId']??null;$nonce=$input['nonce']??null;
  if($email===null||!preg_match('/^NOORI-[TF]-[A-F0-9]{32}$/D',$serial)
    ||!is_string($device)||!preg_match('/^[a-f0-9]{64}$/Di',$device)
    ||!is_string($nonce)||!preg_match('/^[a-f0-9-]{32,36}$/Di',$nonce))throw new NooriError(400,'Enter the email and key supplied by NOORI.');
  $device=strtolower($device);$hash=hash('sha256',$serial);
  if(!$this->limit('key:'.$hash,10,$now))throw new NooriError(429,'Too many key attempts. Wait one minute.');
  $this->db->beginTransaction();
  try{
   $sql='SELECT * FROM noori_licenses WHERE key_hash=?';if($this->db->getAttribute(PDO::ATTR_DRIVER_NAME)==='mysql')$sql.=' FOR UPDATE';
   $s=$this->db->prepare($sql);$s->execute([$hash]);$row=$s->fetch();
   if(!$row||($row['email']!==null&&$row['email']!==$email))throw new NooriError(403,'The email and serial number do not match.');
   if((int)$row['revoked']!==0)throw new NooriError(403,'This key has been disabled. Contact NOORI.');
   if($row['kind']==='trial'&&$row['expires_at']!==null&&(int)$row['expires_at']<=$now)throw new NooriError(403,'Your three-day trial has ended. Ask NOORI for full access.');
   if($row['device_id']!==null&&$row['device_id']!==$device)throw new NooriError(403,'This key is active on another computer. Ask NOORI to reset it.');
   if(!$activate&&$row['device_id']===null)throw new NooriError(403,'Activate this key first.');
   if($activate){
    $row['email']=$email;$row['device_id']=$device;$row['activated_at']=$row['activated_at']??$now;
    $row['expires_at']=$row['kind']==='trial'?($row['expires_at']??($now+self::TRIAL_SECONDS)):null;
    $s=$this->db->prepare('UPDATE noori_licenses SET email=?,device_id=?,activated_at=?,expires_at=? WHERE id=?');
    $s->execute([$row['email'],$row['device_id'],$row['activated_at'],$row['expires_at'],$row['id']]);
   }
   $online=$row['kind']==='trial'||$this->policy!=='activation';
   $until=$online?min($now+self::LEASE_SECONDS,$row['expires_at']===null?PHP_INT_MAX:(int)$row['expires_at']):0;
   $payload=json_encode(['version'=>1,'product'=>'noori-choir','licenseId'=>$row['id'],'email'=>$row['email'],
    'deviceId'=>$row['device_id'],'kind'=>$row['kind'],'issuedAt'=>$now,'trialExpiresAt'=>(int)($row['expires_at']??0),
    'validUntil'=>$until,'onlineRequired'=>$online,'nonce'=>$nonce],JSON_UNESCAPED_SLASHES|JSON_THROW_ON_ERROR);
   if(!openssl_sign($payload,$signature,$this->privateKey,OPENSSL_ALGO_SHA256))throw new RuntimeException('Signing failed.');
   $this->db->commit();return ['algorithm'=>'RS256','payload'=>base64_encode($payload),'signature'=>base64_encode($signature)];
  }catch(Throwable $e){if($this->db->inTransaction())$this->db->rollBack();if($e instanceof PDOException&&$e->getCode()==='23000')throw new NooriError(409,'A trial has already been assigned to this email.');throw $e;}
 }
 public function all():array {return $this->db->query('SELECT id,email,key_hint,kind,created_at,activated_at,expires_at,device_id,revoked FROM noori_licenses ORDER BY created_at DESC,id LIMIT 1000')->fetchAll();}
 public function change(string $id,string $action):void {
  if(!preg_match('/^[a-f0-9-]{36}$/Di',$id))throw new NooriError(400,'Invalid license.');
  $sql=match($action){'revoke'=>'UPDATE noori_licenses SET revoked=1 WHERE id=?',
   'restore'=>'UPDATE noori_licenses SET revoked=0 WHERE id=?',
   'reset'=>'UPDATE noori_licenses SET device_id=NULL WHERE id=?',
   'full'=>'UPDATE noori_licenses SET kind=\'full\',expires_at=NULL WHERE id=?',
   default=>throw new NooriError(400,'Unknown action.')};
  $s=$this->db->prepare($sql);$s->execute([$id]);
 }
}
