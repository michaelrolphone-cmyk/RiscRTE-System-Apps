import {createPrivateKey,createPublicKey,createHash} from 'node:crypto';import {writeFileSync} from 'node:fs';import {sealEnrollment} from '../../scripts/relay-enroll.mjs';
const sk=Buffer.alloc(32,7),key=createPrivateKey({key:Buffer.concat([Buffer.from('302e020100300506032b656e04220420','hex'),sk]),format:'der',type:'pkcs8'}),pk=createPublicKey(key).export({type:'spki',format:'der'}).subarray(-32).toString('hex');
const identity={relay_id:'test-relay',enroll_public_key:pk,key_id:createHash('sha256').update(pk).digest('hex')};
const secret={url_path:'/private-path',auth_header_name:'Authorization',auth_header_value:'Bearer test-only-secret',signing_key:'test-only-signature-key'};
const list=[];for(const [id,operation]of [['first','add'],['rotate','rotate'],['revoke','revoke'],['tampered','add'],['wrong-key','add'],['reject','add'],['expire','add']]){
 const request={relay_id:'test-relay',enrollment_id:id,subscriber_id:'grok-worker-1',requested_by:'grok-worker-1',operation,origin:'https://new-hooks.example',subscription:{id:'grok-worker-1-decisions',channel:'capabilities.events',match:{types:['decision.accepted'],filters:[{path:'source.business_key',op:'prefix',value:'grok-worker-1:'}]},enabled:true}};
 const p=sealEnrollment({identity,request,secret:operation==='revoke'?{}:{...secret,auth_header_value:id==='rotate'?'Bearer rotated-secret':secret.auth_header_value}});if(id==='tampered')p.sealed.ciphertext=(p.sealed.ciphertext[0]==='0'?'1':'0')+p.sealed.ciphertext.slice(1);if(id==='wrong-key')p.sealed.key_id='00'.repeat(32);list.push(p);
}
writeFileSync(process.argv[2],JSON.stringify({identity,requests:list}));
