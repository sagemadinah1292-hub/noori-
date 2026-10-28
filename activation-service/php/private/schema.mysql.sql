CREATE TABLE IF NOT EXISTS noori_licenses (
 id CHAR(36) PRIMARY KEY,
 email VARCHAR(254) COLLATE utf8mb4_bin NULL,
 key_hash CHAR(64) NOT NULL UNIQUE,
 key_hint VARCHAR(12) NOT NULL,
 kind ENUM('trial','full') NOT NULL,
 created_at BIGINT NOT NULL,
 activated_at BIGINT NULL,
 expires_at BIGINT NULL,
 device_id CHAR(64) NULL,
 revoked TINYINT NOT NULL DEFAULT 0,
 trial_email VARCHAR(254) COLLATE utf8mb4_bin GENERATED ALWAYS AS (CASE WHEN kind='trial' THEN email ELSE NULL END) STORED,
 UNIQUE KEY one_trial_per_email(trial_email),
 KEY email_index(email)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS noori_rate_limits (
 bucket VARCHAR(96) PRIMARY KEY,
 requests INT NOT NULL,
 reset_at BIGINT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
