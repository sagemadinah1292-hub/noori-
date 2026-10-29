<?php
declare(strict_types=1);
require __DIR__.'/portal-bootstrap.php';
try{if($_SERVER['REQUEST_METHOD']!=='POST')throw new NooriError(405,'Use POST.');nooriCsrf();$_SESSION=[];session_destroy();nooriRedirect('login.php');}catch(Throwable $e){echo esc(nooriErrorText($e));}
