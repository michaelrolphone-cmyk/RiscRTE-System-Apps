# Optional exact-source firmware routes

`build_portable_updates.py --source-routes` selects firmware provider0.1.5.
It adds no app capability and leaves the firmware provider's three dependencies
unchanged. Flag-off builds and the App Store provider retain their selected
versions and behavior. X4 still requires its explicit product/repository policy;
an unconfigured feed returns an empty list without network or bank access.

The optional schema1 top-level `firmware_routes` array contains up to16 objects.
Each object has exactly `from` and `firmware`. `from` has exactly these fields:
`product`, `version`, `source_repo`, `source_revision`, `runtime_version`,
`layout`, `store_abi`, and `active_store_sha256`. Every string is bounded; versions are canonical,
source revisions are40 lowercase hex digits, store hashes are64 lowercase hex digits, and product/repository must match
the compiled product policy. There are no wildcard or range selectors.

`firmware` is the existing complete paired-cohort firmware record. It must keep
the same layout/ABI, advance the product version and avoid a Runtime downgrade.
All routes, including unmatched routes, must parse and obey these bounds.
Matching uses the currently installed cohort plus native bank status. Exactly
one matching route supplies the single existing UI row. No match gives an empty
list; multiple matches or malformed metadata fail the check. The exact installed
source and active store digest are checked again when the user begins the selected update. A changed
source rejects the operation before opening the payload or staging a bank.

Catalogs still require the legacy `firmware` member and preserve its syntax.
Clients built with route selection use the routes whenever the routes array is
present, including an explicitly empty array; they do not fall back to an
unmatched universal offer. Clients without this opt-in ignore the additive key.
Use `firmware: null` when no single legacy offer is safe. An individual App Store update changes the store digest, so the old exact route
no longer matches even when cohort metadata remains the same. Source selection is
compatibility filtering only: native bank admission, identity/hash validation,
rollback and persistent namespace checks remain mandatory and unchanged.

Installed Watch12–15 cannot learn this protocol from a catalog. Their qualified
source-specific bootstrap offers still require a staged deployment before a
new provider can use automatic routing. This change does not publish a catalog,
release, payload or product build and does not bypass that bootstrap limitation.

`test_update_source_routes.py` exercises the actual provider for Watch, configured
X4 and unconfigured X4 in normal and ASan/UBSan modes. It covers exact selection,
no match, duplicate matches, missing/unknown/malformed selectors, invalid targets,
16/17-route bounds, changed source before begin, old API prefixes, unterminated
native strings, HTTP close refusal and fallback refresh. Transport/native-bank
functions are explicit test doubles; physical network, flash and hardware are
not qualified by this test.

The runner selects the available compiled product policies. This public Watch
base runs72 cases. Applying the same focused provider/header delta to the X4
policy source runs216 cases including configured and unconfigured X4; those
profiles do not become part of this Watch-base publication by implication.
