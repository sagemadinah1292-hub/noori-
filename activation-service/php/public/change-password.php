<?php
declare(strict_types=1);
require __DIR__.'/portal-bootstrap.php';if(!$friend)nooriRedirect('login.php');$message='';
if($_SERVER['REQUEST_METHOD']==='POST')try{nooriCsrf();nooriRate($service,'change-password',6);if(!hash_equals((string)($_POST['password']??''),(string)($_POST['confirm']??'')))throw new NooriError(400,'The passwords do not match.');$_SESSION['friend_version']=$portal->changePassword($friend['id'],(string)($_POST['current']??''),(string)($_POST['password']??''));session_regenerate_id(true);$_SESSION['csrf']=bin2hex(random_bytes(32));$message='Password updated. Other sessions have been signed out.';}catch(Throwable $e){$message=nooriErrorText($e);}
nooriPageStart('Change password',$friend);
?>
<section class="account-wrap wrap"><div class="account-intro"><p class="eyebrow">YOUR ACCOUNT</p><h1>A new<br><em>password.</em></h1><p><a href="dashboard.php">← Back to your account</a></p></div><div class="account-card"><h2>Change password</h2><p class="form-message" role="status"><?=esc($message)?></p><form method="post"><?php nooriFormToken();?><label>Current password<input type="password" name="current" required autocomplete="current-password"></label><label>New password<input type="password" name="password" required minlength="12" maxlength="72" autocomplete="new-password"></label><label>Confirm new password<input type="password" name="confirm" required minlength="12" maxlength="72" autocomplete="new-password"></label><button class="button">Save password →</button></form></div></section>
<?php nooriPageEnd();
