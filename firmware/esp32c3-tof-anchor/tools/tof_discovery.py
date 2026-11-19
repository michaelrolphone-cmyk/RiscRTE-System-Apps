"""Compact FTM peer identity announcements; never ranging measurements."""
import ipaddress
import json
import re

def discovery_records(solution):
    identities={}
    endpoints=set()
    for anchor in solution.get("anchors",[]):
        if anchor.get("received_age_s",float("inf"))>30:
            continue
        try:
            endpoint=str(ipaddress.IPv4Address(anchor.get("ip","")))
            endpoints.add(endpoint)
        except ipaddress.AddressValueError:
            pass
        candidates=[dict(bssid=anchor.get("bssid"),ssid=anchor.get("ssid"),
                         channel=anchor.get("channel"))]
        for peer in anchor.get("ranges",[]):
            if peer.get("seen_age_ms",float("inf"))<=60000:
                candidates.append(dict(bssid=peer.get("target_bssid"),
                                       ssid=peer.get("ssid"),channel=peer.get("channel")))
        for peer in candidates:
            mac=str(peer.get("bssid") or "").lower()
            channel=peer.get("channel");ssid=peer.get("ssid")
            if (not re.fullmatch(r"(?:[0-9a-f]{2}:){5}[0-9a-f]{2}",mac)
                    or not isinstance(ssid,str) or not ssid.startswith("C3-TOF-")
                    or not isinstance(channel,int) or isinstance(channel,bool)
                    or not 1<=channel<=14):
                continue
            identities[mac]=dict(schema="rover.tof.responder/1",bssid=mac,ssid=ssid,
                                 channel=channel,ftm_responder=True)
    return list(identities.values()),sorted(endpoints)

def publish_responders(sock,solution,port):
    records,endpoints=discovery_records(solution)
    sent=errors=0
    for record in records:
        payload=json.dumps(record,separators=(",",":")).encode()
        for endpoint in endpoints:
            try:
                sock.sendto(payload,(endpoint,port));sent+=1
            except OSError:
                errors+=1
    return dict(responders=len(records),endpoints=len(endpoints),sent=sent,errors=errors)
