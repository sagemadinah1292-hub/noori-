<?php
declare(strict_types=1);
require_once __DIR__.'/bootstrap.php';
if((int)($_SERVER['CONTENT_LENGTH']??0)>16384){http_response_code(413);exit('Request too large.');}
if(!is_file($privateDir.'/portal.php')){http_response_code(503);exit('Upload the private portal upgrade first.');}
require_once $privateDir.'/portal.php';
try{$portal=new NooriPortal(nooriDb());$service=nooriService();}catch(Throwable){http_response_code(503);exit('The account service is temporarily unavailable. Contact NOORI.');}
nooriSession('noori_friend');
$friend=nooriAccount($portal);
