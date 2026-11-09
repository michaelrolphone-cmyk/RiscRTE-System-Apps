#!/usr/bin/env python3
"""Receive ESP32CAM radio telemetry. JSONL on stdout; no files unless --log is set.

Examples:
  python3 tools/telemetry_receiver.py
  python3 tools/telemetry_receiver.py --view
  python3 tools/telemetry_receiver.py --log radio.jsonl --max-bytes 1048576 --backups 3

UDP is best-effort and unauthenticated. Keep this listener on a trusted LAN.
The receiver preserves device-reported observation age and never turns RSSI
estimates into measured distances. Python 3.8+; standard library only.
"""
import argparse
from collections import OrderedDict
from datetime import datetime, timezone
import json
import logging
from logging.handlers import RotatingFileHandler
import math
import re
import socket
import sys
import time
import unicodedata

MAX_DATAGRAM = 4096
MAC = re.compile(r"^(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}$")


class PacketError(ValueError):
    """A datagram is not a valid supported telemetry message."""


def _integer(value, label, minimum=0, maximum=2**64 - 1):
    if type(value) is not int or not minimum <= value <= maximum:
        raise PacketError("%s must be an integer in range" % label)
    return value


def _number(value, label, minimum=-1e12, maximum=1e12, nullable=False):
    if value is None and nullable:
        return
    if type(value) not in (int, float):
        raise PacketError("%s must be finite" % label)
    if not minimum <= value <= maximum:
        raise PacketError("%s is out of range" % label)
    if isinstance(value, float) and not math.isfinite(value):
        raise PacketError("%s must be finite" % label)


def _text(value, label, maximum=128, empty=False):
    if not isinstance(value, str) or (not value and not empty):
        raise PacketError("%s must be text" % label)
    try:
        length = len(value.encode("utf-8"))
    except UnicodeEncodeError as exc:
        raise PacketError("%s contains invalid Unicode" % label) from exc
    if length > maximum:
        raise PacketError("%s is too long" % label)


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise PacketError("duplicate JSON key")
        result[key] = value
    return result


def _float(value):
    number = float(value)
    if not math.isfinite(number):
        raise PacketError("nonfinite JSON number")
    return number


def _constant(_value):
    raise PacketError("nonstandard JSON number")


def parse_datagram(data):
    """Validate schema 1 without discarding additive firmware metadata."""
    if not data or len(data) > MAX_DATAGRAM:
        raise PacketError("empty or oversized datagram")
    try:
        message = json.loads(data.decode("utf-8"), object_pairs_hook=_unique_object,
                             parse_float=_float, parse_constant=_constant)
        # Reject escaped unpaired surrogates even in additive metadata, before
        # printing or writing UTF-8. Every finite JSON field remains preserved.
        json.dumps(message, ensure_ascii=False, allow_nan=False).encode("utf-8")
    except (UnicodeError, ValueError, RecursionError) as exc:
        raise PacketError("invalid JSON datagram") from exc
    if not isinstance(message, dict) or type(message.get("schema")) is not int or message["schema"] != 1:
        raise PacketError("unsupported telemetry schema")
    _text(message.get("receiver_id"), "receiver_id")
    _integer(message.get("uptime_ms"), "uptime_ms")
    _integer(message.get("sequence"), "sequence")
    if "boot_id" in message:
        _text(message["boot_id"], "boot_id", 64)
    if message.get("timestamp_ms") is not None:
        _integer(message["timestamp_ms"], "timestamp_ms")
    kind = message.get("type")
    if kind == "observation":
        radio = message.get("radio")
        if radio not in ("wifi", "ble"):
            raise PacketError("unknown observation radio")
        item = message.get("observation")
        if not isinstance(item, dict):
            raise PacketError("observation must be an object")
        for key, maximum, empty in (("id", 128, False), ("address", 17, False),
                                    ("name", 256, True)):
            _text(item.get(key), key, maximum, empty)
        if not MAC.fullmatch(item["address"]):
            raise PacketError("invalid transmitter address")
        _number(item.get("rssi_dbm"), "rssi_dbm", -127, 20)
        _number(item.get("rssi_filtered_dbm"), "rssi_filtered_dbm", -127, 20)
        _integer(item.get("age_ms"), "age_ms")
        _integer(item.get("sample_count"), "sample_count", 1)
        if radio == "ble":
            _integer(item.get("address_type"), "address_type", 0, 255)
        if "channel" in item:
            _integer(item["channel"], "channel", 1, 14)
        if "distance_m" in item:
            _number(item["distance_m"], "distance_m", 0, 10000, nullable=True)
        if "distance_method" in item:
            if item["distance_method"] not in ("rssi_assumed", "rssi_calibrated", "wifi_ftm"):
                raise PacketError("unknown distance method")
        if "reference_rssi_1m_dbm" in item:
            _number(item["reference_rssi_1m_dbm"], "reference_rssi_1m_dbm", -127, 20)
        if "path_loss_exponent" in item:
            _number(item["path_loss_exponent"], "path_loss_exponent", 0.1, 10)
        if "calibrated" in item and type(item["calibrated"]) is not bool:
            raise PacketError("calibrated must be boolean")
        if "tx_power_dbm" in item:
            _number(item["tx_power_dbm"], "tx_power_dbm", -127, 127, nullable=True)
    elif kind == "summary":
        for key in ("wifi_count", "ble_count"):
            if key in message:
                _integer(message[key], key, 0)
        if "status" in message and not isinstance(message["status"], dict):
            raise PacketError("summary status must be an object")
        if "ftm" in message:
            ftm = message["ftm"]
            if not isinstance(ftm, dict):
                raise PacketError("FTM report must be an object")
            if "status" in ftm:
                _text(ftm["status"], "FTM status", 80)
            for key, maximum in (("distance_m", 42949672.95), ("rtt_ns", 1e12)):
                if key in ftm:
                    _number(ftm[key], "FTM " + key, 0, maximum, nullable=True)
            if ftm.get("age_ms") is not None:
                _integer(ftm["age_ms"], "FTM age_ms")
    else:
        raise PacketError("unknown message type")
    return message


