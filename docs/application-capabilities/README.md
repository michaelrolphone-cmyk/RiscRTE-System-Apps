# Shared application capability contracts

Definition work for Rhizome task a60738b8-3a44-4502-af30-6e8ad56948be (13 points).

Read CONTRACT-RECONCILIATION.md for the existing API/ownership map and required versus optional service semantics. CANVAS-BOUNDARY-DESIGN.md defines the additive missing custom-surface boundary; proposed/RiscCanvasV1.h is a **candidate definition**, not an installed provider. No existing ABI is replaced.

The source-bound inventory covers 57 capability/API pairs actually declared by applications across four products. Declaration is not evidence that every optional path ran. Existing headless Alarms, semantic scenes, page/telemetry, lifecycle, storage and device contracts are reused; CanvasApp engines and features are preserved as the implementation reuse baseline.

Run definition/admission checks from the repository root:

```
python3 docs/application-capabilities/qualify_contract_examples.py --system . --output dist/application-capability-contracts
```

This runs 38 specification-model cases and 20 existing optional-scene admission cases, normal plus ASan/UBSan. These are **not** production canvas, target-compiler, renderer performance or physical-device acceptance. Independent review caught and corrected retained receipt/lease revision model errors; source and review receipts preserve the final evidence.

## Completion mapping

- Versioned optional UI/input/device/lifecycle: reuse source map and actual manifest API/version bindings.
- Required/optional and absence/error: operation-level semantics, explicit unknown acquisition reason, no fabricated success.
- GUI-free lifecycle: reuse delivered Alarms control/Runtime ownership, no UI dependency added.
- Provider-owned presentation/resources: canvas lease/receipt/input/geometry/handoff definition and examples; app describes content, never backend/device identity.
- Preserve custom rendering: retain full engines, MONO1/color content and multiple contacts; semantic pages are not forced onto color engines.

This task defines contracts. Implementing the canvas provider/adapters and each app migration remains separate existing convergence work; a passed model must not close those implementation tasks. Retained product/hardware uncertainty stays explicit.
