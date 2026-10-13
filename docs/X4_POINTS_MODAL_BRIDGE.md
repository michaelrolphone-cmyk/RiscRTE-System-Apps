# X4 Points component bridge

This branch preserves System 5b28172e979b5f008da19e9a00a3b98e35937974,
the adapter that reproduces the deployed X4 Points 0.6.17 ELF. It adds the
opt-in text checkpoint ABI/client/policy bridge from System
fd4b1e52fe2a49c3c4c22f9ccd4c98a91f88afa0. The older ABI prefixes are unchanged.
The host control header comes from the selected Runtime SDK.

Only PORTABLE_TEXT_INPUT_POLICY enables the added adapter code. A read-only
exit query distinguishes a resident exit from alarm attention after the modal
session settles; the app can close and return without repainting or requesting
a different launch destination. It never clears Runtime's pending exit.

The component intentionally retains the deployed Points gesture behavior.
Taking the complete newer adapter made a rejected controls gesture remain
pending and block policy in the existing unsaved-draft regression. The X4
Points source, renderer, grants and resident architecture are independently
preserved by the Productivity qualification. No Home, native Runtime, shared
text-host binary, RF, OBDTest, settings or SD component is selected by this branch.

This is a component build checkpoint, not a proposal to replace current System
main with its historical ancestor. Qualification and source/target receipts are
owned by the paired Productivity X4 Points modal-policy branch. Physical device
testing, merging and flashing are separate actions.
