const PRODUCT = 'noori-choir';
export const TRIAL_SECONDS = 3 * 24 * 60 * 60;
export const LEASE_SECONDS = 15 * 60;
const encoder = new TextEncoder();

export function normaliseEmail(value) {
  if (typeof value !== 'string') return null;
  const email = value.trim().toLowerCase();
  return email.length <= 254 && /^[a-z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-z0-9](?:[a-z0-9.-]*[a-z0-9])?\.[a-z]{2,63}$/.test(email) ? email : null;
}
export async function digest(value) {
  const bytes = await crypto.subtle.digest('SHA-256', encoder.encode(value));
  return Array.from(new Uint8Array(bytes), b => b.toString(16).padStart(2, '0')).join('');
}
function json(status, body) {
  return new Response(JSON.stringify(body), { status, headers: {
    'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store',
    'X-Content-Type-Options': 'nosniff', 'Strict-Transport-Security': 'max-age=31536000'
  } });
}
function base64(bytes) {
  let text = ''; for (const b of new Uint8Array(bytes)) text += String.fromCharCode(b);
  return btoa(text);
}
export async function signGrant(env, record, nonce, now) {
  if (!env.LICENSE_PRIVATE_JWK) throw new Error('Signing is not configured');
  const algorithm = env.SIGNATURE_ALGORITHM === 'RS256' ? 'RS256' : 'PS256';
  const key = await crypto.subtle.importKey('jwk', JSON.parse(env.LICENSE_PRIVATE_JWK),
    { name: algorithm === 'RS256' ? 'RSASSA-PKCS1-v1_5' : 'RSA-PSS', hash: 'SHA-256' }, false, ['sign']);
  const onlineRequired = record.kind === 'trial' || env.ACCESS_POLICY !== 'activation';
  const validUntil = onlineRequired ? Math.min(now + LEASE_SECONDS, record.expires_at ?? Number.MAX_SAFE_INTEGER) : 0;
  const body = encoder.encode(JSON.stringify({ version: 1, product: PRODUCT,
    licenseId: record.id, email: record.email, deviceId: record.device_id,
    kind: record.kind, issuedAt: now, trialExpiresAt: record.expires_at ?? 0,
    validUntil, onlineRequired, nonce }));
  const signature = await crypto.subtle.sign(algorithm === 'RS256' ? { name: 'RSASSA-PKCS1-v1_5' } : { name: 'RSA-PSS', saltLength: 32 }, key, body);
  return { algorithm, payload: base64(body), signature: base64(signature) };
}
async function rateLimit(env, bucket, now, maximum) {
  const row = await env.DB.prepare(`
    INSERT INTO rate_limits(bucket,requests,reset_at) VALUES(?,1,?)
    ON CONFLICT(bucket) DO UPDATE SET
      requests=CASE WHEN reset_at<=? THEN 1 ELSE requests+1 END,
      reset_at=CASE WHEN reset_at<=? THEN excluded.reset_at ELSE reset_at END
    RETURNING requests
  `).bind(bucket, now + 60, now, now).first();
  return row.requests <= maximum;
}

