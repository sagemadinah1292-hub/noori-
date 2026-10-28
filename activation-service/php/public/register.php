<?php
declare(strict_types=1);
require __DIR__.'/portal-bootstrap.php';if($friend)nooriRedirect('dashboard.php');$error='';
if($_SERVER['REQUEST_METHOD']==='POST')try{
 nooriCsrf();nooriRate($service,'register',6);
 if(!hash_equals((string)($_POST['password']??''),(string)($_POST['confirm']??'')))throw new NooriError(400,'The passwords do not match.');
 $user=$portal->register((string)($_POST['email']??''),(string)($_POST['password']??''),(string)($_POST['serial']??''),time());nooriSignIn($user);nooriRedirect('dashboard.php');
}catch(Throwable $e){$error=nooriErrorText($e);}
nooriPageStart('Create account');
?>
<section class="account-wrap wrap"><div class="account-intro"><p class="eyebrow">BY PERSONAL INVITATION</p><h1>Your choir.<br><em>Your account.</em></h1><p>Use the email and invitation key shared with you by NOORI. Create your own password to access the downloads.</p><p>Your three-day trial starts when you first activate the plugin on Windows. Creating this account does not start it.</p><p>Already registered? <a href="login.php">Sign in.</a></p></div><div class="account-card"><h2>Create your account</h2><p class="form-message" role="status"><?=esc($error)?></p><form method="post"><?php nooriFormToken();?><label>Email<input type="email" name="email" required maxlength="254" autocomplete="username" value="<?=esc((string)($_POST['email']??''))?>"></label><label>Invitation key<input type="password" name="serial" required maxlength="64" autocomplete="off" placeholder="NOORI-T-… or NOORI-F-…"></label><label>Create password<input type="password" name="password" required minlength="12" maxlength="72" autocomplete="new-password"></label><label>Confirm password<input type="password" name="confirm" required minlength="12" maxlength="72" autocomplete="new-password"></label><p class="field-help">Use 12 or more characters. Keep your invitation key; you will also need it in the plugin.</p><button class="button">Create account →</button></form></div></section>
<?php nooriPageEnd();
