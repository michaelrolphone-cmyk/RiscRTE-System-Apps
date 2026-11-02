// Exercise the real enrollment/transport engine with an isolated in-memory DB.
// The live monitor is never impersonated and no network/hardware effects occur.
import assert from 'node:assert/strict';
import {pathToFileURL} from 'node:url';
const source=process.env.CAPABILITIES_SOURCE;
if(!source)throw Error('Set CAPABILITIES_SOURCE to a capabilities-service checkout');
const {environment,fixture}=await import(pathToFileURL(source+'/tests/harness.mjs'));
const {handle}=await import(pathToFileURL(source+'/lib/capabilities/transport.mjs'));
const e=environment(),f=await fixture(e,{name:'reflash-recovery'});
const call=async(operation,args,{credential=f.credential,enrollment=false,key=crypto.randomUUID()}={})=>{
 const response=await handle(new Request('https://test/v1/call',{method:'POST',headers:{'Content-Type':'application/json',authorization:'Bearer '+credential,...(enrollment?{'x-enrollment-id':f.enrollment.id}:{})},body:JSON.stringify({protocol_version:'1.0',family:'participant',operation,operation_id:key,arguments:args})}),e,{});
 return {http:response.status,...await response.json()};
};
const rejected=await call('enroll',{bootstrap_credential:f.bootstrap,identity_binding:f.participant.identity_binding,display_name:'reflash',class:'virtual_worker'},{credential:f.bootstrap,enrollment:true});
assert.equal(rejected.http,401);assert.equal(rejected.code,'UNAUTHENTICATED');
const args={participant_id:f.participant.id,boot_id:'restored-identity-boot',adapter_protocols:['1.0'],journal_summary:{reset_reason:5,previous_stage:'http'},adapter_version:'biome-monitor/1.1.2'};
const key=crypto.randomUUID(),restored=await call('open_session',args,{key});
assert.equal(restored.http,200);assert.equal(restored.status,'completed');
assert.equal(restored.data.participant_id,f.participant.id);
assert.deepEqual(restored.data.journal_summary,args.journal_summary);
assert.equal((await call('open_session',args,{key})).data.id,restored.data.id);
assert.equal((await call('open_session',args,{credential:'invalid-test-credential'})).http,401);
const other=await fixture(e,{name:'other-device'});
assert.equal((await call('open_session',{...args,participant_id:other.participant.id})).code,'FORBIDDEN');
const current=await call('get',{participant_id:f.participant.id});
assert.equal(current.data.credential_epoch,f.participant.credential_epoch);
console.log('9 enrollment/reflash transport assertions passed (isolated DB; no physical device)');
