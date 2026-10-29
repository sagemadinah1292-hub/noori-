<?php
declare(strict_types=1);
// EDIT THIS PATH ONLY. Put noori-private OUTSIDE ALL public_html directories.
$privateDir='/CHANGE_TO_YOUR_HOME_DIRECTORY/noori-private';
header('Cache-Control: no-store');header('X-Content-Type-Options: nosniff');
header('Referrer-Policy: no-referrer');header('X-Frame-Options: DENY');
header("Content-Security-Policy: default-src 'self'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'; form-action 'self'; base-uri 'none'");
if(($_SERVER['HTTPS']??'')!=='on'&&($_SERVER['HTTPS']??'')!=='1'){
 http_response_code(403);header('Content-Type: application/json');echo '{"error":"HTTPS is required."}';exit;
}
header('Strict-Transport-Security: max-age=31536000');
if(!is_file($privateDir.'/config.php')||!is_file($privateDir.'/service.php')){
 http_response_code(503);header('Content-Type: application/json');echo '{"error":"Activation service is not configured."}';exit;
}
require_once $privateDir.'/service.php';
$config=require $privateDir.'/config.php';
function nooriDb():PDO {global $config;return new PDO($config['dsn'],$config['db_user'],$config['db_password'],[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION]);}
function nooriService():NooriService {global $config,$privateDir;$key=@file_get_contents($privateDir.'/signing-private.pem');if($key===false)throw new RuntimeException('Service not configured.');return new NooriService(nooriDb(),$key,$config['access_policy']??'online');}
function esc(string $value):string{return htmlspecialchars($value,ENT_QUOTES|ENT_SUBSTITUTE,'UTF-8');}
