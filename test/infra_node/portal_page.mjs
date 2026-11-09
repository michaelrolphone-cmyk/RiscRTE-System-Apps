import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';
import assert from 'node:assert/strict';
const source=readFileSync(new URL('../../Services/infra_node/Portal.h',import.meta.url),'utf8');
const page=source.match(/R"HTML\(([\s\S]*?)\)HTML"/)[1],script=page.match(/<script>([\s\S]*?)<\/script>/)[1];
assert(!page.includes('This setup network has no internet.'));
assert(page.includes('Internet access is not needed to save settings.'));
for(const mode of ['relay','explorer','sentinel','reconciler','continuity'])assert(page.includes('value="'+mode+'"'));
async function scenario({embedded=false,file=false,status=200,token=true,invalid=false,mode='',identity=false}={}){
 const elements=Object.fromEntries(['intro','browser-help','paste','file','pairing','mode','ssid','password','submit','form','status','native-identity','native-details'].map(id=>[id,{value:'',hidden:id==='browser-help'||id==='native-identity',files:[],disabled:false,open:false,textContent:''}]));
 elements.mode.value=mode;elements.ssid.value='Home';elements.password.value='test-password';
 const pairing={format:'riscrte-setup/1',test:'file bytes'};
 if(file)elements.file.files=[{size:100,text:async()=>JSON.stringify(pairing)}];
 else elements.pairing.value=invalid?'{bad json':JSON.stringify(pairing);
 const requests=[];
 const env={navigator:{userAgent:embedded?'Mozilla/5.0 (Linux; Android 14; Phone; wv) Version/4.0 Chrome/130.0':'Mozilla/5.0 (Linux; Android 14) Chrome/130.0'},document:{getElementById:id=>elements[id]},fetch:async(url,opts)=>{
  requests.push({url,opts});if(url==='/session')return {json:async()=>({token:token?'0123456789abcdef0123456789abcdef':'',message:'Connect this device to your 2.4 GHz Wi-Fi.',...(identity?{biomerte:{pairing:'awaiting_approval',public_key:'PUBLIC KEY'}}:{})})};
  assert.equal(url,'/setup');assert.equal(opts.method,'POST');assert.equal(opts.headers['Content-Type'],'application/json');assert.equal(opts.headers['X-Setup-Token'],'0123456789abcdef0123456789abcdef');
  assert.deepEqual(JSON.parse(opts.body),{ssid:'Home',password:'test-password',pairing,...(mode?{mode}:{})});
  return {ok:status===200,text:async()=>status===200?'Settings saved.':'Check the Wi-Fi details.'};
 }};
 runInNewContext(script,env);await new Promise(resolve=>setImmediate(resolve));
 assert.equal(elements['browser-help'].hidden,!embedded);assert.equal(elements.paste.open,embedded);assert.equal(elements['native-identity'].hidden,!identity);if(identity)assert.equal(JSON.parse(elements['native-details'].textContent).pairing,'awaiting_approval');
 await elements.form.onsubmit({preventDefault(){}});
 if(!token||invalid){assert.equal(requests.length,1);assert.equal(elements.submit.disabled,false);assert.equal(elements.form.hidden,false);}
 else if(status===200){assert.equal(elements.form.hidden,true);assert.equal(elements.password.value,'');assert.equal(elements.pairing.value,'');assert.equal(elements.status.textContent,'Settings saved.');}
 else{assert.equal(elements.form.hidden,false);assert.equal(elements.submit.disabled,false);assert.match(elements.status.textContent,/Settings were not saved/);assert.equal(elements.password.value,'test-password');}
}
await scenario({file:true});await scenario({embedded:true});await scenario({status:400});await scenario({token:false});await scenario({invalid:true});await scenario({identity:true});
console.log('Actual portal page script: file bytes/pasted data, Android WebView guidance, session token, save success/error and input retention PASS (DOM/network mocks; not Android hardware)');


for(const mode of ['explorer','sentinel','reconciler','continuity'])await scenario({mode});
