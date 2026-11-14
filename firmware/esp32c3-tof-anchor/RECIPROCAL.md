# Reciprocal rover FTM

The rover preserves its station connection and adds a password-protected FTM
responder AP on the station channel. The C3-TOF-ROVER- SSID prefix allows the
deployed C3 1.0.3 scanners to discover it without updating anchor firmware.
FTM does not require AP association. The random per-boot AP password is not
published. The AP-only IotWebConf provisioning path is not used.

GET /api/telemetry exposes ftm_responder with active state, actual AP BSSID,
station-connected state, channel and error. Configuration accepts the Boolean
ftm_responder_enabled (default true). Disabling responder support keeps
network interfaces up. Reconnection restores responder support without
cancelling another consumer's initiating session.

The collector reports rover_directional_ranges and one rover_ranges constraint
per anchor. Both directions retain original timestamps, source boot, report
sequence, sample counts and RTT. With <=2s skew, agreeing reports are combined
with a median. Disagreement above max(1m,25% of median) is flagged and excluded
from localization. Wider skew uses the newest direction with agreement unknown.
This is not motion compensation or a calibrated accuracy guarantee.

Reverse ranges to the mobile rover are excluded from fixed-anchor refinement.
Original expiry and reboot boundaries remain enforced. No simultaneous
transmit/receive or additional physical radio is claimed.

## Anchor transport compatibility

Existing C3 1.0.3 WiFiUDP writers split JSON above 1460 bytes into separate
datagrams. A fifth peer can cross that boundary. The collector keeps UDP as
the preferred source and recovers complete /api/status responses in parallel
when no complete UDP report has arrived for six seconds. Endpoints are saved
from observed UDP peers or explicitly verified known devices. HTTP identity
must match the expected BSSID. Per-anchor buffers keep only the latest report;
transport changes do not refresh measurement timestamps or session identities.
No anchor reflash is needed for this compatibility path.
