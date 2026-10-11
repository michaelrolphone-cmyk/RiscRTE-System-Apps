# Existing Watch user files

This work maps existing files; a new `documents.ruf` tree is optional scratch
storage and is not a replacement for existing Watch data.

Exact inspected product: Watch `089868395268b696c05651c33816345bfe0425df`,
full 25-app composition. Utilities source
`814502e51125d799a7ca625b78707753ca12d4e5` supplies the event-bank and
Model Viewer filenames. Lists uses its existing `lists.bin` AppData record.

| Files path | Existing owner / namespace | Existing record | Access |
|---|---|---|---|
| `/timecard/timecard.json` | timecard / 1 | timecard.json | read, revision-checked replace |
| `/audio-spectrum/spectrum-events-a.sqt` | audio_spectrum / 2 | spectrum-events-a.sqt | read, revision-checked replace |
| `/audio-spectrum/spectrum-events-b.sqt` | audio_spectrum / 2 | spectrum-events-b.sqt | read, revision-checked replace |
| `/waterfall/rf-events-a.rft` | waterfall / 3 | rf-events-a.rft | read, revision-checked replace |
| `/waterfall/rf-events-b.rft` | waterfall / 3 | rf-events-b.rft | read, revision-checked replace |
| `/lists/lists.bin` | lists / 62 | lists.bin | read, revision-checked replace |
| `/models/model.obj` | model_viewer / 64 | model.obj | read, revision-checked replace |
| `/models/model.stl` | model_viewer / 64 | model.stl | read, revision-checked replace |

These are policy mappings, not a claim that every file is present on a device.
Missing records remain missing until explicitly created at the configured path.
Current AppData limits are 64 KiB per file and four files / 128 KiB per namespace.
The export interface does not advertise remove, rename or mkdir: those require
an additive Runtime storage change and cannot be simulated by deleting or
renaming unrelated native files. Models are already consumed by Watch Model
Viewer's Saved OBJ / Saved STL choices. Arbitrary Watch `file.open` launch is
not provided by that application profile; X4 uses declared SD associations.

Credentials, BLE bonds, installation journals, internal firmware metadata,
crash reports and Contexts' internal fingerprints are excluded. Existing
key-value settings and alarm records are not files and are not represented as
invented files. No namespace enumeration or blanket native filesystem access
is granted. The composition script verifies each owner and namespace against
the selected boot policy before applying this map.
