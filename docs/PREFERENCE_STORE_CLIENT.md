# One preference-provider interface

`PORTABLE_PREFERENCE_STORE` selects `storage.preferences@1` as the shared controller interface. Selection is fixed for the common application build; no application knows the physical backend. Runtime provisioning selects the namespace provider.

The facade has copied-input and terminal-receipt semantics. A provider can finish a submitted operation synchronously. An atomic-group flag documents the stronger all-or-none group guarantee when available; the application does not branch on it. Existing atomic-only builds retain their original interface and strict worker/durability validation.

Captured revisions and expected values remain mandatory. Stale drafts fail without rebasing. Uncertain results are never replayed automatically. The app still keeps accepted tokens until terminal collection and retains custody on malformed grants or broken cleanup.

For OFF operations, the common controller submits the prior usable restore level before the zero level. Atomic providers publish both together. Sequential providers preserve the existing safe write order and may return an uncertain receipt after partial failure. This capability does not manufacture atomic guarantees for such providers. Namespace and key encodings remain unchanged.
