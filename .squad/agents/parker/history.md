# Project Context

- **Owner:** Dan Paulat
- **Project:** supercell-wx
- **Product:** Cross-platform application for visualizing live and archived NEXRAD weather radar data
- **Stack:** C++20, Qt6, CMake, Conan 2, GoogleTest, Python build helpers
- **Created:** 2026-04-04

## Learnings

- Team initialized on 2026-04-04 under the Alien cast.
- Parker owns scwx-qt, Qt managers, views, and rendering integration.
- Use `Q_EMIT` for Qt signals in this repo.
- When a map load fails due to an invalid API key, `SetMapStyle("None")` is called. When the API key later changes to a valid one, the style name "None" must be treated as "no valid style" — pass `""` to `ResolveMapStyleName` or `ResetMap` so the provider's default style is used instead of staying blank. (Issue #605)

## Session History

### 2026-04-04 — Fixed Map Style Reset on API Key Change (Issue #605)

**Status:** ✅ Complete

Fixed regression in `MapWidgetImpl::ConnectSignals()` where map remained blank after user corrected invalid API key.

**Changes:**
- Modified `mapbox_api_key` lambda to check if active style is "None" and reset to default
- Modified `maptiler_api_key` lambda with same logic
- File: `scwx-qt/source/scwx/qt/map/map_widget.cpp`

**Decision:** Treat "None" as equivalent to empty style name, allowing provider defaults to apply. Merged to canonical decisions.md.

**Commit:** `Fix map style reset to default when API key changes (#605)`