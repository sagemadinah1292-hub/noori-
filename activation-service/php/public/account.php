<?php
declare(strict_types=1);
require __DIR__.'/portal-bootstrap.php';
if(nooriOwner())nooriRedirect('admin.php');if($friend)nooriRedirect('dashboard.php');nooriRedirect('login.php');
