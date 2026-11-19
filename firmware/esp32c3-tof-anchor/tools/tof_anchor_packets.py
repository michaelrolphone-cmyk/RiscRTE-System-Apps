"""Decode complete anchor reports, including legacy Arduino UDP writes split at 1460 bytes."""
import json


class AnchorReports:
    def __init__(self):
        self.partial = {}
        self.last = {}

    def receive(self, peer, data, now):
        # Anchors publish every two seconds. Continuations of one WiFiUDP write
        # belong to the same burst; never carry an incomplete report into the next one.
        for source, (started, _) in tuple(self.partial.items()):
            if now - started >= 1:
                del self.partial[source]
        starts_report = data.startswith(b'{"schema":"rover.tof.anchor/1"')
        if starts_report:
            self.partial.pop(peer, None)
            started, body = now, bytearray(data)
        elif peer in self.partial:
            started, body = self.partial.pop(peer)
            body.extend(data)
        elif data.lstrip().startswith(b'{'):
            started, body = now, bytearray(data)
        else:
            return None
        try:
            report = json.loads(body)
        except (ValueError, UnicodeDecodeError):
            # WiFiUDP flushes a full internal block into its own datagram.
            # A short malformed final block is discarded.
            if len(data) == 1460 and (starts_report or len(body) > len(data)):
                self.partial[peer] = started, body
            return None
        if (not isinstance(report, dict) or report.get("schema")!="rover.tof.anchor/1"
                or not isinstance(report.get("bssid"),str)
                or type(report.get("seq")) is not int or "boot_id" not in report):
            return None
        identity = report["boot_id"], report["seq"]
        if self.last.get(peer) == identity:
            return None
        self.last[peer] = identity
        return report


from tof_anchor_transport import AnchorHTTPFallback


class PushAnchorRegistry(AnchorHTTPFallback):
    """Reuse the anchor identity registry without starting a telemetry poller."""
    def note_udp(self, packet, ip, received):
        if packet.get("schema") != "rover.tof.anchor/1" or not packet.get("ftm_responder"):
            return
        mac = str(packet.get("bssid","")).lower()
        with self.lock:
            changed = mac not in self.last_udp or self.anchors.get(mac) != ip
        super().note_udp(packet, ip, received)
        routes = getattr(self, "routes", None)
        if changed and routes is not None:
            routes.observe_anchor(mac, ip)

    def status(self):
        now = self.clock()
        with self.lock:
            return {
                "mode":"udp_push","registered":len(self.anchors),
                "anchors":[{
                    "bssid":mac, "ip":ip,
                    "transport":"udp" if mac in self.last_udp and now-self.last_udp[mac]<6 else "waiting_for_udp",
                    "last_udp_age_s":None if mac not in self.last_udp else now-self.last_udp[mac]
                } for mac,ip in sorted(self.anchors.items())]
            }