def received_record(message, peer):
    return {
        "received_at": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
        "source_ip": peer[0],
        "source_port": peer[1],
        "telemetry": message,
    }


def json_line(record):
    return json.dumps(record, ensure_ascii=False, separators=(",", ":"), allow_nan=False)


def make_log(path, max_bytes, backups):
    """The active file and at most backups older files are retained."""
    logger = logging.Logger("radio-telemetry")
    logger.setLevel(logging.INFO)
    handler = RotatingFileHandler(path, maxBytes=max_bytes, backupCount=backups,
                                  encoding="utf-8")
    handler.setFormatter(logging.Formatter("%(message)s"))
    logger.addHandler(handler)
    return logger


def _is_newer_packet(incoming, previous):
    """Order same-boot packets with uint32 serial arithmetic; accept a new boot."""
    if not incoming.get("boot_id") or incoming.get("boot_id") != previous.get("boot_id"):
        return True
    new, old = incoming["sequence"], previous["sequence"]
    if 0 <= new <= 0xffffffff and 0 <= old <= 0xffffffff:
        # Deltas of half the serial space are ambiguous and cannot establish freshness.
        return 0 < ((new - old) & 0xffffffff) < 0x80000000
    return new > old


class LatestObservations:
    """Bounded display cache. Age includes time since receipt; IDs keep provenance."""
    def __init__(self, maximum=1024, ttl=180.0):
        self.maximum = maximum
        self.ttl = ttl
        self.items = OrderedDict()
        self.summaries = OrderedDict()

    def add(self, record, now):
        message = record["telemetry"]
        receiver = (record["source_ip"], message["receiver_id"])
        if message["type"] == "summary":
            previous = self.summaries.get(receiver)
            if previous and not _is_newer_packet(message, previous[0]["telemetry"]):
                return
            self.summaries[receiver] = (record, now)
            self.summaries.move_to_end(receiver)
            while len(self.summaries) > self.maximum:
                self.summaries.popitem(last=False)
            return
        item = message["observation"]
        key = receiver + (message["radio"], item["address"].lower(),
                          item.get("address_type"))
        previous = self.items.get(key)
        if previous and not _is_newer_packet(message, previous[0]["telemetry"]):
            return
        self.items[key] = (record, now)
        self.items.move_to_end(key)
        while len(self.items) > self.maximum:
            self.items.popitem(last=False)

    def rows(self, now):
        rows = []
        for key, (record, arrived) in list(self.items.items()):
            age = record["telemetry"]["observation"]["age_ms"] / 1000.0 + max(0, now - arrived)
            if age > self.ttl:
                del self.items[key]
            else:
                rows.append((record, age))
        for key, (_record, arrived) in list(self.summaries.items()):
            if now - arrived > self.ttl:
                del self.summaries[key]
        return rows


def display_text(value, length):
    clean = "".join(c if c.isprintable() and not unicodedata.category(c).startswith("C")
                    else "?" for c in str(value))
    return clean[:length]


