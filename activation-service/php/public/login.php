<?php
declare(strict_types=1);
require __DIR__.'/portal-bootstrap.php';
if($friend)nooriRedirect('dashboard.php');$error='';
if($_SERVER['REQUEST_METHOD']==='POST')try{
 nooriCsrf();nooriRate($service,'login',10);
 $user=$portal->login((string)($_POST['email']??''),(string)($_POST['password']??''),time());nooriSignIn($user);nooriRedirect('dashboard.php');
}catch(Throwable $e){$error=nooriErrorText($e);}
nooriPageStart('Sign in');
?>
<section class="account-wrap wrap"><div class="account-intro"><p class="eyebrow">YOUR NOORI ACCOUNT</p><h1>Welcome<br><em>back.</em></h1><p>Sign in to download NOORI CHOIR and view your invitation access.</p><p>New here? <a href="register.php">Create your account with an invitation key.</a></p></div><div class="account-card"><h2>Sign in</h2><p class="form-message" role="status"><?=esc($error)?></p><form method="post"><?php nooriFormToken();?><label>Email<input type="email" name="email" required maxlength="254" autocomplete="username" value="<?=esc((string)($_POST['email']??''))?>"></label><label>Password<input type="password" name="password" required maxlength="72" autocomplete="current-password"></label><button class="button">Sign in →</button></form><a class="small-link" href="reset-password.php">Forgot your password?</a></div></section>
<?php nooriPageEnd();
