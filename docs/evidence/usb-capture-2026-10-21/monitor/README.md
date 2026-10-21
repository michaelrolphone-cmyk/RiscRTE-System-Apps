# GitHub source and evidence capture

Source and test records are preserved as an unfinished candidate, not a firmware
release or hardware acceptance. See CAPTURE.json for exact local-to-GitHub
commit/tree mappings, evidence hashes and qualification limits. The source commit
has exactly the original tree. This separate evidence commit adds only this
directory. Historical reports are retained without rewriting their test claims.

## Restore original commits

Start from a clone of this repository containing upstream commit
0651a55513b0aa0dff1b6a9072303def08a97378. Download the client incremental
bundle from the client capture branch's evidence directory first. Then run:

```sh
git fetch /path/to/portable-serial-client.bundle refs/heads/tasker/portable-serial-client-20261021:refs/remotes/recovery/serial-client
```

This restores original f9d55dbeb941ba40fb965f53172ce3e1b85af376.
For the monitor, fetch its bundle after the client:

```sh
git fetch /path/to/native-serial-monitor-ui.bundle refs/heads/tasker/native-serial-monitor-ui-20261021:refs/remotes/recovery/serial-monitor
```

This restores original 5843d1bf2079e66f837aec5eb03a1f15b9a95ae7.
For the mouse checkpoint, fetch its bundle after the client:

```sh
git fetch /path/to/mouse-consumer-incremental.bundle HEAD:refs/remotes/recovery/mouse-consumer
```

This restores original b643fdfe7e36a5a007603c34e24f7953ee90b072.
Incremental bundles require their stated base. They do not need a fabricated
shallow boundary. Patches are also included as an alternative; use their exact
recorded base. Do not treat source capture as qualification, merge or deployment.

## Qualification status

Previously recorded software fixtures and independent review only. Unmanifested, receive-only, no target build or product/hardware acceptance.
