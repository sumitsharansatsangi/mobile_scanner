# Upstream integration, 2026-11-07

Merged juliansteenbakker/mobile_scanner develop at 676312ed into the fork,
following the existing develop tracking lineage (previous base 7d8936e4).
The merge includes the upstream 7.5.0 release and subsequent lifecycle/web fixes.

Preserved the fork version 8.0.0, publish_to: none, Dart/Flutter constraints,
Git plugin interface, newer Android toolchains, native ZXing ABI v2/postal
formats, format-aware web reader selection, advanced processing options,
and controller registry/resume API. Kept the existing lint baseline to avoid
unrelated source migrations. Did not restore upstream-only release workflows.

Adapted upstream camera session locking to the registry's id/callback API.
Registry handoff stops the previous owner under the already-held lock rather
than reentering the public stop lock. Resume callbacks request a frame so they
run even when the application is idle. Regression coverage exercises handoff,
resume, dispose during start, and stop/pause/start serialization.

Retained the Darwin const-pointer conversion and String slice fixes from
84ac6707 and 95ac90a3. AutoTorch is serialized alongside custom start options;
upstream web teardown generation checks retain fork-specific reader selection.

Validation: flutter analyze reported no issues; the complete VM suite passed
432 tests (35 skipped), and all 70 Chromium browser tests passed. Apple ABI
header equality was verified. The separate CMake native suite could not be
completed because disk space exhausted during the dependency checkout; the
native engine source is unchanged by this upstream integration.
