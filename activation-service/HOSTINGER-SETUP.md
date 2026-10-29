# NOORI Hostinger account portal — owner setup and upgrades

The friend portal lives at `https://www.noorivst.stopncart.com/noori-activation/`.
Live verification must be done after upload; local/CI checks do not establish
that a particular Hostinger installation has been upgraded.

## Upgrade the existing, working activation service

Do not rerun initial setup or replace your signing key. Use the **Final Portal**
artifacts from the successful Windows build:

1. Back up your activation database and the existing `noori-private` directory
   privately. Keep passwords, signing-private.pem and owner.json out of chat/Git.
2. Extract **NOORI-Final-Private-Upgrade.zip** directly INSIDE `noori-private`,
   outside every public_html. It adds portal.php, portal-deploy.php and downloads/.
   It does not contain config.php, owner.json, signing-private.pem,
   public-modulus.txt or installed.marker, so these files remain unchanged.
3. Extract **NOORI-Final-Public-Upgrade.zip** directly INSIDE
   `public_html/noori-activation`. It adds the actual register/login/dashboard/
   password/download pages and updates index.php, admin.php and .htaccess.
   The already configured bootstrap.php is not replaced.
4. Open the normal website → Sign in. Use Owner ID `owner` and the EXISTING owner
   password from setup. The role opens the separate owner dashboard, which installs
   the root public-file deny rules automatically and backs up the old .htaccess.
   It reports any protection failure instead of claiming success. Friend email/
   password opens a limited user dashboard; friends receive HTTP 403 on owner
   pages/actions. Everyone uses the same sign-in screen. Existing sessions need
   to sign in again after this update, without resetting passwords or keys.
   Under Website management open **Review and secure the website upgrade**. It lists the exact old public NOORI files found on your host.
   Review and apply. The operation moves identified installers, public website ZIPs,
   duplicate assets and root PHP copies into `noori-private/legacy-public-backup/`.
   It backs up the root .htaccess and adds a managed NOORI archive-denial rule.
   Nothing is deleted. Unknown hosting files, .private, default.php and llms.txt
   are preserved. Investigate any reported errors before sharing the portal.
5. Generate one trial invitation assigned to your own email. Save the CSV privately.
   In an incognito browser enter the email and invitation key, continue to the
   Create password step, then finish registration. Only the completed,
   authenticated account can download from its user dashboard.
6. Signed out, dashboard.php/download.php must require sign-in. At BOTH the root
   and /noori-activation, old /downloads/...exe, suffixed download folders and
   NOORI-Website-With-Plugin.zip links must return 403 or 404. Re-test installer
   download after sign-in. Test plugin activation in Cubase before sharing keys.

The new account table is added with CREATE TABLE IF NOT EXISTS; existing license
rows, passwords and signing keys are not migrated or reset. A website account
reserves the invitation email but does not start a trial or bind a Windows PC.
The trial starts on the first successful plugin activation and lasts 72 hours.
Every file request checks the current account and license, including revocation
or expiry after the user has logged in. A copied download.php link is not access.
Binaries live only outside the web root and are streamed by an allowlisted PHP
handler; no private path or public installer URL is returned to the browser.

## Expected folders after the upgrade

```text
noori-private/                     OUTSIDE public_html
  config.php                      KEEP existing database settings
  service.php
  signing-private.pem             KEEP existing key, never share
  owner.json                      KEEP existing owner password hash
  installed.marker                KEEP
  public-modulus.txt
  portal.php
  portal-deploy.php
  downloads/                      protected installer, VST3/source and metadata
  legacy-public-backup/            recoverable old public copies
public_html/
  .htaccess                       existing rules plus managed NOORI guard
  index.html                      redirect if an old NOORI homepage was found
  noori-activation/
    bootstrap.php                 KEEP existing private-directory configuration
    .htaccess
    index.php, index.html          index.html redirects to the PHP portal
    home.php, register.php, login.php, dashboard.php, logout.php
    change-password.php, reset-password.php, download.php
    portal-bootstrap.php, admin.php, secure-downloads.php
    assets/
```

There should be no public installer files, website-with-plugin archives or active
public downloads directories. The upgrade tool only moves identified NOORI files;
it does not remove unrelated website data. Any unrecognized leftover needs review.

## Password recovery and owner controls

Users sign up with a valid privately delivered invitation; public self-issue of
trial keys is not allowed. Passwords are stored with password_hash and sessions
use Secure/HttpOnly/SameSite cookies, CSRF tokens and expiry. Changing or recovering
a password invalidates earlier sessions. There are no automatic verification or
reset emails: possession of the privately delivered invitation is the enrollment
proof; mailbox ownership is not independently verified.

If a friend forgets their password, sign in to admin.php → Friend accounts →
Create recovery code. Send the one-time code privately. It expires after 15 minutes
and is stored only as a hash. The friend uses reset-password.php with their email,
the code and a new password. Codes/passwords/serials never belong in chat.

Owner key actions remain available: grant free full access, disable/enable, reset
computer. Resetting a computer does not restart a trial. Trial and full keys use
online checks by default; an existing signed plugin lease can last up to 15 minutes
after an owner change. A disabled/expired invitation immediately blocks the next
website file request, even from an existing web session.

## Initial setup on a fresh host only

Use the source-only NOORI-Activation-Hostinger.zip. Upload public files into
public_html/noori-activation and private files outside ALL public_html roots.
Set bootstrap.php to the actual private path (for the sibling layout above,
`$privateDir = dirname(__DIR__, 2) . '/noori-private';`). Copy config.example.php to
config.php and fill the full MySQL database/user/password, HTTPS public_url, and
an independent random setup_token of at least 32 characters. PHP 8.2+, PDO MySQL
and OpenSSL are required. Open setup.php over HTTPS and choose an owner password
of at least 14 characters. Back up the private key/database and delete setup.php
when complete. Never deploy an old unprotected installer as the friends build.

Version 0.4.1 includes the supplied public trust configuration in
voice-choir/Config/Activation.json and requires registration by default. Private
signing keys stay on the server. CMake/Actions can explicitly override public
configuration. No live activation or Cubase integration is inferred from CI alone.

## Common entry, distinct roles

The homepage contains normal registration and sign-in navigation. There is no
separate public Owner sign-in or upgrade entry in the footer. `account.php` routes
authenticated roles to their own dashboard. Owner ID defaults to `owner`; the
private config may optionally set `owner_login` to a different ID. The owner uses
the original owner password hash; a user cannot choose or submit an owner role.
Owner controls include invitations, recovery, key status, private downloads and
website management. Friends can access only their own account and authorized
files. A user POSTing to admin.php with a valid user CSRF token is still denied.

The first registration step validates an invitation and stores a short-lived
server-side proof containing its hash, not the raw serial. It neither creates an
account nor reserves the email. The password step rechecks the invitation under
a database lock, reserves the email and creates the account. Pending registration
expires after ten minutes and cannot download. Trial/device binding still happens
only in the Windows plugin.

For an immediate manual block during a deployment, prepend the contents of
NOORI-EMERGENCY-DOWNLOAD-GUARD.txt to public_html/.htaccess while preserving its
existing rules. This denies direct public .exe/.zip/.vst3/.dll files throughout
the website. The authenticated PHP download handler remains available. The owner
dashboard installs the same additive block automatically after the new code is
uploaded and the owner signs in. Always verify in an incognito browser afterwards.
