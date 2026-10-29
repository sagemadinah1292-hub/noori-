# NOORI 0.4.2: automatic verified trials

Upload private upgrade contents into the existing noori-private folder OUTSIDE
public_html, then public contents into public_html/noori-activation. Replace code
files. Never replace config.php, signing-private.pem, owner.json, bootstrap.php,
notification-settings.json or the database. No setup rerun is needed. New token
storage is created automatically with the existing database user.

NEW ACCOUNT FLOW
1. Register email and password. Downloads remain locked until email confirmation.
2. An automatic confirmation email is sent using the existing configured hosting
   sender. Check inbox and Junk/Spam. Its link expires after 24 hours; a GET by a
   mail scanner does not grant access. The recipient confirms using the form.
3. A single trial key is generated for the confirmed email. Sign in with the SAME
   original password. Your own dashboard shows your serial and download buttons.
4. Use that email/serial in the plugin. First successful activation starts a fixed
   72-hour period. Closing/not using the plugin does not pause or restart it.
5. Trial is online-validated and binds to the first computer. Reusing the same
   serial on a different computer or email is denied. Duplicate email signup is
   denied. Owner computer resets never restart the trial.
6. Only the owner can grant Full access. The automatic trial serial stays the
   same after that upgrade. Existing manually issued keys continue to work; their
   users can enter those keys in their existing dashboard.

PASSWORDS
The password-creation form now supplies the email username to password managers.
Repeated sign-ins and sign-out/sign-in after verification are regression-tested.
The reported production failure was not independently reproduced; if it persists,
report the exact visible error, never the password. Forgot password now emails a
one-time 15-minute link directly. The owner no longer needs to create a recovery
code. Recovery responses do not disclose whether an address has an account.

EMAIL
Keep the existing hosting sender setting (noori@noorivst.stopncart.com) and owner
recipient setting. Real Gmail verification/reset delivery must be tested after
upload. Server acceptance does not guarantee inbox placement. No SMTP password is
required by the PHP mail transport. Mail verification controls trial signup, not
owner or hosting access. Owner/user roles remain separate.

PLUGIN
Install the accompanying tested 0.4.2 Windows x64 build with Cubase closed. Live
Low-Cut/Brightness/Reverb preview uses smoothed DSP and does not stop/rewind the
preview for offline FX rendering. Drag-ready WAV files are prepared on request;
export renders current FX settings. The plugin displays Trial/Demo plus expiry,
Trial expired, or Full status. A real Cubase playback test is still required.

Direct drag FROM a Cubase audio event is not implemented or verified. The drop
area accepts filesystem WAV/AIFF files. For Cubase events use CAPTURE FROM TRACK
or export/browse the WAV. No ARA is present. No neural choir model or code-signed
installer is claimed.
