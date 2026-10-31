import {readFileSync} from 'node:fs';import {createHmac} from 'node:crypto';import assert from 'node:assert/strict';
const v=JSON.parse(readFileSync('/tmp/unified-relay-signature.json'));const signed=v.id+'.'+v.timestamp+'.'+v.body;
assert.equal(v.signature,[42,43].map(b=>'v1,'+createHmac('sha256',Buffer.alloc(32,b)).update(signed).digest('base64')).join(' '));
console.log('Standard Webhooks: independent Node HMAC checks exact body, ID, timestamp, old/new key rotation PASS');
