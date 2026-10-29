<?php
declare(strict_types=1);
require __DIR__.'/portal-bootstrap.php';if(nooriOwner())nooriRedirect('admin.php');if($friend)nooriRedirect('dashboard.php');$proof=$_SESSION['pending_registration']??null;
if(!is_array($proof)||(int)($proof['expires']??0)<=time()){unset($_SESSION['pending_registration']);nooriRedirect('register.php');}$error='';
if($_SERVER['REQUEST_METHOD']==='POST')try{
 nooriCsrf();nooriRate($service,'register-password',6);
 if(!hash_equals((string)($_POST['password']??''),(string)($_POST['confirm']??'')))throw new NooriError(400,'The passwords do not match.');
 $user=$portal->pendingRegistration($proof['email'],(string)($_POST['password']??''),time());$portal->notifyOwner($user,nooriNotificationSettings());$sent=$portal->sendVerification($user,nooriNotificationSettings(),time());$_SESSION['verification_sent']=$sent;nooriSignIn($user);nooriRedirect('dashboard.php');
}catch(Throwable $e){$error=nooriErrorText($e);}
nooriPageStart('Create password');
?>
<section class="account-wrap wrap"><div class="account-intro"><p class="eyebrow">STEP 2 OF 2 · SECURE YOUR ACCOUNT</p><h1>Make it<br><em>yours.</em></h1><p>Create your password, then confirm your email to receive an automatic three-day trial.</p><p><a href="register.php">← Change registration details</a></p></div><div class="account-card"><h2>Create your password</h2><ol class="form-steps" aria-label="Registration progress"><li>1. Your details</li><li aria-current="step">2. Create password</li></ol><p class="field-help">Account: <?=esc($proof['email'])?></p><p class="form-message" role="status"><?=esc($error)?></p><form method="post"><?php nooriFormToken();?><label>Email<input type="email" name="email" readonly autocomplete="username" value="<?=esc($proof['email'])?>"></label><label>Create password<input type="password" name="password" required minlength="12" maxlength="72" autocomplete="new-password"></label><label>Confirm password<input type="password" name="confirm" required minlength="12" maxlength="72" autocomplete="new-password"></label><p class="field-help">Use at least 12 characters. Confirm your email to receive your trial serial automatically.</p><button class="button">Finish registration →</button></form></div></section>
<?php nooriPageEnd();
