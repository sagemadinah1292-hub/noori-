import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {generateKeyPairSync,randomUUID} from 'node:crypto';
import {readFileSync} from 'node:fs';
import {handleRequest,digest,TRIAL_SECONDS,normaliseEmail} from '../src/worker.mjs';
const {privateKey,publicKey}=generateKeyPairSync('rsa',{modulusLength:2048});
class Database {
 constructor(){this.db=new DatabaseSync(':memory:');this.db.exec(readFileSync(new URL('../migrations/0001_licenses.sql',import.meta.url),'utf8'));}
 prepare(sql){const stmt=this.db.prepare(sql);return {bind:(...args)=>({first:async()=>stmt.get(...args)??null,run:async()=>stmt.run(...args)})};}
}
async function setup(kind='trial',policy='online'){
 const env={DB:new Database(),LICENSE_PRIVATE_JWK:JSON.stringify(privateKey.export({format:'jwk'})),ACCESS_POLICY:policy};
 const serial='NOORI-'+(kind==='trial'?'T':'F')+'-'+'A'.repeat(32),email='friend@example.com',deviceId=randomUUID();let now=1800000000;
 env.DB.db.prepare('INSERT INTO licenses(id,email,key_hash,kind,created_at) VALUES(?,?,?,?,?)').run(randomUUID(),email,await digest(serial),kind,now);
 async function call(path='/v1/activate',extra={}){return handleRequest(new Request('https://activation.example'+path,{method:'POST',headers:{'Content-Type':'application/json','CF-Connecting-IP':'127.0.0.1'},body:JSON.stringify({email,serial,deviceId,nonce:randomUUID(),...extra})}),env,()=>now);}
 return {env,email,serial,deviceId,call,advance:seconds=>now+=seconds,now:()=>now};
}
function payload(response){return JSON.parse(Buffer.from(response.payload,'base64'));}
test('three-day trial begins once, at first activation',async()=>{
 const s=await setup();s.advance(90000);let r=await s.call();assert.equal(r.status,200);let grant=payload(await r.json());assert.equal(grant.trialExpiresAt,s.now()+TRIAL_SECONDS);const end=grant.trialExpiresAt;
 s.advance(100);grant=payload(await (await s.call()).json());assert.equal(grant.trialExpiresAt,end);
 s.advance(TRIAL_SECONDS);r=await s.call();assert.equal(r.status,403);assert.match((await r.json()).error,/trial has ended/);
});
test('signed response verifies with public key, tampering fails',async()=>{
 const s=await setup();const token=await (await s.call()).json();const key=await crypto.subtle.importKey('jwk',publicKey.export({format:'jwk'}),{name:'RSA-PSS',hash:'SHA-256'},false,['verify']);
 assert.equal(await crypto.subtle.verify({name:'RSA-PSS',saltLength:32},key,Buffer.from(token.signature,'base64'),Buffer.from(token.payload,'base64')),true);
 const bytes=Buffer.from(token.payload,'base64');bytes[8]^=1;assert.equal(await crypto.subtle.verify({name:'RSA-PSS',saltLength:32},key,Buffer.from(token.signature,'base64'),bytes),false);
});
test('email binding and device binding reject a copied key',async()=>{
 const s=await setup();assert.equal((await s.call(undefined,{email:'other@example.com'})).status,403);assert.equal((await s.call()).status,200);assert.equal((await s.call(undefined,{deviceId:randomUUID()})).status,403);
});
test('reinstall/device reset cannot create another trial or extend expiry',async()=>{
 const s=await setup();const first=payload(await (await s.call()).json());s.advance(300);s.env.DB.db.exec('UPDATE licenses SET device_id=NULL');
 const changed=payload(await (await s.call(undefined,{deviceId:randomUUID()})).json());assert.equal(changed.trialExpiresAt,first.trialExpiresAt);
 assert.throws(()=>s.env.DB.db.prepare('INSERT INTO licenses(id,email,key_hash,kind,created_at) VALUES(?,?,?,?,?)').run(randomUUID(),s.email,'another-hash','trial',s.now()),/UNIQUE/);
});
test('revoked keys fail activation and heartbeat',async()=>{
 const s=await setup();await s.call();s.env.DB.db.exec('UPDATE licenses SET revoked=1');assert.equal((await s.call('/v1/check')).status,403);
});
test('heartbeat cannot start an unused trial',async()=>{const s=await setup();assert.equal((await s.call('/v1/check')).status,403);assert.equal(s.env.DB.db.prepare('SELECT activated_at FROM licenses').get().activated_at,null);});
test('free full access supports configurable online or activation-only policy',async()=>{
 for(const policy of ['online','activation']){const s=await setup('full',policy);let grant=payload(await (await s.call()).json());assert.equal(grant.kind,'full');assert.equal(grant.trialExpiresAt,0);assert.equal(grant.onlineRequired,policy==='online');assert.equal(grant.validUntil,policy==='online'?s.now()+900:0);}
});
test('trial remains online-validated even when full-access keys allow offline use',async()=>{const s=await setup('trial','activation');assert.equal(payload(await (await s.call()).json()).onlineRequired,true);});
test('key request attempts are rate limited',async()=>{const s=await setup();for(let i=0;i<10;i++)await s.call();assert.equal((await s.call()).status,429);s.advance(61);assert.equal((await s.call()).status,200);});
test('email normalization and malformed serial checks',async()=>{assert.equal(normaliseEmail(' FRIEND@EXAMPLE.COM '),'friend@example.com');assert.equal(normaliseEmail('not-email'),null);const s=await setup();assert.equal((await s.call(undefined,{serial:'fake'})).status,400);});
test('response includes caller nonce and current installation',async()=>{const s=await setup(),nonce=randomUUID();const grant=payload(await (await s.call(undefined,{nonce})).json());assert.equal(grant.nonce,nonce);assert.equal(grant.deviceId,s.deviceId);});

test('unassigned batch key binds its first email and device atomically',async()=>{
 const s=await setup('full');s.env.DB.db.exec('UPDATE licenses SET email=NULL');
 assert.equal((await s.call()).status,200);
 assert.equal((await s.call(undefined,{email:'second@example.com',deviceId:randomUUID()})).status,403);
 assert.equal(s.env.DB.db.prepare('SELECT email FROM licenses').get().email,s.email);
});
test('64-character computer fingerprints are accepted',async()=>{const s=await setup();assert.equal((await s.call(undefined,{deviceId:'b'.repeat(64)})).status,200);});
