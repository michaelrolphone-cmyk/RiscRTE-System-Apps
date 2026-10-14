# T063 launcher startup recovery

A failed initial native app catalog refresh left Springboard permanently empty: initialized was set and no launch could trigger the existing changed-catalog refresh. Retry unavailable publications at most once per second. Successful recovery resets selection and gesture state before rendering. Terminal custody failures stop the bridge; permanent unavailability remains bounded and Back remains available.

The actual production controller and adapter pass 164 cases, including recovery, persistent failure, terminal retention during retry, inventory bounds, stale launch fencing and byte-identical prior 23-entry pixels at 240x240 and 410x502 under ASan/UBSan. See catalog-regressions.log.

Published Waveshare .35/.36 already grant app.catalog v1. Native catalog implementation and ABI match. These changes establish the recovery defect, not the physical trigger for the first failed publication. A diagnostic now distinguishes acquisition/interface failure from publication failure. No hardware test claimed. No native or Watch firmware change.
