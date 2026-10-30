#pragma once
#include "Core.h"
#include <RiscTcpListenerV1.h>
namespace Infra {
// Product-owned HTTP/DNS. No sockets survive the setup-to-station transition.
class Portal {
 public:
 static constexpr unsigned InputMax=28672;
 risc_tcp_listener_v1 api{};uint64_t listener=0,client=0,dns=0,deadline=0;
 char* input=nullptr;unsigned used=0,bodyAt=0,bodySize=0,sent=0,headerSent=0;
 bool headers=false,submitted=false,answered=false,fault=false;char nonce[33]{},ssid[33]{},header[512]{},session[256]{};
 const char* body=nullptr;unsigned outputSize=0;const char* message="Connect this device to your 2.4 GHz Wi-Fi.";
 static const char* page();
 static void wipe(void* v,unsigned n){auto* p=static_cast<volatile unsigned char*>(v);while(n--)*p++=0;}
 static bool equal(const char* a,unsigned n,const char* b){if(strlen(b)!=n)return false;for(unsigned i=0;i<n;++i){char c=a[i],d=b[i];if(c>='A'&&c<='Z')c+=32;if(d>='A'&&d<='Z')d+=32;if(c!=d)return false;}return true;}
 template<size_t N> static bool exact(const char* value,unsigned size,const char (&expected)[N]){return size==N-1&&!memcmp(value,expected,N-1);}
 static unsigned dnsReply(uint8_t* p,unsigned n){
  if(n<17||n>496||(p[2]&0xf8)||p[4]||p[5]!=1||p[6]||p[7]||p[8]||p[9])return 0;
  unsigned at=12,total=0;while(at<n&&p[at]){unsigned k=p[at++];if(k>63||at+k>=n||total+k+1>254)return 0;at+=k;total+=k+1;}
  if(at+5>n)return 0;++at;const bool a=p[at]==0&&p[at+1]==1&&p[at+2]==0&&p[at+3]==1;at+=4;
  p[2]=0x84|(p[2]&1);p[3]=0;p[6]=0;p[7]=a?1:0;p[8]=p[9]=p[10]=p[11]=0;
  if(a){const uint8_t answer[]={0xc0,0x0c,0,1,0,1,0,0,0,0,0,4,192,168,4,1};memcpy(p+at,answer,sizeof(answer));at+=sizeof(answer);}return at;
 }
 bool initialize(){if(!input)input=static_cast<char*>(std::malloc(InputMax+1));return input&&api.datagram_tag==RISC_DATAGRAM_TAG&&api.datagram_bind&&api.datagram_receive&&api.datagram_send;}
 int closeHandle(uint64_t& h){if(!h)return 0;int r=api.close(api.context,h);if(r==RISC_TCP_RETAINED){fault=true;return -2;}if(r)return 1;h=0;return 0;}
 int close(){int r=closeHandle(client);if(r)return r;r=closeHandle(dns);if(r)return r;r=closeHandle(listener);if(r)return r;reset();return 0;}
 void reset(){if(input)wipe(input,InputMax+1);used=bodyAt=bodySize=sent=headerSent=0;headers=submitted=answered=false;body=nullptr;outputSize=0;}
 void dispose(){if(!client&&!listener&&!dns&&!fault){reset();std::free(input);input=nullptr;wipe(nonce,sizeof(nonce));}}
 int open(){risc_tcp_listen_v1 req{sizeof(req),{192,168,4,1},80,0};int r;if(!listener){r=api.listen(api.context,&req,&listener);if(r)return r==RISC_TCP_RETAINED?-2:-1;}req.port=53;if(!dns){r=api.datagram_bind(api.context,&req,&dns);if(r)return r==RISC_TCP_RETAINED?-2:-1;}return 0;}
 void respond(unsigned code,const char* content,const char* type="text/plain; charset=utf-8",bool redirect=false){body=content;outputSize=strlen(content);headerSent=sent=0;answered=true;snprintf(header,sizeof(header),"HTTP/1.1 %u %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; connect-src 'self'; form-action 'self'; frame-ancestors 'none'\r\n%s\r\n",code,code==200?"OK":code==302?"Found":"Error",type,outputSize,redirect?"Location: http://192.168.4.1/\r\n":"");}
 // Strict framing: no chunking, duplicate length/host/token, pipeline or
 // cross-origin writes. Nonce is obtained by same-origin GET, never in URLs.
 void decodeHeaders(){
  unsigned end=0;for(unsigned i=0;i+3<used;++i)if(!memcmp(input+i,"\r\n\r\n",4)){end=i+4;break;}
  if(!end){if(used>=2048)respond(431,"Request headers too large.");return;}
  if(end>2048){respond(431,"Request headers too large.");return;}bodyAt=end;headers=true;
  unsigned line=0;while(line+1<end&&memcmp(input+line,"\r\n",2))++line;
  const bool isPost=exact(input,line,"POST /setup HTTP/1.1")||exact(input,line,"POST /setup HTTP/1.0");
  const bool get=line>=5&&!memcmp(input,"GET /",5);
  unsigned hosts=0,tokens=0,origins=0;bool host=false,token=false,length=false,json=false,origin=true,invalid=false;unsigned at=line+2;
  while(at+2<end){unsigned stop=at;while(stop+1<end&&memcmp(input+stop,"\r\n",2))++stop;if(stop==at)break;unsigned colon=at;while(colon<stop&&input[colon]!=':')++colon;if(colon==stop){invalid=true;break;}unsigned val=colon+1;while(val<stop&&input[val]==' ')++val;unsigned n=stop-val;
   if(equal(input+at,colon-at,"host")){if(++hosts>1)invalid=true;host=equal(input+val,n,"192.168.4.1")||equal(input+val,n,"192.168.4.1:80");}
   else if(equal(input+at,colon-at,"content-length")){if(length||!n)invalid=true;length=true;bodySize=0;for(unsigned i=val;i<stop;++i){if(input[i]<'0'||input[i]>'9'||bodySize>InputMax/10){invalid=true;break;}bodySize=bodySize*10+input[i]-'0';}}
   else if(equal(input+at,colon-at,"transfer-encoding"))invalid=true;
   else if(equal(input+at,colon-at,"x-setup-token")){if(++tokens>1)invalid=true;token=n==32&&!memcmp(input+val,nonce,32);}
   else if(equal(input+at,colon-at,"origin")){if(++origins>1)invalid=true;origin=equal(input+val,n,"http://192.168.4.1");}
   else if(equal(input+at,colon-at,"content-type"))json=equal(input+val,n,"application/json");
   at=stop+2;
  }
  if(invalid||bodySize>InputMax-bodyAt){respond(400,"Invalid or oversized request.");return;}
  if(isPost){if(!host||!token||!origin||!json||!length||!bodySize){respond(403,"Open http://192.168.4.1/ and submit the setup form.");return;}}
  else if(get){if(!host){respond(302,"Open device setup.","text/plain",true);return;}if(line>=13&&!memcmp(input,"GET /session ",13)){snprintf(session,sizeof(session),"{\"token\":\"%s\",\"message\":\"%s\"}",nonce,message);respond(200,session,"application/json");}else respond(200,page(),"text/html; charset=utf-8");}
  else respond(405,"Use the setup page.");
 }
 int tick(uint64_t now){
  if(fault)return -2;
  uint8_t packet[512];uint32_t n=0;risc_tcp_listen_v1 peer{};
  int r=api.datagram_receive(api.context,dns,packet,sizeof(packet),&n,&peer);
  if(r==RISC_TCP_RETAINED)return -2;
  if(!r&&peer.address[0]==192&&peer.address[1]==168&&peer.address[2]==4&&(n=dnsReply(packet,n))){r=api.datagram_send(api.context,dns,packet,n,&peer);if(r==RISC_TCP_RETAINED)return -2;}
  if(!client){r=api.accept(api.context,listener,&client);if(r==RISC_TCP_RETAINED)return -2;if(r)return 1;reset();deadline=now+20000;}
  if(answered){const unsigned hlen=strlen(header);bool head=headerSent<hlen;unsigned remaining=head?hlen-headerSent:outputSize-sent;if(remaining){n=0;r=api.write(api.context,client,head?header+headerSent:body+sent,remaining>2048?2048:remaining,&n);if(r==RISC_TCP_RETAINED)return -2;if(!r){if(head)headerSent+=n;else sent+=n;}else if(r!=RISC_TCP_WOULD_BLOCK)deadline=0;}
   if((headerSent==hlen&&sent==outputSize)||now>=deadline){bool wasSubmitted=submitted;r=closeHandle(client);if(r)return r;if(wasSubmitted)return 2;reset();}return 1;}
  if(submitted)return 1;
  if(now>=deadline){r=closeHandle(client);if(!r)reset();return r?r:1;}
  if(used>=InputMax){respond(413,"Setup data too large.");return 1;}
  n=0;r=api.read(api.context,client,input+used,InputMax-used>2048?2048:InputMax-used,&n);if(r==RISC_TCP_RETAINED)return -2;
  if(!r){used+=n;input[used]=0;if(!headers)decodeHeaders();if(headers&&!answered&&used>=bodyAt+bodySize){if(used!=bodyAt+bodySize)respond(400,"Invalid request framing.");else{submitted=true;return 3;}}}
  else if(r!=RISC_TCP_WOULD_BLOCK){r=closeHandle(client);if(!r)reset();return r?r:1;}return 1;
 }
};
inline const char* Portal::page(){return R"HTML(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>RiscRTE setup</title><style>body{font:17px system-ui;background:#101820;color:#edf3f5;margin:0;padding:24px}main{max-width:480px;margin:24px auto}h1{font-size:30px}label{display:block;margin:22px 0 7px}input,textarea,button{box-sizing:border-box;width:100%;padding:14px;border-radius:8px;border:1px solid #71848e;font:inherit}input,textarea{background:#202e38;color:white}button{margin-top:26px;background:#84e0c8;color:#101820;font-weight:700}small,p{line-height:1.5;color:#b7cad3}#status{white-space:pre-wrap}details{margin-top:18px}aside{margin:20px 0;padding:16px;border:1px solid #84e0c8;border-radius:8px}aside p{margin-bottom:0}</style><main><h1>Set up your RiscRTE</h1><p id="intro">Enter your Wi-Fi details. Settings are saved on this device.</p><form id="form"><label for="ssid">Wi-Fi network name</label><input id="ssid" maxlength="32" required autocomplete="off"><small>Use a 2.4 GHz network.</small><label for="password">Wi-Fi password</label><input id="password" type="password" maxlength="63" autocomplete="new-password"><aside id="browser-help" hidden><strong>Choose your file in Chrome</strong><p>Android’s Wi-Fi sign-in window may not open a file picker. Stay connected to RiscRTE-Setup, open Chrome and enter <b>http://192.168.4.1/</b>. Then choose your downloaded pairing file there. You can also paste its contents below.</p></aside><label for="file">Private pairing file</label><input id="file" type="file" accept=".json,application/json"><small>Choose your downloaded pairing file on first setup. Leave it empty when only changing Wi-Fi.</small><details id="paste"><summary>Or paste pairing data</summary><textarea id="pairing" rows="4" autocomplete="off" spellcheck="false"></textarea></details><button id="submit">Save and connect</button></form><p id="status" role="status"></p><p id="local-note">RiscRTE-Setup connects your phone directly to this device. Internet access is not needed to save settings. Keep this Wi-Fi connection until saving finishes.</p></main><script>let token='';const $=id=>document.getElementById(id);const embedded=/Android/i.test(navigator.userAgent)&&(/;\s*wv\)/.test(navigator.userAgent)||/Version\/4\.0/.test(navigator.userAgent));if(embedded){$('browser-help').hidden=false;$('paste').open=true}$('file').onclick=()=>{$('browser-help').hidden=false};fetch('/session',{cache:'no-store'}).then(r=>r.json()).then(s=>{token=s.token;$('intro').textContent=s.message}).catch(()=>{$('status').textContent='Reload this page to start setup.'});$('form').onsubmit=async e=>{e.preventDefault();$('submit').disabled=true;$('status').textContent='Saving settings on your device…';try{if(!token)throw Error('Reload this page to start setup.');const f=$('file').files[0];if(f&&f.size>24000)throw Error('Pairing file is too large.');const raw=f?await f.text():$('pairing').value;const pairing=raw.trim()?JSON.parse(raw):null;const r=await fetch('/setup',{method:'POST',headers:{'Content-Type':'application/json','X-Setup-Token':token},body:JSON.stringify({ssid:$('ssid').value,password:$('password').value,pairing})});const message=await r.text();$('status').textContent=r.ok?message:'Settings were not saved. '+message;if(r.ok){$('password').value='';$('pairing').value='';$('file').value='';$('form').hidden=true}else $('submit').disabled=false}catch(e){$('status').textContent=e.message||'Could not save. Reconnect to setup Wi-Fi and try again.';$('submit').disabled=false}};</script></html>)HTML";}
}
