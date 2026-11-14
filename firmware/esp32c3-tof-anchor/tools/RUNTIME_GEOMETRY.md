# Runtime measured anchor geometry

The collector reads `geometry-config.json` beside `baseline.json` every update.
The default with no enabled configured anchors preserves joint radio self-localization.
A configured anchor is a measured fixed position, never an optimizer variable.
Unconfigured radio modules remain estimates and are included only when their measured
links support a unique, consistent 3D lateration. Missing links are not synthesized.

Configuration schema `rover.tof.geometry-config/1`:

- `frame_id`: user chosen coordinate frame name.
- `axes`: `east_north_up`; x=east, y=north, z=up.
- `units`: `in` or `m`, shared by all positions.
- `anchors`: any number of records with unique `bssid`, optional `label`,
  `position: [east, north, up]`, and optional boolean `enabled`.

Keep horizontal origin and floor-height datum explicit. Ground offsets do not replace
mount heights. Module labels are descriptive; identity is the actual responder BSSID.

Use the existing Biome terminal capability on the collector host:
```sh
python3 tof_config.py --state /path/to/state --get
python3 tof_config.py --state /path/to/state --replace /path/to/new.json --expected-revision CURRENT_SHA256
```
For the first configuration omit `--expected-revision`. Subsequent writes are
compare-and-swap and atomic. Read the latest revision, edit the complete document,
then replace it. Add, move, disable, rename or remove modules through data alone.
No code redeployment or collector restart is needed. An empty anchors list returns
to self-localization. Removing the configuration file also clears measured constraints.
Invalid configuration exposes an explicit error and suppresses the old position output;
fixing the file restores live solving. Revision/frame identity changes invalidate old
rover refinement rather than mixing coordinates from different configurations.

Baseline output includes `geometry_configuration`, `fixed_anchors`, per-position
`position_source` and labels. Radio residuals remain diagnostics, including disagreement
between fixed anchors. They never move tape measured positions. Estimated positions
still require observability and the existing range consistency gates. This does not
calibrate RF bias, grant navigation authority or register a SLAM map to the room.
