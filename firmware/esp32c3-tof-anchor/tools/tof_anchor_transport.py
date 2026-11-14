"""Latest-only anchor HTTP recovery when complete UDP reports stop arriving."""
import concurrent.futures
import ipaddress
import json
import os
from pathlib import Path
import re
import threading
import time
import urllib.request

class AnchorHTTPFallback:
    def __init__(self,path,fetch=None,clock=time.monotonic):
        self.path=Path(path)
        self.fetch=fetch or self._fetch
        self.clock=clock
        self.lock=threading.Lock()
        self.anchors={};self.last_udp={};self.next_poll={};self.latest={};self.health={}
        self.stop=threading.Event();self.thread=None;self.futures={}
        self.executor=None
        if self.path.exists():
            try:
                value=json.loads(self.path.read_text())
                if value.get("schema")=="rover.tof.anchor-http-registry/1":
                    for mac,ip in value.get("anchors",{}).items():self.register(mac,ip,save=False)
            except (OSError,ValueError,TypeError):pass

    @staticmethod
    def _fetch(ip):
        # Identity is validated against the registry before accepting the result.
        with urllib.request.urlopen("http://"+ip+"/api/status",timeout=2) as response:
            return json.load(response)

    def register(self,mac,ip,save=True):
        mac=str(mac).lower()
        if not re.fullmatch(r"(?:[0-9a-f]{2}:){5}[0-9a-f]{2}",mac):return False
        try:address=ipaddress.ip_address(ip)
        except ValueError:return False
        if address.version!=4 or address.is_loopback or address.is_multicast or address.is_unspecified:return False
        with self.lock:
            if mac not in self.anchors and len(self.anchors)>=12:return False
            changed=self.anchors.get(mac)!=str(address)
            self.anchors[mac]=str(address)
            if changed and save:
                self.path.parent.mkdir(parents=True,exist_ok=True)
                temp=self.path.with_suffix(".tmp")
                temp.write_text(json.dumps({"schema":"rover.tof.anchor-http-registry/1","anchors":self.anchors}))
                os.replace(temp,self.path)
        return True

    def note_udp(self,packet,ip,received):
        if packet.get("schema")!="rover.tof.anchor/1" or not packet.get("ftm_responder"):return
        mac=str(packet.get("bssid","")).lower()
        if self.register(mac,ip):
            with self.lock:self.last_udp[mac]=received

    def poll_once(self,mac,ip):
        try:
            packet=self.fetch(ip)
            if (not isinstance(packet,dict) or packet.get("schema")!="rover.tof.anchor/1"
                    or str(packet.get("bssid","")).lower()!=mac or not packet.get("ftm_responder")):
                raise ValueError("anchor_identity_mismatch")
            packet=dict(packet,ip=ip)
            now=self.clock()
            with self.lock:
                if self.anchors.get(mac)!=ip:return False
                # Coalesce by anchor: there is no accumulating telemetry queue.
                self.latest[mac]=(packet,now)
                self.health[mac]={"last_success":now,"error":None}
            return True
        except Exception as error:
            with self.lock:
                self.health[mac]=dict(self.health.get(mac,{}),error=type(error).__name__+":"+str(error)[:100])
            return False

    def start(self):
        self.executor=concurrent.futures.ThreadPoolExecutor(max_workers=12,thread_name_prefix="anchor-status")
        self.thread=threading.Thread(target=self._run,name="anchor-http-fallback",daemon=True)
        self.thread.start()
        return self

    def _run(self):
        while not self.stop.is_set():
            now=self.clock()
            for mac,future in list(self.futures.items()):
                if future.done():
                    future.result()
                    del self.futures[mac]
            with self.lock:
                due=[(mac,ip) for mac,ip in self.anchors.items()
                     if now-self.last_udp.get(mac,float("-inf"))>=6
                     and now>=self.next_poll.get(mac,0) and mac not in self.futures]
                for mac,_ in due:self.next_poll[mac]=now+2
            for mac,ip in due:
                self.futures[mac]=self.executor.submit(self.poll_once,mac,ip)
            self.stop.wait(.1)

    def drain(self):
        with self.lock:
            packets=list(self.latest.values());self.latest.clear()
        return packets

    def status(self):
        now=self.clock()
        with self.lock:
            return {"mode":"udp_with_background_http_recovery","registered":len(self.anchors),
                    "anchors":[{"bssid":mac,"ip":ip,
                    "transport":"udp" if now-self.last_udp.get(mac,float("-inf"))<6 else "http",
                    "last_http_age_s":None if "last_success" not in self.health.get(mac,{}) else now-self.health[mac]["last_success"],
                    "error":self.health.get(mac,{}).get("error")}
                    for mac,ip in sorted(self.anchors.items())]}

    def close(self):
        self.stop.set()
        if self.thread:self.thread.join()
        if self.executor:self.executor.shutdown(wait=True)