export async function handleRequest(request, env, clock = () => Math.floor(Date.now() / 1000)) {
  const url = new URL(request.url);
  if (url.pathname === '/health' && request.method === 'GET') return json(200, { service: PRODUCT, protocol: 1 });
  if (!['/v1/activate', '/v1/check'].includes(url.pathname)) return json(404, { error: 'Not found.' });
  if (request.method !== 'POST') return json(405, { error: 'Use POST.' });
  if (!request.headers.get('content-type')?.toLowerCase().startsWith('application/json'))
    return json(415, { error: 'JSON request required.' });
  if (!env.DB || !env.LICENSE_PRIVATE_JWK) return json(503, { error: 'Activation service is not configured.' });
  const now = clock();
  // Only Cloudflare's edge-supplied connecting address is used, never X-Forwarded-For.
  const ip = request.headers.get('CF-Connecting-IP') ?? 'unknown';
  if (!await rateLimit(env, 'ip:' + await digest(ip), now, 60)) return json(429, { error: 'Too many attempts. Wait one minute.' });
  if (Number(request.headers.get('Content-Length') || 0) > 4096) return json(413, { error: 'Request too large.' });
  let body;
  const reader = request.body?.getReader();
  if (!reader) return json(400, { error: 'Request body required.' });
  let length = 0, chunks = [];
  for (;;) {
    const { done, value } = await reader.read(); if (done) break;
    length += value.byteLength;
    if (length > 4096) { await reader.cancel(); return json(413, { error: 'Request too large.' }); }
    chunks.push(value);
  }
  try {
    const bytes = new Uint8Array(length); let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.byteLength; }
    body = JSON.parse(new TextDecoder().decode(bytes));
  } catch { return json(400, { error: 'Invalid JSON.' }); }
  const email = normaliseEmail(body.email);
  if (!email || typeof body.serial !== 'string' || !/^NOORI-[TF]-[A-F0-9]{32}$/.test(body.serial)
      || typeof body.deviceId !== 'string' || !/^(?:[a-f0-9]{64}|[a-f0-9-]{32,36})$/i.test(body.deviceId)
      || typeof body.nonce !== 'string' || !/^[a-f0-9-]{32,36}$/i.test(body.nonce))
    return json(400, { error: 'Enter the email and key supplied by NOORI.' });
  const keyHash = await digest(body.serial);
  if (!await rateLimit(env, 'key:' + keyHash, now, 10)) return json(429, { error: 'Too many key attempts. Wait one minute.' });
  let record = await env.DB.prepare('SELECT * FROM licenses WHERE key_hash=?').bind(keyHash).first();
  if (!record || (record.email && record.email !== email)) return json(403, { error: 'The email and serial number do not match.' });
  if (record.revoked) return json(403, { error: 'This key has been disabled. Contact NOORI.' });
  if (record.kind === 'trial' && record.expires_at !== null && record.expires_at <= now)
    return json(403, { error: 'Your three-day trial has ended. Ask NOORI for a full-access key.' });
  if (record.device_id && record.device_id !== body.deviceId)
    return json(403, { error: 'This key is active on another installation. Ask NOORI to reset it.' });
  if (url.pathname === '/v1/check' && !record.device_id)
    return json(403, { error: 'Activate this key first.' });
  if (url.pathname === '/v1/activate') {
    if (record.kind === 'trial') {
      const previous = await env.DB.prepare("SELECT id FROM licenses WHERE email=? AND kind='trial' AND id<>?").bind(email,record.id).first();
      if (previous) return json(403,{error:'A trial has already been assigned to this email. Ask NOORI for full access.'});
    }
    // One atomic statement prevents concurrent first activations from binding two devices
    // or starting the three-day clock twice. Re-activation never replaces the original expiry.
    record = await env.DB.prepare(`UPDATE licenses SET
      email=COALESCE(email,?), device_id=COALESCE(device_id,?), activated_at=COALESCE(activated_at,?),
      expires_at=CASE WHEN kind='trial' THEN COALESCE(expires_at,?) ELSE NULL END
      WHERE id=? AND revoked=0 AND (device_id IS NULL OR device_id=?)
      AND (email IS NULL OR email=?) AND (kind='full' OR expires_at IS NULL OR expires_at>?) RETURNING *
    `).bind(email, body.deviceId, now, now + TRIAL_SECONDS, record.id, body.deviceId, email, now).first();
    if (!record) return json(403, { error: 'Activation is no longer available for this installation.' });
  }
  return json(200, await signGrant(env, record, body.nonce, now));
}

export default {
  async fetch(request, env) {
    try { return await handleRequest(request, env); }
    catch { return json(503, { error: 'Activation is temporarily unavailable. Please try again later.' }); }
  },
  async scheduled(_event, env) {
    await env.DB.prepare('DELETE FROM rate_limits WHERE reset_at<?').bind(Math.floor(Date.now()/1000)-86400).run();
  }
};
