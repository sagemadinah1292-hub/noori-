<?php
// Copy to config.php OUTSIDE every public_html/web-root directory.
// Do not paste the database password, setup token or private signing key into chat.
return [
 'dsn' => 'mysql:host=localhost;dbname=CHANGE_DATABASE;charset=utf8mb4',
 'db_user' => 'CHANGE_DATABASE_USER',
 'db_password' => 'CHANGE_DATABASE_PASSWORD',
 'public_url' => 'https://www.noorivst.stopncart.com/noori-activation',
 'setup_token' => 'CHANGE_TO_A_RANDOM_SECRET_OF_AT_LEAST_32_CHARACTERS',
 // online: check on launch and renew a 15-minute signed session.
 // activation: full-access keys may run offline; trials still validate online.
 'access_policy' => 'online'
];
