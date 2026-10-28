# NOORI friends-only activation

For the supplied **Hostinger PHP/MySQL** hosting, use `php/` and
[HOSTINGER-SETUP.md](HOSTINGER-SETUP.md). No Node.js, shop, payment processor,
Gmail password or automated email sender is needed on Hostinger.

The owner page generates a CSV of 20 random invitation keys. Send one privately
to each friend using Gmail/Live.com. Each key binds to the first registered email
and Windows installation. A three-day trial starts on first successful activation,
not when the key is generated. The owner may grant free full access using the same
key, disable it, or deliberately reset a computer binding without resetting a trial.
Keys sent to a known email can be preassigned individually; batch keys bind their
first claimed email. Possession of the privately delivered key is the invitation
proof; the service does not send email verification codes or independently prove
control of a typed mailbox.

The PHP API signs SHA-256/RSA-2048 grants. The VST3 verifies them with Windows CNG
using an embedded public key. Only the server stores the private signing key.
Default policy requires an online check at startup and renews a short signed
lease. Three-day trial expiry is server-side and cannot be restarted by ordinary
reinstall/re-activation. Full keys can optionally allow offline use; offline keys
cannot be promptly revoked while offline. Leave policy `online` for one-computer
control with revocation checks.

The older/reference JavaScript worker and Node/SQLite adapter are retained for
portable protocol tests and optional future hosting; they are not required by the
PHP deployment. Its test server uses ephemeral keys only. Never deploy the fixture
server or include test signing material in a real service.

## Current delivery status

Source, local tests and CI can be prepared without hosting credentials. The live
service is **not deployed**, and a protected friends build cannot be finalized
until HTTPS, the database and the production signing key exist. The normal
unconfigured development build keeps activation enforcement off explicitly.

The project is currently AGPL; activation does not remove recipients' existing
source-code rights. This is protection against casual key sharing, not a claim
that an editable desktop program is impossible to modify.
