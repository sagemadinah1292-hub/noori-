<?php
declare(strict_types=1);
require __DIR__.'/portal-bootstrap.php';if(nooriOwner())nooriRedirect('admin.php');if($friend)nooriRedirect('dashboard.php');$error='';
if($_SERVER['REQUEST_METHOD']==='POST')try{
 nooriCsrf();nooriRate($service,'register-details',6);
 $proof=$portal->invitation((string)($_POST['email']??''),(string)($_POST['serial']??''),time());
 if($proof['email']===nooriOwnerId())throw new NooriError(403,'This login is reserved. Use your personal email.');
 $_SESSION['pending_registration']=$proof;nooriRedirect('create-password.php');
}catch(Throwable $e){$error=nooriErrorText($e);}
nooriPageStart('Registration');
?>
<section class="account-wrap wrap"><div class="account-intro"><p class="eyebrow">STEP 1 OF 2 · YOUR DETAILS</p><h1>Your choir.<br><em>Your invitation.</em></h1><p>Enter the email and invitation key shared with you by NOORI. Next, you will create your personal password.</p><p>Your trial starts when you first activate the Windows plugin. Registration on this website does not start it.</p><p>Already have an account? <a href="login.php">Sign in.</a></p></div><div class="account-card"><h2>Registration details</h2><ol class="form-steps" aria-label="Registration progress"><li aria-current="step">1. Your details</li><li>2. Create password</li></ol><p class="form-message" role="status"><?=esc($error)?></p><form method="post"><?php nooriFormToken();?><label>Email<input type="email" name="email" required maxlength="254" autocomplete="email" value="<?=esc((string)($_POST['email']??''))?>"></label><label>Invitation key<input type="password" name="serial" required maxlength="64" autocomplete="off" placeholder="NOORI-T-… or NOORI-F-…"></label><p class="field-help">Use the details from your private NOORI invitation.</p><button class="button">Continue to password →</button></form></div></section>
<?php nooriPageEnd();
