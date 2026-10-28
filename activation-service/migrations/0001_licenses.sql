CREATE TABLE IF NOT EXISTS licenses (
  id TEXT PRIMARY KEY,
  email TEXT,
  key_hash TEXT NOT NULL UNIQUE,
  kind TEXT NOT NULL CHECK(kind IN ('trial','full')),
  created_at INTEGER NOT NULL,
  activated_at INTEGER,
  expires_at INTEGER,
  device_id TEXT,
  revoked INTEGER NOT NULL DEFAULT 0 CHECK(revoked IN (0,1))
);
CREATE UNIQUE INDEX IF NOT EXISTS one_trial_per_email ON licenses(email) WHERE kind='trial';
CREATE INDEX IF NOT EXISTS licenses_email ON licenses(email);
CREATE TABLE IF NOT EXISTS rate_limits (
  bucket TEXT PRIMARY KEY,
  requests INTEGER NOT NULL,
  reset_at INTEGER NOT NULL
);
