# Squad Decisions

## Active Decisions

### Treat "None" Style as Empty on API Key Change

**Author:** Parker  
**Date:** 2026-04-04  
**Issue:** #605

When a map fails to load due to an invalid API key, `SetMapStyle("None")` is called to signal no valid style is active. When the user later enters a valid API key, the `mapbox_api_key` and `maptiler_api_key` changed signals fire — but they were passing the literal style name `"None"` into `ResetMap` / `SetMapStyle`, leaving the map blank.

**Decision:** In the API key changed signal lambdas inside `MapWidgetImpl::ConnectSignals()`, treat `"None"` as equivalent to an empty style name. Pass `""` to `ResetMap` (Mapbox path) and call `ResolveMapStyleName("")` (MapTiler path) so the provider's first/default style is used instead.

**Rationale:** `ResolveMapStyleName("")` already handles the "no style requested" case by returning the first style for the current provider. This is the correct semantic: if the previous style was invalid (None), restoring a working API key should restore the default style rather than preserving the failed state.

## Governance

- All meaningful changes require team consensus
- Document architectural decisions here
- Keep history focused on work, decisions focused on direction
