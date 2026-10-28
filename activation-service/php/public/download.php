<?php
declare(strict_types=1);
require __DIR__.'/portal-bootstrap.php';
if(!$friend)nooriRedirect('login.php');
try{
 if(!in_array($_SERVER['REQUEST_METHOD'],['GET','HEAD'],true))throw new NooriError(405,'Use GET.');
 if(!NooriPortal::mayDownload($friend,time()))throw new NooriError(403,'Your download access is unavailable. Contact NOORI.');
 $allowed=['installer'=>['NOORI_CHOIR_Setup_x64.exe','application/octet-stream'],'vst3'=>['NOORI-CHOIR-Windows-x64.zip','application/zip'],'guide'=>['INSTALLATION.txt','text/plain; charset=utf-8'],'checksums'=>['SHA256SUMS.txt','text/plain; charset=utf-8']];
 $id=(string)($_GET['file']??'');if(!isset($allowed[$id]))throw new NooriError(404,'Download not found.');
 $downloads=nooriDownloads();[$name,$type]=$allowed[$id];$path=realpath($downloads['directory'].'/'.$name);
 if($path===false||dirname($path)!==$downloads['directory']||!is_file($path)||!is_readable($path))throw new NooriError(503,'The download is temporarily unavailable.');
 $input=fopen($path,'rb');if(!$input)throw new NooriError(503,'The download is temporarily unavailable.');$size=filesize($path);
 session_write_close();header('Content-Type: '.$type);header('Content-Disposition: attachment; filename="'.$name.'"');header('Content-Length: '.(string)$size);header('Cache-Control: private, no-store, max-age=0');header('Accept-Ranges: none');
 if($_SERVER['REQUEST_METHOD']!=='HEAD')while(!feof($input)&&!connection_aborted()){echo fread($input,1048576);flush();}fclose($input);exit;
}catch(Throwable $e){$message=nooriErrorText($e);header('Content-Type: text/plain; charset=utf-8');echo $message;}
