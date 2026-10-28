import {generateKeyPairSync,randomUUID} from 'node:crypto';
import {writeFileSync,mkdirSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {LocalDatabase} from '../src/sqlite.mjs';
import {createActivationServer} from '../src/server.mjs';
import worker,{digest,signGrant} from '../src/worker.mjs';
const file=resolve(process.argv[2] || 'license-fixture.json');mkdirSync(dirname(file),{recursive:true});
const {privateKey,publicKey}=generateKeyPairSync('rsa',{modulusLength:2048});
const env={DB:new LocalDatabase(':memory:'),LICENSE_PRIVATE_JWK:JSON.stringify(privateKey.export({format:'jwk'})),ACCESS_POLICY:'online'};
const email='friend@example.com',trialSerial='NOORI-T-'+'1'.repeat(32),fullSerial='NOORI-F-'+'2'.repeat(32),offlineSerial='NOORI-F-'+'3'.repeat(32);
const now=Math.floor(Date.now()/1000);
for(const [serial,kind] of [[trialSerial,'trial'],[fullSerial,'full'],[offlineSerial,'full']])env.DB.db.prepare('INSERT INTO licenses(id,email,key_hash,kind,created_at) VALUES(?,?,?,?,?)').run(randomUUID(),email,await digest(serial),kind,now);
const deviceId='a'.repeat(64),nonce='b'.repeat(32);
const record={id:randomUUID(),email,device_id:deviceId,kind:'trial',expires_at:now+259200};
const token=await signGrant(env,record,nonce,now);
const phpCompatibleToken=await signGrant({...env,SIGNATURE_ALGORITHM:'RS256'},record,nonce,now);
const expired=await signGrant(env,{...record,expires_at:now-1},nonce,now);
const server=createActivationServer(env,async(request,environment)=>{
 const url=new URL(request.url);
 if(url.pathname.startsWith('/offline/')){
  url.pathname=url.pathname.slice('/offline'.length);
  return worker.fetch(new Request(url,request),{...environment,ACCESS_POLICY:'activation'});
 }
 return worker.fetch(request,environment);
});
server.listen(0,'127.0.0.1',()=>{
 const publicModulus=Buffer.from(publicKey.export({format:'jwk'}).n,'base64url').toString('hex');
 writeFileSync(file,JSON.stringify({baseUrl:'http://127.0.0.1:'+server.address().port,publicModulus,email,trialSerial,fullSerial,offlineSerial,deviceId,nonce,token,phpCompatibleToken,expired,pid:process.pid}));
 console.log('Ephemeral activation fixture is ready. No production keys are used.');
});
