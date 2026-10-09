# Selected Watch Settings hour display

`PORTABLE_UNPADDED_HOURS` displays 24-hour Settings rows and wall labels as
`0`, `1`, ... `23` without a leading zero. Stored hours, minutes, RTC, timezone,
12-hour formatting and the flag-off profile are unchanged.

The bounded `System.bundle` preserves exact tested source
`df1d7d1b248de13f773690139750ad2f963967f3` over alarm source
`e2b2b8b7d903acb6aec6b07ebee7d037de2babb1`. Its tree is
`9250000672f989be18f2ef45d48c257980c18604`. Public code commit
`a290ca19c56e472989a54db0ddfe9580c9ff792c` has the same production and test
bytes plus the existing source custody files. `manifest.json` fixes both
identities and the bundle digest.

Run `python scripts/verify_watch_settings_hours_source.py` in a full public
clone. It imports the three bounded bundles into a fresh repository, checks
the exact original commit/tree/parent and verifies the public code mapping.

`python scripts/test_unpadded_hours.py` runs 280 actual Settings scenarios
normally and under ASan/UBSan: raw RTC, Denver, navigation and Nova profiles,
all 24 row hours and explicit midnight/1/9/10/noon/23 wall labels. The existing
alarm target parity check also passes four flag-off byte comparisons and four
selected compilations after this change. Dedicated CI repeats these checks.
