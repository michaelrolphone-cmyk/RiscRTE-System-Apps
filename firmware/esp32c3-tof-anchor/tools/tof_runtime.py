"""Same-host ToF ownership and bounded request custody.

The lock excludes cooperating solver processes sharing this state directory.
Native-to-container migration still stops and disables the previous native
process before activation; this is not a distributed failover lease.
"""
import fcntl
import json
import math
import os
from pathlib import Path
import stat
import time


REQUEST_SETTLE_SECONDS = 15.
TERMINAL_FTM_STATES = (
    "success", "unsupported", "configuration_rejected", "no_response", "failed", "start_failed",
)


class OwnerLock:
    def __init__(self, path):
        self.path = Path(path)
        self.handle = None

    def acquire(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        flags = os.O_RDWR | os.O_CREAT | getattr(os, "O_NOFOLLOW", 0)
        fd = os.open(str(self.path), flags, 0o600)
        try:
            if not stat.S_ISREG(os.fstat(fd).st_mode):
                raise ValueError("ToF owner lock must be a regular file")
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.handle = os.fdopen(fd, "r+")
        except BaseException:
            os.close(fd)
            raise
        self.handle.seek(0)
        self.handle.truncate()
        json.dump(dict(schema="biomerte.tof-local-owner/1",
                       pid=os.getpid(), acquired_at=time.time()), self.handle)
        self.handle.flush()
        return self

    def close(self):
        if self.handle is not None:
            fcntl.flock(self.handle.fileno(), fcntl.LOCK_UN)
            self.handle.close()
            self.handle = None

    def __enter__(self):
        return self.acquire()

    def __exit__(self, *args):
        self.close()


class RoverRequest:
    """Retain uncertain requests across solver/VM restarts; reconcile by device."""
    def __init__(self, state_path=None, legacy_path=None, *, clock=None, wall=None):
        self.boot_id = None
        self.uptime_ms = None
        self.pending = None
        self.started = 0.
        self.path = Path(state_path) if state_path is not None else None
        self.clock = clock or time.monotonic
        self.wall = wall or time.time
        if self.path is not None and self.path.exists():
            saved = json.loads(self.path.read_text())
            if saved.get("schema") != "biomerte.tof-request-custody/1":
                raise ValueError("Invalid ToF request custody")
            self.boot_id = saved.get("camera_boot_id")
            self.uptime_ms = saved.get("camera_uptime_ms")
            self.pending = saved.get("pending_target")
            if self.pending is not None:
                if not isinstance(self.pending, str) or not self.pending:
                    raise ValueError("Invalid pending ToF target")
                started = saved.get("started_at")
                if type(started) not in (int, float) or not math.isfinite(started):
                    raise ValueError("Missing pending ToF request time")
                self.started = self.clock() - max(0., self.wall() - started)
        elif self.path is not None and legacy_path is not None and Path(legacy_path).is_file():
            saved = json.loads(Path(legacy_path).read_text())
            self.boot_id = saved.get("camera_boot_id")
            target = saved.get("pending_target")
            if target:
                if not isinstance(target, str):
                    raise ValueError("Invalid legacy ToF target")
                self.pending = target.lower()
                age = saved.get("request_age_s", {}).get(target, 0.)
                updated = saved.get("updated_at", self.wall())
                self.started = self.clock() - max(0., self.wall() - updated + age)
                self._save()

    def _save(self):
        if self.path is None:
            return
        self.path.parent.mkdir(parents=True, exist_ok=True)
        value = dict(schema="biomerte.tof-request-custody/1",
                     camera_boot_id=self.boot_id, camera_uptime_ms=self.uptime_ms,
                     pending_target=self.pending,
                     started_at=None if self.pending is None else
                     self.wall() - (self.clock() - self.started))
        temporary = self.path.with_name(self.path.name + ".tmp")
        with temporary.open("w") as target:
            json.dump(value, target, allow_nan=False, separators=(",", ":"))
            target.flush()
            os.fsync(target.fileno())
        os.replace(temporary, self.path)

    def observe(self, snapshot, observed=None):
        before = self.boot_id, self.pending
        ftm = snapshot.get("ftm", {})
        boot = snapshot.get("boot_id")
        uptime = snapshot.get("uptime_ms")
        rebooted = ((boot is not None and self.boot_id is not None and boot != self.boot_id)
                    or (isinstance(uptime, (int, float)) and isinstance(self.uptime_ms, (int, float))
                        and uptime < self.uptime_ms))
        if boot is not None:
            self.boot_id = boot
        if isinstance(uptime, (int, float)):
            self.uptime_ms = uptime
        idle = not ftm.get("busy") and not ftm.get("retiring")
        reset = idle and ftm.get("status") == "never_requested" and not ftm.get("target")
        completed = idle and self.pending and str(ftm.get("target") or "").lower() == self.pending
        # An uncertain POST can leave the camera's previous terminal report in
        # place. Fresh same-boot evidence that the radio is idle reopens the
        # slot after the settle interval without treating that report as ours.
        settled_idle = (
            self.pending is not None and isinstance(boot, str) and boot and boot == before[0]
            and ftm.get("busy") is False and ftm.get("retiring") is False
            and ftm.get("status") in TERMINAL_FTM_STATES
            and type(observed) in (int, float) and observed >= self.started + REQUEST_SETTLE_SECONDS
            and 0 <= self.clock() - observed < 2
        )
        if rebooted or reset or completed or settled_idle:
            self.pending = None
        # One bounded state record, written for custody/boot changes only.
        if before != (self.boot_id, self.pending):
            self._save()
        return rebooted

    def submitted(self, address, now):
        # Call before the HTTP POST: timeout or termination must not imply
        # that the device did not receive the command.
        self.pending = address.lower()
        self.started = now
        self._save()
