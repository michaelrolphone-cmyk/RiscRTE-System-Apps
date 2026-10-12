# App-local navigation during shared text input

The selected Watch scene-host manifest supplies display, touch, clock and presentation profile, but no `input.navigation` dependency. Watch's local bridge instead reads completed short/long key events through the already granted PMU API. A neutral short press maps to Back; long and ambiguous events are ignored. This source remains independent of scene touch input.

Restore the predecessor's `PORTABLE_INPUT_NAVIGATION_LOCAL` attention block so shared modal clients preserve this path. Shared navigation profiles never enter this block: their scene host remains the sole navigation consumer. No provider grant, keyboard, renderer or domain behavior is added. Existing Runtime-exit and alarm priority and post-provider liveness fences are preserved.

The exact attention function body is extracted from production source by `scripts/test_text_input_local_navigation.py` and tested in 16 normal/ASan/UBSan combinations of local/shared navigation, policy enabled/disabled, and alarm enabled/disabled. Tests cover neutral suppression, Back, unopened local navigation, provider failure, runtime loss, alarm/exit priority, and no shared-navigation consumption. The separate Points full-host suite uses the actual four-dependency Watch scene configuration and checks its UI, policy and retention workflows.

This correction does not alter touch gesture recognition. Source publication alone does not repair previously compiled clients; each selected component must be rebuilt and qualified explicitly. Physical PMU hardware is not executed by these host tests.
