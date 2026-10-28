<?php
declare(strict_types=1);

function nooriUpgradePaths(string $app,string $private):array {
 $app=realpath($app);$private=realpath($private);if($app===false||$private===false||basename($app)!=='noori-activation')throw new NooriError(503,'The application must be in the noori-activation folder.');
 $root=realpath(dirname($app));if($root===false||basename($root)!=='public_html'||$private===$root||str_starts_with($private,$root.DIRECTORY_SEPARATOR))throw new NooriError(503,'The private folder must be outside public_html.');
 return [$app,$root,$private];
}
function nooriUpgradePlan(string $app,string $private):array {
 [$app,$root,$private]=nooriUpgradePaths($app,$private);$items=[];$notes=[];
 foreach([$root,$app] as $dir){
  foreach(new DirectoryIterator($dir) as $entry){
   if($entry->isDot()||$entry->isLink())continue;$name=$entry->getFilename();$path=$entry->getPathname();$relative=substr($path,strlen($root)+1);
   if($entry->isFile()&&preg_match('/^NOORI[^\/]*\.(zip|exe)$/iD',$name)){$items[$relative]=$path;continue;}
   if($entry->isDir()&&preg_match('/^downloads(?:\.[0-9]+)?$/D',$name)){
    // Move only known NOORI payloads; unrelated files inside a downloads folder survive.
    $known=[];$other=false;$identified=false;
    foreach(new DirectoryIterator($path) as $payload){
     if($payload->isDot())continue;if(!$payload->isFile()||$payload->isLink()){$other=true;continue;}$file=$payload->getFilename();
     $binary=(bool)preg_match('/^NOORI[^\/]*\.(zip|exe)$/iD',$file);
     if($binary||in_array($file,['BUILD-INFO.txt','INSTALLER-TEST-RESULTS.txt','release-info.txt','INSTALLATION.txt','SHA256SUMS.txt'],true)){$known[$relative.'/'.$file]=$payload->getPathname();$identified=$identified||$binary;}else $other=true;
    }
    if($identified&&!$other)$items[$relative]=$path;else foreach($known as $key=>$value)if(preg_match('/^NOORI/i',basename($value)))$items[$key]=$value;
   }
   if($entry->isDir()&&(($dir===$root&&$name==='assets')||($dir===$app&&preg_match('/^assets\.[0-9]+$/D',$name)))){
    $entries=array_values(array_diff(scandir($path),['.','..']));$safe=['noori-logo.svg','site.css','plugin-preview.png'];
    if(in_array('noori-logo.svg',$entries,true)&&!array_diff($entries,$safe)&&hash_file('sha256',$path.'/noori-logo.svg')===hash_file('sha256',$app.'/assets/noori-logo.svg'))$items[$relative]=$path;
   }
  }
 }
 foreach(['admin.php'=>"session_name('noori_owner')",'bootstrap.php'=>'function nooriService()','index.php'=>'nooriService()','index.html'=>'NOORI CHOIR'] as $file=>$marker){
  $path=$root.'/'.$file;if(is_file($path)&&!is_link($path)){$content=file_get_contents($path);if(is_string($content)&&str_contains($content,$marker))$items[$file]=$path;else $notes[]=$file.' was left unchanged because it was not identified as a NOORI copy.';}
 }
 ksort($items);return ['app'=>$app,'root'=>$root,'private'=>$private,'files'=>$items,'notes'=>$notes];
}
function nooriDownloadGuardBlock():string {
 return <<<'GUARD'
# BEGIN NOORI PRIVATE DOWNLOAD GUARD
<FilesMatch "(?i)\.(?:exe|zip|vst3|dll)$">
  Require all denied
</FilesMatch>
<IfModule mod_rewrite.c>
RewriteEngine On
RewriteRule ^(?:noori-activation/)?(?:downloads(?:\.[0-9]+)?/)?NOORI[^/]*\.(?:zip|exe)$ - [F,L,NC]
</IfModule>
# END NOORI PRIVATE DOWNLOAD GUARD
GUARD;
}
function nooriInstallDownloadGuard(string $app,string $private):void {
 [$app,$root,$private]=nooriUpgradePaths($app,$private);$path=$root.'/.htaccess';$original=is_file($path)?file_get_contents($path):'';
 if($original===false)throw new NooriError(503,'Cannot read the root download protection file.');$guard=nooriDownloadGuardBlock();if(str_contains($original,$guard))return;
 $backup=$private.'/guard-backup/'.gmdate('Ymd_His').'_'.bin2hex(random_bytes(4));if(!mkdir($backup,0700,true))throw new NooriError(503,'Cannot create the private protection backup.');
 if(file_put_contents($backup.'/root.htaccess.original',$original,LOCK_EX)===false)throw new NooriError(503,'Cannot back up the root download rules.');
 $clean=preg_replace('/# BEGIN NOORI PRIVATE DOWNLOAD GUARD.*?# END NOORI PRIVATE DOWNLOAD GUARD\s*/s','',$original);
 $temp=$backup.'/root.htaccess.new';if(file_put_contents($temp,$guard."\n".$clean,LOCK_EX)===false||!rename($temp,$path))throw new NooriError(503,'Cannot update root download protection. Ask the host to allow this website to write its own .htaccess.');chmod($path,0644);
}
function nooriSecureLegacyDownloads(string $app,string $private):array {
 $plan=nooriUpgradePlan($app,$private);$root=$plan['root'];$backup=$plan['private'].'/legacy-public-backup/'.gmdate('Ymd_His').'_'.bin2hex(random_bytes(4));
 if(!mkdir($backup,0700,true))throw new NooriError(503,'Cannot create the private backup folder.');
 nooriInstallDownloadGuard($app,$private);
 $moved=[];$errors=[];
 foreach($plan['files'] as $relative=>$source){
  if(is_link($source)){$errors[]=$relative;continue;}$destination=$backup.'/files/'.$relative;
  if(!is_dir(dirname($destination))&&!mkdir(dirname($destination),0700,true)){$errors[]=$relative;continue;}
  if(rename($source,$destination))$moved[]=$relative;else $errors[]=$relative;
 }
 // Keep the domain's root useful after backing up an identified duplicate homepage.
 if(in_array('index.html',$moved,true)){
  $redirect='<!doctype html><html lang="en"><meta charset="utf-8"><meta http-equiv="refresh" content="0;url=/noori-activation/"><title>NOORI CHOIR</title><a href="/noori-activation/">Open NOORI CHOIR</a></html>';
  if(file_put_contents($root.'/index.html',$redirect,LOCK_EX)===false)$errors[]='Root homepage redirect';else chmod($root.'/index.html',0644);
 }
 $report=['moved'=>$moved,'errors'=>$errors,'backup'=>$backup,'time'=>time()];file_put_contents($backup.'/report.json',json_encode($report,JSON_PRETTY_PRINT|JSON_THROW_ON_ERROR),LOCK_EX);return $report;
}
