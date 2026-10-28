# Hostinger PHP/MySQL setup — owner only

Planned URL: `https://www.noorivst.stopncart.com/noori-activation`.
This is a deployment guide, not a statement that the site has been deployed or
that this domain's TLS certificate has been verified.

## Add activation to the already uploaded download website

Use the two separate activation ZIPs to keep the working download page in place:

- Extract **NOORI-Activation-Public.zip** inside the existing public
  `noori-activation/` directory. It contains PHP endpoint/setup/owner files and
  the standard routing .htaccess. It does not replace index.html, assets or downloads.
- Extract **NOORI-Activation-Private.zip** only inside `noori-private/` OUTSIDE
  every public_html/web-root directory. Do not upload or extract this private ZIP
  in the public website. It contains template/source files, not a production config
  or signing key. Continue with private path/config/setup steps below.

Do not re-extract the full source-site ZIP over the working download website:
its generic welcome page is not the generated page with local installer downloads.
Back up a custom .htaccess before replacing it. Do not overwrite an existing live
bootstrap.php configuration or private directory during an upgrade.

## Files and secrets

1. In Hostinger, enable HTTPS for the domain and select PHP 8.2 or newer with
   PDO MySQL and OpenSSL enabled.
2. Create a **new, separate MySQL database and database user** for NOORI activation.
   Keep its password in Hostinger/the private config. Do not send it in chat.
3. Upload `noori-activation/` from the hosting ZIP into the domain's web root.
   The public URL should then be `/noori-activation/`.
4. Upload `noori-private/` **outside every public_html/web-root directory**, for
   example `/home/u12345678/noori-private`. Do not put it inside another website's
   public folder. Use owner-only directory/file permissions where supported.
5. Edit the public `bootstrap.php` and set its private-directory path to that exact
   absolute server path. The path is not a password; the files in that directory
   contain secrets after setup.
6. In the private directory, copy `config.example.php` to `config.php`. Enter the
   database name, user and password, keep the correct public URL, and replace the
   setup token with a unique random secret of at least 32 characters. Do not leave
   any `CHANGE...` placeholders. Do not commit this file to Git.
7. Open `/noori-activation/setup.php` using HTTPS. Enter that setup token and choose
   an owner password of at least 14 characters. Setup creates the tables, private
   RSA signing key, public modulus and owner password hash. Back up the private
   folder and database securely. Losing/changing the signing key requires a new
   plugin trust-key build.
8. Delete the public `setup.php` after successful setup. It also refuses to run
   again once the private installed marker exists. Clear the now-unused setup
   token in the private configuration.

## Finish the protected Windows build

1. Log in to `/noori-activation/admin.php`.
2. Copy the **public modulus** shown at the bottom. This public key and the API URL
   can safely be supplied for the protected plugin build. Never supply the private
   PEM, database password, setup token, or owner password.
3. Compile with `NOORI_ENABLE_LICENSING=ON`, `NOORI_LICENSE_SERVER_URL` set to the
   URL above, and `NOORI_LICENSE_PUBLIC_KEY_HEX` set to that public modulus. CMake
   rejects an enabled build without HTTPS/public-key configuration.
   For GitHub Actions, set these three repository **Actions variables** (public
   configuration, not secrets): `NOORI_ENABLE_LICENSING=ON`,
   `NOORI_LICENSE_SERVER_URL`, and `NOORI_LICENSE_PUBLIC_KEY_HEX`, then run the
   Windows workflow. With no variables, Actions deliberately builds an
   unprotected development installer; `BUILD-INFO.json` records that status.
4. Test one trial key end to end on the live host, including expiry/device binding,
   before distributing the protected setup to friends. The unconfigured developer
   installer is not a substitute for this step.

## Make the 20 keys

In the owner page choose **20**, then **Three-day trial** or **Free full access**,
and click **Generate and download CSV**. Keep that CSV private; the database stores
only key hashes and short hints, so full serials cannot be recovered later. Send
one key per friend. To preassign an email, generate a single key with that address.
A batch key otherwise binds its first registered email and computer.

Friends install the protected plugin, open **Settings → Registration / Trial**,
and enter their email and invitation key. The plugin sends only that email, key,
request nonce and a hashed Windows-installation identifier over HTTPS. Audio is
never sent to the activation service.

A key remains usable on its registered Windows installation, including normal
plugin restarts. Another computer is rejected. Windows reinstallation/identity
changes may need an owner-approved reset. Deleting the local activation cache
neither releases a key nor resets the server's trial deadline.

Use **Grant full access** after a trial to allow that friend permanent free access
with the same key. **Disable** and **Reset computer** are explicit owner actions.
An existing signed online session may remain usable for up to 15 minutes after an
owner change. Keys cannot automatically create a fresh trial. Trial keys always
require online validation; full-key offline policy is optional and defaults off.

## Checks and recovery

- `/noori-activation/health` must return `activationReady: true` over HTTPS.
- Test the actual activation API and plugin, not health alone.
- Owner login has rate limiting, secure/HttpOnly/SameSite cookies and CSRF-protected
  state changes. Use HTTPS, a unique password and a current PHP version.
- Do not enable request-body logging that records serials. Do not publish CSV files,
  config, PEM keys, owner JSON or database dumps.
- Database contents include registered emails, key hashes, machine-ID hashes and
  activation/expiry times. Retain them only as needed for your friends' access.
- If unavailable, the plugin preserves dry track audio. Licensed features stop when
  the current online lease ends; a network failure never deletes generated audio.

## Public welcome page

The public directory includes a responsive NOORI welcome page with the supplied
white logo, plugin registration instructions and FAQs. Apache's DirectoryIndex
selects index.html, while the existing PHP API endpoints continue to use index.php.
The page can be displayed before the private database service is configured.
It does not accept serials or register computers in a web browser.

For the welcome page alone, upload and extract NOORI-Page-Only.zip inside this
domain's public_html/noori-activation folder. The ZIP includes index.html, assets,
and the standard activation-directory .htaccess. Back up an existing custom
.htaccess before replacing it. It contains no owner portal, API implementation,
configuration, credentials or private signing keys. Complete the full service
setup above before using Owner sign in or distributing the protected plugin.

## Website with actual plugin downloads

Use **NOORI-Website-With-Plugin.zip** when you want the installer and VST3 archive
hosted on your own website. Upload and extract its contents directly inside
`public_html/noori-activation/`: `index.html`, `.htaccess`, `assets/`, and
`downloads/` belong at that level, not inside another nested folder. Back up an
existing custom `.htaccess` before replacement. Existing PHP service/config files
are not included or overwritten. The welcome-page-only ZIP does not include binaries.

This package includes the real installer, VST3 plus project sources, the actual
plugin screenshot, installation instructions, SHA-256 checksums and build evidence.
The download buttons use relative local file URLs; friends do not need GitHub
accounts. The current owner development build explicitly says that serial/trial
enforcement is disabled. It must not be described as the protected trial installer.
The live activation backend and its public signing key are still required for that.

Actions builds this complete package after all Windows tests and the installer
smoke test pass, and uploads it as `NOORI-Website-With-Plugin`. For a local package
using existing tested artifacts, run `scripts/package-website.py --help`.
Public metadata uses `.txt` filenames because the activation directory blocks
public access to `.json` files. No private keys or live credentials go in downloads.
