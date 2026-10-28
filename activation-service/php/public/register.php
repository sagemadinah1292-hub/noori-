<?php
declare(strict_types=1);
require __DIR__.'/portal-bootstrap.php';if(nooriOwner())nooriRedirect('admin.php');if($friend)nooriRedirect('dashboard.php');$error='';
if($_SERVER['REQUEST_METHOD']==='POST')try{
 nooriCsrf();nooriRate($service,'register-details',6);
 $email=NooriService::email((string)($_POST['email']??''));if($email===null)throw new NooriError(400,'Enter a valid email.');$proof=['email'=>$email,'expires'=>time()+600];
 if($proof['email']===nooriOwnerId())throw new NooriError(403,'This login is reserved. Use your personal email.');
 $_SESSION['pending_registration']=$proof;nooriRedirect('create-password.php');
}catch(Throwable $e){$error=nooriErrorText($e);}
nooriPageStart('Registration');
?>
<section class="account-wrap wrap"><div class="account-intro"><p class="eyebrow">STEP 1 OF 2 · YOUR DETAILS</p><h1>Your choir.<br><em>Your account.</em></h1><p>Enter your personal email. Next, create a password. Downloads stay locked until the owner gives you a final access key.</p><p>Your trial starts when you first activate the Windows plugin. Registration on this website does not start it.</p><p>Already have an account? <a href="login.php">Sign in.</a></p></div><div class="account-card"><h2>Registration details</h2><ol class="form-steps" aria-label="Registration progress"><li aria-current="step">1. Your details</li><li>2. Create password</li></ol><p class="form-message" role="status"><?=esc($error)?></p><form method="post"><?php nooriFormToken();?><label>Email<input type="email" name="email" required maxlength="254" autocomplete="email" value="<?=esc((string)($_POST['email']??''))?>"></label><button class="button">Continue to password →</button></form></div></section>
<?php nooriPageEnd();
