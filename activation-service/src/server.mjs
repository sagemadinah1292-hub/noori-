import { createServer } from 'node:http';
import { mkdirSync, readFileSync, chmodSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { LocalDatabase } from './sqlite.mjs';
import worker from './worker.mjs';

export function createActivationServer(env, handler = worker.fetch) {
  const server = createServer(async (incoming, outgoing) => {
    try {
      let size = 0; const chunks = [];
      for await (const chunk of incoming) {
        size += chunk.length;
        if (size > 4096) {
          outgoing.writeHead(413, { 'Content-Type':'application/json' });
          outgoing.end('{"error":"Request too large."}');return;
        }
        chunks.push(chunk);
      }
      const headers = new Headers();
      for (const [name, value] of Object.entries(incoming.headers))
        if (value !== undefined) headers.set(name, Array.isArray(value) ? value.join(',') : value);
      // Do not trust arbitrary client-provided IP forwarding headers.
      headers.set('CF-Connecting-IP', incoming.socket.remoteAddress || 'unknown');
      const method = incoming.method || 'GET';
      const request = new Request(new URL(incoming.url || '/', 'https://activation.local'), {
        method, headers, ...(!['GET','HEAD'].includes(method) ? { body:Buffer.concat(chunks) } : {})
      });
      const response = await handler(request, env);
      outgoing.writeHead(response.status, Object.fromEntries(response.headers));
      outgoing.end(Buffer.from(await response.arrayBuffer()));
    } catch {
      if (!outgoing.headersSent) outgoing.writeHead(503, { 'Content-Type':'application/json' });
      outgoing.end('{"error":"Activation is temporarily unavailable."}');
    }
  });
  server.requestTimeout=10000;server.headersTimeout=5000;server.keepAliveTimeout=5000;
  return server;
}
if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  const database = resolve(process.env.NOORI_LICENSE_DB || 'data/licenses.sqlite');
  mkdirSync(dirname(database), { recursive:true, mode:0o700 });
  const keyFile = process.env.NOORI_PRIVATE_KEY_FILE;
  if (!keyFile) throw new Error('Set NOORI_PRIVATE_KEY_FILE to a private JWK outside public web folders.');
  const env = { DB:new LocalDatabase(database), LICENSE_PRIVATE_JWK:readFileSync(keyFile,'utf8'),
    ACCESS_POLICY:process.env.NOORI_ACCESS_POLICY || 'online' };
  JSON.parse(env.LICENSE_PRIVATE_JWK);chmodSync(database,0o600);
  const server=createActivationServer(env);
  server.listen(Number(process.env.PORT || 3000), process.env.NOORI_BIND_HOST || '127.0.0.1',
    () => console.log('NOORI activation service started. Use an HTTPS reverse proxy.'));
}
