<?php
declare(strict_types=1);
require __DIR__.'/bootstrap.php';header('Content-Type: application/json; charset=utf-8');
try{
 $base=rtrim(parse_url($config['public_url'],PHP_URL_PATH)?:'','/');$path=parse_url($_SERVER['REQUEST_URI'],PHP_URL_PATH);
 if($path===$base.'/health'){
  $ready=is_file($privateDir.'/installed.marker')&&is_file($privateDir.'/signing-private.pem');
  http_response_code($ready?200:503);echo json_encode(['service'=>'noori-choir','protocol'=>1,'activationReady'=>$ready]);exit;
 }
 if(!in_array($path,[$base.'/v1/activate',$base.'/v1/check'],true))throw new NooriError(404,'Not found.');
 if($_SERVER['REQUEST_METHOD']!=='POST')throw new NooriError(405,'Use POST.');
 if(!str_starts_with(strtolower($_SERVER['CONTENT_TYPE']??''),'application/json'))throw new NooriError(415,'JSON request required.');
 $service=nooriService();$now=time();if(!$service->limit('ip:'.($_SERVER['REMOTE_ADDR']??'unknown'),60,$now))throw new NooriError(429,'Too many attempts. Wait one minute.');
 $input=fopen('php://input','rb');$raw=stream_get_contents($input,4097);fclose($input);if(strlen($raw)>4096)throw new NooriError(413,'Request too large.');
 $body=json_decode($raw,true,16,JSON_THROW_ON_ERROR);if(!is_array($body))throw new NooriError(400,'JSON object required.');
 echo json_encode($service->activate($body,$path===$base.'/v1/activate',$now),JSON_THROW_ON_ERROR);
}catch(NooriError $e){http_response_code($e->status);echo json_encode(['error'=>$e->getMessage()]);}
catch(JsonException){http_response_code(400);echo '{"error":"Invalid JSON."}';}
catch(Throwable){http_response_code(503);echo '{"error":"Activation is temporarily unavailable. Contact NOORI."}';}
