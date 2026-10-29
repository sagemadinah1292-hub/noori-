import { generateKeyPairSync, randomBytes, randomUUID, createHash } from 'node:crypto';
import { writeFileSync, mkdirSync } from 'node:fs';
import { resolve } from 'node:path';
import { normaliseEmail } from '../src/worker.mjs';
const args = process.argv.slice(2), command = args.shift();
function option(name, fallback) { const i=args.indexOf(name);return i>=0?args[i+1]:fallback; }
function quote(value) { return "'"+value.replaceAll("'","''")+"'"; }
if (command === 'keygen') {
  const folder=resolve(option('--out','private'));mkdirSync(folder,{recursive:true,mode:0o700});
  const {privateKey,publicKey}=generateKeyPairSync('rsa',{modulusLength:2048,publicExponent:65537});
  const jwk=privateKey.export({format:'jwk'}), pub=publicKey.export({format:'jwk'});
  writeFileSync(resolve(folder,'signing-private.jwk'),JSON.stringify(jwk),{mode:0o600,flag:'wx'});
  writeFileSync(resolve(folder,'public-modulus.txt'),Buffer.from(pub.n,'base64url').toString('hex')+'\n',{mode:0o644,flag:'wx'});
  console.log('Private signing key written to the private folder. Keep it out of Git and the plugin.');
  console.log('Public modulus written separately for the plugin build.');
} else if (command === 'batch') {
  const count=Number(option('--count','20')),kind=option('--kind','full');
  if(!Number.isInteger(count)||count<1||count>1000||!['trial','full'].includes(kind))throw new Error('Use batch --count 20 --kind full|trial --out private/friends.sql');
  const sql=[],csv=['Serial,Type,Registered email'];
  for(let i=0;i<count;++i){const key='NOORI-'+(kind==='trial'?'T':'F')+'-'+randomBytes(16).toString('hex').toUpperCase();const hash=createHash('sha256').update(key).digest('hex');sql.push(`INSERT INTO licenses(id,email,key_hash,kind,created_at) VALUES(${quote(randomUUID())},NULL,${quote(hash)},${quote(kind)},${Math.floor(Date.now()/1000)});`);csv.push(key+','+kind+',');}
  const output=resolve(option('--out','private/friends.sql'));writeFileSync(output,sql.join('\n')+'\n',{mode:0o600,flag:'wx'});writeFileSync(output+'.csv',csv.join('\n')+'\n',{mode:0o600,flag:'wx'});
  console.log(count+' unique invitation keys written privately to SQL and CSV. No keys are active until their SQL is loaded into your service.');
} else if (command === 'issue') {
  const email=normaliseEmail(option('--email','')), kind=option('--kind','trial');
  if(!email||!['trial','full'].includes(kind))throw new Error('Use issue --email friend@example.com --kind trial|full --out private/invite.sql');
  const serial='NOORI-'+(kind==='trial'?'T':'F')+'-'+randomBytes(16).toString('hex').toUpperCase();
  const hash=createHash('sha256').update(serial).digest('hex'), id=randomUUID();
  const sql=`INSERT INTO licenses(id,email,key_hash,kind,created_at) VALUES(${quote(id)},${quote(email)},${quote(hash)},${quote(kind)},${Math.floor(Date.now()/1000)});\n`;
  const file=resolve(option('--out','private/invite.sql'));writeFileSync(file,sql,{mode:0o600,flag:'wx'});
  // This is an intentionally generated invitation serial, not a server credential.
  console.log('Apply the SQL using wrangler d1 execute, then privately send your friend:');
  console.log('Email: '+email);console.log('Key: '+serial);console.log('Type: '+kind+' (no payment involved)');
} else if (['revoke','reset-device'].includes(command)) {
  const id=option('--id','');if(!/^[a-f0-9-]{36}$/i.test(id))throw new Error('Provide the license UUID with --id');
  const sql=command==='revoke'?`UPDATE licenses SET revoked=1 WHERE id=${quote(id)};\n`:`UPDATE licenses SET device_id=NULL WHERE id=${quote(id)};\n`;
  writeFileSync(resolve(option('--out','private/change.sql')),sql,{mode:0o600,flag:'wx'});
  console.log('Owner-only SQL written. Device reset does not reset trial expiry.');
} else { console.log('Commands: keygen, issue, batch, revoke, reset-device. See README.md.');process.exitCode=1; }
