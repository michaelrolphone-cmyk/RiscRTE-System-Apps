# Portable Settings sleep choice 1.1.0

## Optional persisted sleep choice

`--sleep-settings` enables the bounded Light/Deep chooser through an explicit
storage.key-value@1 namespace-1 grant. The client uses PortableSleepPolicy.h;
mode bytes and safe Light defaults remain app policy. Missing/invalid/error
storage never silently selects Deep. Save verifies the record; a failed save is
reported as unconfirmed because persistence may already have changed. The
interface is a newly versioned generic runtime contract, not the old Reader
firmware-owned CrossPointSettings bridge or removable-media filesystem API.
Deep wake restarts Clock; no ULP-coprocessor or measured power claim is made.
