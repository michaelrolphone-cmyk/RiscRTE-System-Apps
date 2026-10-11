# USB transfer 0.1.7: connected-state foreground cadence

This is a focused successor of the USB application 0.1.6 delivered in X4 0.1.55.
The only runtime change is requesting a 1 ms foreground input poll while the
MSC provider reports CONNECTED, instead of 2 ms. All other states retain 2 ms.
The provider's bounded active BOT pump, storage ownership, completion handling,
eject decisions and Runtime cleanup are unchanged.

## Reproduced mechanism and limits

The actual MSC owner pump keeps servicing a command across short packet gaps.
After the successful CSW, command_active clears. A following CBW arriving after
the current poll ends must wait for the foreground application to poll again.
The delivered application requests 2 ms. Native cooperativeDelayTicks maps 1 ms
to one tick on the selected ESP32-S3 Arduino SDK's 1000 Hz scheduler. This change
reduces that foreground gap, while still yielding to other tasks.

The production app and shared adapter were linked with the actual MSC provider,
patched TinyUSB, X4 SD provider and FatFs. The physical DCD/SDMMC, clock,
display, input and host schedule are simulated. The model assigns 65 us to a
full-speed USB packet and sweeps the delay before the host's next CBW. It is not
a Windows trace or a measurement of hardware throughput or power.

Each workload lists 322 real FAT entries (962 long-filename entries) over an
81-cluster root directory, follows FAT links, performs 64 KiB read-ahead and
reads high LBAs 100000 and 131071. Actual START STOP UNIT eject and successful
CSW drive the safe application return. All cases complete 167 commands, 2678
packets and 293 raw SD reads. The full card hash is unchanged, no export-time
writes occur, and remount verifies the file bytes.

| Host inter-command gap (us) | Delivered 0.1.6 (us) | 0.1.7 (us) |
| ---: | ---: | ---: |
| 0 | 177477 | 177477 |
| 50 | 481618 | 337618 |
| 200 | 481618 | 337618 |
| 500 | 481618 | 337618 |
| 1000 | 482653 | 338700 |
| 2000 | 509037 | 511502 |
| 10000 | 1866570 | 1883068 |

The short-gap workload improves by about 30%; longer host waits are dominated
by the host model and vary by under 1%. More frequent foreground polls increase
CPU wakeups during a connected session. No hardware energy claim is made. An
alternative provider prototype that continued pumping after every CSW was
rejected: it regressed the 2 ms host-gap case from 509037 to 725700 us.

The model does not explain the user's minute-scale mount/directory delay.
Useful Windows USB requests/completions and the delivered post-exit USB summary
are still needed to attribute that residual delay. Windows safe removal that
never supplies the expected eject signal remains a separate unresolved issue.
Suspend, reset or deconfiguration alone is not treated as safe eject.

## Reproduction

Run scripts/test_usb_transfer_cadence.py with explicit --reader, --runtime,
--x4 and --tinyusb source roots, and a new --output directory. --sanitized adds
ASan/UBSan. --app-source can select the baseline 0.1.6 application while keeping
the rest of the compiled path identical. The receipt records compiler commands,
all compiled dependency hashes and each case's deterministic counts.

Qualification also runs scripts/test_usb_transfer.py (31 cases per mode) and
scripts/test_resident_system_clients.py --app usb against native 0.1.106
(7 actual Runtime cases per mode). These cover preparation cancellation, owner
cleanup, retained failure, provider-confirmed eject, one Home return and bounded
diagnostic behavior. No physical device is accessed.
