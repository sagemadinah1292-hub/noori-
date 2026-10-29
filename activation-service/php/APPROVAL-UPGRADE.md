# Owner-approved accounts

New accounts register with email and password and remain pending. They can sign in
and change their password, but every download request returns 403 until they
redeem an owner-issued key explicitly assigned to their account email. Unassigned,
other-email, revoked and expired keys cannot unlock a pending account. Signup and
approval never start a trial or bind a computer. Previously approved accounts and
existing owner credentials are preserved.

Upload the private upgrade into the existing noori-private directory outside
public_html. Upload the public upgrade into public_html/noori-activation. Extract
with Replace/Overwrite, never Keep both. Do not replace config.php, bootstrap.php,
owner.json or signing-private.pem. No setup rerun or manual SQL import is needed.
The new requests table is created automatically with the existing database user.

Sign in as owner using the existing password. Under Registration notifications,
set a real sender email authorized by Hostinger for your domain. The notification
recipient defaults to shahidulhaq@live.com. Notifications use hosting PHP mail();
no Live.com password is needed. Delivery depends on hosting mail configuration,
SPF/DKIM and recipient filtering. accepted_by_mail_server is not proof of delivery;
not_sent means the message was not accepted or no sender was configured. Pending
requests remain visible even when email fails. Retry notification after setting
the sender and check both inbox and spam. No automatic approval occurs.

For an approved request, generate ONE trial or full-access key assigned to its
exact email and privately give the CSV serial to the user. The user enters it in
My account. They keep the same key for activation inside the Windows plugin.
Without this final key the account stays locked indefinitely.

Acceptance test in a private browser: register without a serial; confirm download
endpoint returns 403; confirm owner sees the request and receives its email;
confirm a fabricated/other-email/unassigned key does not unlock; assign and redeem
a real final key; confirm authorized download; revoke and confirm download 403.
Also check the OLD root /downloads/NOORI_CHOIR_Setup_x64.exe URL returns 403/404.

This update does not verify mailbox ownership, provide an SMTP client, or alter
the Windows audio engine. End-to-end delivery must be checked on the real host.
