"""Identity routes shared with the rover gateway; no measurement data is stored."""
import ipaddress
import json
import re
import sqlite3
import subprocess
import time
from urllib.parse import urlsplit,urlunsplit

def mac(value):
    return ":".join(part.zfill(2) for part in str(value).lower().split(":"))

def neighbor_hosts(bssid):
    # Candidate inference only. The caller must verify the responder BSSID.
    parts=[int(p,16) for p in bssid.split(":")]
    preceding=parts.copy();preceding[-1]=(preceding[-1]-1)%256
    universal=parts.copy();universal[0]&=~2
    station={":".join("%02x"%p for p in row) for row in [parts,preceding,universal]}
    try:output=subprocess.check_output(["arp","-an"],text=True,timeout=3)
    except (OSError,subprocess.SubprocessError):return []
    return [host for host,address in re.findall(r"\(([^)]+)\) at ([0-9a-fA-F:]+)",output) if mac(address) in station]

class GatewayRoutes:
    def __init__(self,path):
        self.path=str(path)
        from tof_host_io import client
        self.client=client()
    def get(self,device_id):
        if self.client is not None:return self.client.route(device_id)
        with sqlite3.connect(self.path,timeout=3) as db:
            db.row_factory=sqlite3.Row
            row=db.execute("SELECT * FROM routes WHERE device_id=?",(device_id,)).fetchone()
        return dict(row) if row else None
    def origin(self,url,device_id):
        if self.client is not None:return self.client.origin(url,device_id)
        row=self.get(device_id)
        if not row:return url
        parsed=urlsplit(url);host=str(ipaddress.ip_address(row["host"]))
        if ":" in host:host="["+host+"]"
        if parsed.port:host+=":"+str(parsed.port)
        if "@" in parsed.netloc:host=parsed.netloc.rsplit("@",1)[0]+"@"+host
        return urlunsplit(parsed._replace(netloc=host))
    def candidates(self,bssid):
        if self.client is not None:return self.client.candidates(bssid)
        row=self.get("ftm:"+bssid)
        return list(dict.fromkeys(([row["host"]] if row else [])+neighbor_hosts(bssid)))
    def observe_anchor(self,bssid,host):
        if self.client is not None:return self.client.observe_anchor(bssid,host)
        host=str(ipaddress.ip_address(host));identity={"kind":"ftm_anchor","bssid":bssid}
        with sqlite3.connect(self.path,timeout=3) as db:
            db.execute("BEGIN IMMEDIATE")
            row=db.execute("SELECT host,revision,identity FROM routes WHERE device_id=?",("ftm:"+bssid,)).fetchone()
            if row:
                old=json.loads(row[2])
                if old.get("bssid",bssid)!=bssid:raise ValueError("anchor identity changed")
                identity={**old,**identity}
            revision=row[1]+(row[0]!=host) if row else 1
            db.execute("INSERT OR REPLACE INTO routes VALUES(?,?,?,?,?)",("ftm:"+bssid,host,revision,time.time(),json.dumps(identity,sort_keys=True)))