def render_view(cache, now, rejected):
    lines = ["ESP32CAM radio telemetry | stale after %.0fs | rejected packets %d" % (cache.ttl, rejected),
             "RECEIVER / SOURCE IP        RADIO  ADDRESS / TYPE        RSSI   EMA    AGE(s)  RANGE(m) / METHOD         NAME"]
    for record, age in cache.rows(now):
        message = record["telemetry"]
        item = message["observation"]
        receiver = "%s@%s" % (message["receiver_id"], record["source_ip"])
        address = item["address"]
        if message["radio"] == "ble":
            address += "/" + str(item["address_type"])
        distance = item.get("distance_m")
        method = item.get("distance_method", "unavailable")
        ranging = ("--" if distance is None else "%.2f" % distance) + " / " + method
        lines.append("%-27s %-6s %-21s %5.0f %6.1f %7.1f  %-25s %s" %
                     (display_text(receiver, 27), message["radio"],
                      display_text(address, 21), item["rssi_dbm"],
                      item["rssi_filtered_dbm"], age, ranging,
                      display_text(item["name"], 24)))
    if len(lines) == 2:
        lines.append("Waiting for fresh device observations.")
    for record, arrived in cache.summaries.values():
        message = record["telemetry"]
        ftm = message.get("ftm", {})
        if ftm and ftm.get("status") not in (None, "idle", "unsupported"):
            age = ftm.get("age_ms")
            age_text = "--" if age is None else "%.1fs" % (age / 1000.0 + max(0, now - arrived))
            distance = ftm.get("distance_m")
            dist_text = "--" if distance is None else "%.2fm" % distance
            lines.append("FTM %s@%s: %s, %s, age %s" %
                         (display_text(message["receiver_id"], 64),
                          record["source_ip"], display_text(ftm.get("status"), 80),
                          dist_text, age_text))
    lines.append("RSSI ranges are approximate. FTM requires a compatible responding AP. Ctrl-C to stop.")
    return "\n".join(lines)


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--bind", default="0.0.0.0", help="local IPv4 address (default: all interfaces)")
    parser.add_argument("--port", type=int, default=9876)
    parser.add_argument("--view", action="store_true", help="show latest devices instead of JSONL stdout")
    parser.add_argument("--ttl", type=float, default=180, help="hide observations older than this many seconds")
    parser.add_argument("--max-devices", type=int, default=1024, help="bounded display cache size")
    parser.add_argument("--log", help="optional rotating JSONL file; no files are written by default")
    parser.add_argument("--max-bytes", type=int, default=1048576, help="maximum size per log file, minimum 8192")
    parser.add_argument("--backups", type=int, default=3, help="number of rotated files, 1 to 100")
    args = parser.parse_args(argv)
    if not 1 <= args.port <= 65535:
        parser.error("--port must be 1 to 65535")
    if not math.isfinite(args.ttl) or not 1 <= args.ttl <= 86400:
        parser.error("--ttl must be 1 to 86400 seconds")
    if not 1 <= args.max_devices <= 100000:
        parser.error("--max-devices must be 1 to 100000")
    if args.max_bytes < 8192 or not 1 <= args.backups <= 100:
        parser.error("--max-bytes must be at least 8192; --backups must be 1 to 100")
    return args


def main(argv=None):
    args = arguments(argv)
    logger = None
    cache = LatestObservations(args.max_devices, args.ttl) if args.view else None
    rejected = 0
    last_view = 0.0
    last_warning = -10.0
    try:
        if args.log:
            logger = make_log(args.log, args.max_bytes, args.backups)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.bind((args.bind, args.port))
            sock.settimeout(0.5)
            print("Listening on %s:%d; %s" % (args.bind, args.port,
                  "rotating log enabled" if logger else "no disk logging"), file=sys.stderr, flush=True)
            while True:
                now = time.monotonic()
                if cache is not None and now - last_view >= 1:
                    prefix = "\x1b[2J\x1b[H" if sys.stdout.isatty() else ""
                    print(prefix + render_view(cache, now, rejected), flush=True)
                    last_view = now
                try:
                    data, peer = sock.recvfrom(MAX_DATAGRAM + 1)
                except socket.timeout:
                    continue
                try:
                    message = parse_datagram(data)
                except PacketError as exc:
                    rejected += 1
                    if now - last_warning >= 5:
                        print("Rejected telemetry from %s (%d total): %s" %
                              (peer[0], rejected, exc), file=sys.stderr, flush=True)
                        last_warning = now
                    continue
                record = received_record(message, peer)
                line = json_line(record)
                if logger:
                    logger.info(line)
                if cache is not None:
                    cache.add(record, time.monotonic())
                else:
                    print(line, flush=True)
    except (KeyboardInterrupt, BrokenPipeError):
        return 0
    except OSError as exc:
        print("Receiver error: %s" % exc, file=sys.stderr)
        return 1
    finally:
        if logger:
            for handler in logger.handlers:
                handler.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
