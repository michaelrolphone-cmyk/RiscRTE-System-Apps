# Shared Home capability and identity projection

One complete `default.elf` contains both current semantic controllers. The
canonical package manifest is `Apps/shared_home.json`. Product upgrades retain
the installed application ID, file route, catalog metadata and namespace
bindings; only version, capability declarations and the complete ELF change.
The canonical new-install ID is not permission to rename an existing app or
discard its saved data.

The five required declarations are display output (brightness/tone control),
native realtime control, provider promotion, immutable Home presentation
policy, and input-free `ui.canvas.raster`. Missing core presentation is a
diagnosed/refused interactive application, not an invented headless UI.

Foreground `ui.canvas-input` is optional at admission. Pure retained TIMER
rendering never acquires it. Interactive startup may refuse cleanly without
it. Existing Viewer continues to use its separately selected `ui.canvas`
provider. Two global providers with the same capability would be ambiguous;
the raster provider has a distinct capability and module identity.

The other optional declarations cover existing storage, radio, battery,
motion, power, civil time, alarms and child-app features. Optional does not
mean a malformed/retained callback can be ignored: loss of custody fences all
later I/O. Exact absence/degradation cases are qualified against owning
controllers, not inferred from manifest labels.

The complete contract has 28 distinct declarations and needs the existing
32-row Runtime requirement configuration. Multiple KV grants retain original
namespaces 1 (settings), 3 (countdown), 5 (Points), and 6 (Wi-Fi policy) where
already authorized. Crash storage keeps its provider-declared existing
namespace; zero means unavailable, not an app-data fallback. Product authority
must be derived from existing policies, not synthesized from this document.

The additive `alarm.service.facade@2` is the single Home alarm declaration; the common native and
overview clients must both use the same qualified contract. The facade forwards to each existing scheduler owner, preserving original
API1/API2 consumers and storage/RTC/retry policy without a second scheduler.
Runtime's exact API matching does not permit pretending API1 and API2 are
interchangeable.
Delivery remains in progress until the complete target and cumulative
three-product admission qualify.
