# Garden relay boot-store source coverage

This additive inventory closes one explicitly uninspected source snapshot from the [October 22 report](../inventory-20261022/README.md). It does not replace that report or complete the all-applications storage migration.

## Immutable evidence

- Repository: michaelrolphone-cmyk/Garden-Controller.
- Source: `2b01d37ac5a6b643b8bc25758a07787587d7ca7e`, observed as `riscrte/relay-boot-store`.
- Git tree: `aac0d57088e6cd7fcaf340db747b15bd12e3f131`.
- [Manifest](sources.json), [hash-bound evidence index](evidence/index.json), and [per-file evidence](evidence/michaelrolphone-cmyk--Garden-Controller.json).
- 179 tracked entries; 149 source/configuration/tool files scanned; 30 non-source-extension files excluded; 23 candidate-bearing files. These are not 23 consumers or defects.

The source is distinct from the previously scanned Garden product commit `2b27fec49c2bb33914f53821f2a530578d5578cc`. The branches diverge, so this report is not a replacement for or a simple migration delta from that snapshot. Equal aggregate counts do not imply equal source coverage. The original manifest, hashes and report remain unchanged.

## Contextual findings

1. The selected [boot manifest](https://github.com/michaelrolphone-cmyk/Garden-Controller/blob/2b01d37ac5a6b643b8bc25758a07787587d7ca7e/riscrte/Apps/garden-relay-boot.json) requires only switch.relay and sound.buzzer. The [boot app](https://github.com/michaelrolphone-cmyk/Garden-Controller/blob/2b01d37ac5a6b643b8bc25758a07787587d7ca7e/riscrte/Apps/garden-relay.c#L28-L54) acquires those grants, validates them and yields. It does not receive a lexical storage hit. This is bounded source evidence, not proof of absent transitive storage dependencies or of a deployed product.
2. [build_relay_deployment.py](https://github.com/michaelrolphone-cmyk/Garden-Controller/blob/2b01d37ac5a6b643b8bc25758a07787587d7ca7e/scripts/build_relay_deployment.py) selects only relay and buzzer provider instances and assembles an eight-file boot store. [build_relay_store.py](https://github.com/michaelrolphone-cmyk/Garden-Controller/blob/2b01d37ac5a6b643b8bc25758a07787587d7ca7e/scripts/build_relay_store.py) constructs and round-trip compares a SPIFFS image on the build host. Their physical/storage markers are host-tooling evidence, not an application storage migration defect. None of these build scripts was executed for this inventory.
3. The repository retains a [storage provider](https://github.com/michaelrolphone-cmyk/Garden-Controller/blob/2b01d37ac5a6b643b8bc25758a07787587d7ca7e/riscrte/Drivers/storage/driver.c#L724-L732) and its legacy storage.volume contract. Presence in the source tree does not establish inclusion in the relay boot store. It remains provider evidence, distinct from migrated application behavior.
4. Hits in garden-encoder.c and garden_controller.c occur on their T5AppApi includes. An API include is a review lead, not sufficient evidence that the application performs storage I/O. The boot-store installation document explicitly excludes the encoder and full garden controller from this store.
5. [The installation contract](https://github.com/michaelrolphone-cmyk/Garden-Controller/blob/2b01d37ac5a6b643b8bc25758a07787587d7ca7e/docs/RELAY_INSTALL.md) pins historical Runtime `e27d3d089086d79f06edeff4c2bd35f6e6243444`, an 8 MiB layout and a flat SPIFFS boot store. It is contextual documentation, not part of the scanner's source-suffix coverage and not current-product admission. No firmware, image, partition, relay or physical-device action is performed or qualified here.

## Reproduction and checks

Use the unchanged immutable-object scanner with a Garden checkout containing the pinned commit:

```sh
python scripts/audit_storage_refs.py --manifest docs/storage/inventory-20261023-garden/sources.json --checkout-root /path/to/repositories --output-dir /tmp/garden-report
diff -ru docs/storage/inventory-20261023-garden/evidence /tmp/garden-report
python -m unittest discover -s tests -p 'test_storage*inventory.py' -v
python -m unittest discover -s tests -v
```

The existing read-only inventory workflow now retrieves that exact Garden commit and byte-compares the reproduced reports. The new tests check manifest/report hashes, coverage accounting, source links, host/provider role separation and preservation of the historical Garden snapshot. Hosted results must be checked against the eventual published head; local preparation alone is not delivered qualification.

## Remaining scope

Garden main, release/0.1.0-BETA, other uninspected product/release commits, CrossPoint's nested Free-Ink SDK, and contextual adjudication across other applications remain open. No absence-of-hits result establishes migration completion. This increment changes only inventory evidence, tests and its read-only workflow; it preserves production code, SDK pins, release assets, existing hardware holds and the all-applications acceptance criterion.
