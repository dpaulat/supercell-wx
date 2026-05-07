# Merge Plan: PR #626 — Multi-Pane Map Layout

**Target PR:** https://github.com/dpaulat/supercell-wx/pull/626
**Author:** @F1r3w477
**Branch:** `feature/pane-functionality-changes`
**Base:** `upstream/develop` (0ae0bbbe8f7a259d17f619820a2b578c164d7926)
**Our fork:** `AscendedGravity/supercell-wx`, branch `develop`

**PR stats:** +3821 / -344 across 90 files, 13 commits
**Merge conflicts:** 29 conflict regions across 6 files
**Overall difficulty:** HARD

---

## Decisions Made (from discussion with maintainer)

| Question | Decision |
|---|---|
| Map widget creation strategy | PR's dynamic approach wins. Discard our pre-create-all-9 hidden-container pattern. |
| State persistence | Combine both. Presets save grid + splitter + products + link state. PR's session persistence (close/restore) stays separate. |
| Mouse handling | PR's restructured handlers win. Re-add our `MapClicked` signal into PR's `mouseReleaseEvent`. |
| Scope | Full integration required — all features from both sides must work together. |

---

## Files Requiring Merge Resolution

### 1. `scwx-qt/source/scwx/qt/main/main_window.cpp` — VERY HARD (19 conflict regions)

**What PR #626 brings:**
- `EnsureMapWidgets()` / `BuildMapLayout()` / `TeardownMapLayout()` / `RebuildMapLayoutContainer()` / `RecreateMapLayoutFromUser()`
- `SelectRadarSiteRespectingViewLink()` — respects per-pane link-view state
- `SaveMapPaneSplitterState()` / `RestoreMapPaneSizesFromSettingsIfMatching()` / `SaveMapPaneViewLinkState()` / `RestoreMapPaneViewLinkFromSettingsIfMatching()`
- Pop-out window system: `PopOutMap()`, `DockPoppedMap()`, `DockAllPoppedBeforeTeardown()`, `SavePoppedMapWindowsToSettings()`, `TryRestorePoppedMapWindows()`
- Link-view: `SetLinkRowSplitters()`, `SetLinkColumnHeights()`, `SnapLinkedColumnWidths()`, `SnapLinkedColumnHeights()`
- Pane menu: `SetupPanesMenu()`, `UpdatePanesPresetSelection()`, `ApplyMapGridPreset()`
- Map style sync: `ConfigureMapStyles(bool)`, `ApplyMatchMapStyleFromMainToAllPanes()`, `OnPanesMatchMapStyleToggled()`
- Context menu handler: `OnMapPaneContextMenuRequested()`, `HandleMapPaneLinkViewToggled()`
- State tracking members: `mapPaneViewLinked_`, `mapPanePoppedOut_`, `mapPanePlaceholders_`, `mapPanePopoutFrames_`, `mapLayoutRoot_`, `linkRowSplitters_`, `linkColumnHeights_`, `builtLayoutGridW_/H_`
- Deferred grid re-sync via `ScheduleMapLayoutSyncIfGridChanged()` (replaces our `gridRebuildTimer_`)
- `ApplyRadarProductsFromSettingsToAllMaps()`
- `ApplyReferencePaneToPendingNewPanes()` for inheriting settings on new panes

**What our fork brings:**
- Radar preset system: `OnSavePreset()`, `OnLoadPreset()`, `OnRenamePreset()`, `OnDeletePreset()`, `ClearActivePreset()`, `PopulatePresetComboBox()`
- Preset members: `radarSitePresetsActions_`, `radarSitePresetsMenu_`, `applyingGridChange_`, `gridRebuildTimer_`, `gridWidthConnection_`, `gridHeightConnection_`
- SPC convective outlook UI (~200 lines of widget setup in constructor)
- GFS sounding dock panel + corner-positioning logic
- Dock width persistence: `firstShow_`, `applyingDockWidth_`, `dockWidthSaveTimer_`, `radarToolboxDockWidth` event filter
- `InitializeGlContext()` / `CreateAllMapWidgets()` / `RebuildMapLayout()` (to be discarded)
- `mapContainer_` hidden widget (to be discarded)
- `resizeEvent()` override for dock width re-application
- Additional includes: `QCheckBox`, `QComboBox`, `QHBoxLayout`, `QInputDialog`, `QVBoxLayout`, `QSlider`, `QResizeEvent`

**Resolution Strategy (in order):**

1. **Includes section:** Accept PR's new includes (`QAction`, `QActionGroup`, `QJsonArray/Document/Object`, `QMenu`, `QPointer`, `QPoint`, `QSizePolicy`, `QDialog`, `QLabel`, `<array>`, `<cmath>`, `<cstdint>`, `<cstddef>`, `<memory>`). Keep our additions (`QCheckBox`, `QComboBox`, `QHBoxLayout`, `QInputDialog`, `QVBoxLayout`, `QSlider`, `QResizeEvent`, `<scwx/qt/manager/spc_outlook_manager.hpp>`, `<scwx/spc/spc_types.hpp>`, `<scwx/qt/settings/spc_outlook_settings.hpp>`, `<scwx/qt/settings/radar_preset_settings.hpp>`, `<scwx/qt/ui/sounding_panel.hpp>`).

2. **Anonymous namespace helpers:** Accept PR's `AllMapPanesShareSameMapStyle()`, `AllMapPanesReportResolvedMapStyle()`, `FindWidgetInGridSplitters()`, `MapSlotByMapIndex()`.

3. **Class declaration block:** Keep all PR's new method declarations and member variables. Keep all our preset-related methods and members. **Discard:**
   - `InitializeGlContext()` — replaced by PR's path
   - `CreateAllMapWidgets()` — replaced by `EnsureMapWidgets()`
   - `RebuildMapLayout()` — replaced by `BuildMapLayout()` / `RecreateMapLayoutFromUser()`
   - `ConnectMapSignalsForWidget()` / `ConnectAnimationSignalsForWidget()` — function signatures change; inline into new connection pattern
   - `gridRebuildTimer_` — replaced by PR's `ScheduleMapLayoutSyncIfGridChanged()`
   - `applyingGridChange_` — replaced by PR's mechanism
   - `gridWidthConnection_` / `gridHeightConnection_` — replaced by PR's mechanism
   - `mapContainer_` — PR's dynamic approach doesn't need hidden container
   - `firstShow_` / `applyingDockWidth_` / `dockWidthSaveTimer_` — dock width persistence logic integrated differently

4. **Constructor:** Start from PR's constructor structure. Then layer in:
   - Our radar site presets menu setup (`radarSitePresetsMenu_`)
   - Our SPC outlook UI (day/prod combo, opacity slider, auto-refresh, lambdas)
   - Our GFS sounding dock + menu action
   - Our preset combo box and buttons + `PopulatePresetComboBox()`
   - Our toolbox dock width restore (`resizeDocks` call)
   - Our `eventFilter` install for dock resize tracking
   - Call our `ConfigureUiSettings()` (which includes `spcOutlookGroup_`)

   Replace:
   - Our `p->InitializeGlContext()` + `p->CreateAllMapWidgets()` + `p->RebuildMapLayout(...)` with PR's `p->BuildMapLayout()`
   - Our `for (size_t i...) SelectRadarProduct(...)` with PR's `p->ApplyRadarProductsFromSettingsToAllMaps()`

5. **`ConfigureMapLayout()`:** Replace with PR's `RebuildMapLayoutContainer()` delegation.

6. **`ConfigureMapStyles()`:** Accept PR's version (with `matchMapStyle` support, per-pane style validation). Add back `SetMapStyle()` calls (PR's version only calls `SetInitialMapStyle`).

7. **`ConnectMapSignals()` / `ConnectAnimationSignals()`:** Accept PR's structure of `ConnectMapToTimelineAndRadarSiteSignals()` + `ReconnectMapDataConnections()`. Ensure our SPC outlook, sounding panel, and dock-width connections are added in `ConnectOtherSignals()`.

8. **`closeEvent()`:** Accept PR's additions for saving pop-out/splitter/link state. Keep our existing save of UI state/geometry.

9. **`showEvent()`:** Accept PR if modified; layer our dock width re-application on top.

10. **Radar site selection handlers:** Replace all simple loops over `maps_` with calls to `SelectRadarSiteRespectingViewLink()` where appropriate.

11. **`UpdateRadarSite()` / `UpdateVcp()` / etc.:** These are identical on both sides; accept whichever version.

12. **Preset methods (`OnSavePreset`, `OnLoadPreset`, etc.):**
    - Adapt `OnSavePreset` to save `maps_.size()` products (not fixed 9) and use PR's splitter state saving approach
    - Adapt `OnLoadPreset` to call PR's `RecreateMapLayoutFromUser()` instead of our `RebuildMapLayout()`
    - Both methods need to handle the `mapPaneViewLinked_` state if we want presets to capture link state

13. **`ConnectOtherSignals()`:** This is the biggest integration point for our features:
    - Our preset button connections (`savePresetButton`, `loadPresetButton`, etc.)
    - SPC outlook day/product/opacity/auto-refresh connections
    - Sounding panel point selection
    - Dock width save timer
    - PR's pane menu connections (`actionPanesLinkColumnWidth`, etc.)
    - Both sets of grid change handlers (PR's `ScheduleMapLayoutSyncIfGridChanged` > our `gridRebuildTimer_`)

---

### 2. `scwx-qt/source/scwx/qt/map/map_widget.cpp` — MEDIUM (1 large conflict region)

**What PR #626 brings:**
- Moved `SetRadarSite()` call from `MapWidgetImpl` constructor to `MapWidget` constructor body (after `p` is fully constructed)
- Added `contextMenuEvent()` override (emits `MapPaneContextMenuRequested`)
- Added `mouseReleaseEvent()` and `mouseDoubleClickEvent()` overrides
- Restructured `mousePressEvent()` with context menu arm/drag detection:
  - `paneContextMenuArmed_`, `paneContextMenuDragTooFar_`, `paneContextMenuPressPos_`, `paneContextMenuDragThresholdSq_`
  - Left+right chord immediately sets `dragTooFar_ = true`
  - Right-button rotates only after drag exceeds `startDragDistance`
  - `lastPos_` reset on threshold crossing for smooth rotation
- Added `SyncStoredViewFromMap()`, `RequestRepaint()`, `CancelPaneContextMenuDebounce()`, `GetMapViewParameters()`
- Added `map_ == nullptr` guards throughout for deferred initialization
- Refactored `InitializeNewRadarProductView()` and `UpdateColorTable()` (identical refactoring to ours)
- `SetMapParameters()` now calls `SyncStoredViewFromMap()` + `RequestRepaint()` at end
- Added `setContextMenuPolicy(Qt::NoContextMenu)` in constructor

**What our fork brings:**
- `lightningLayer_` member + creation in `AddLayer()`
- `convectiveOutlookLayer_` + `convectiveOutlookConnection_` members + creation in `AddLayer()`
- `MapClicked` signal emission on left-click in `mousePressEvent` (for sounding panel)
- Added includes: `<scwx/qt/map/lightning_layer.hpp>`, `<scwx/qt/map/convective_outlook_layer.hpp>`, `<scwx/qt/manager/spc_outlook_manager.hpp>`
- Refactored `InitializeNewRadarProductView()` / `UpdateColorTable()` — identical to PR's

**Resolution Strategy:**

1. **Inits and includes:** Accept PR's base plus add our lightning + convective outlook headers and `LightningLayer`/`ConvectiveOutlookLayer` includes.

2. **Constructor:** Accept PR's version (moves `SetRadarSite` out of `p` init). Keep our `setContextMenuPolicy(Qt::NoContextMenu)` which PR also adds.

3. **Members:** Accept PR's context menu members plus our `lightningLayer_`, `convectiveOutlookLayer_`, `convectiveOutlookConnection_`.

4. **Mouse handlers:** Accept PR's restructured `mousePressEvent`, plus PR's new `mouseReleaseEvent`, `mouseDoubleClickEvent`, `contextMenuEvent`. **Re-add our `MapClicked` signal** in `mouseReleaseEvent` on left-button release (not press, to avoid conflicts with PR's arm/drag logic).

5. **`InitializeNewRadarProductView()` / `UpdateColorTable()`:** Accept PR's version (identical refactoring).

6. **`AddLayers()` / `AddLayer()`:** Accept PR's version with `map_ == nullptr` guard. Add our `LightningLayer` and `ConvectiveOutlookLayer` creation, and `convectiveOutlookConnection_` signal connection at the same insertion points.

7. **`SetMapParameters()`:** Accept PR's version with `SyncStoredViewFromMap()` and `RequestRepaint()`.

8. **`leaveEvent()`, `keyPressEvent()`, `keyReleaseEvent()` etc.:** Accept PR's version with null checks.

9. **`GetMapViewParameters()`:** Accept PR's new method.

---

### 3. `scwx-qt/source/scwx/qt/settings/ui_settings.hpp` — EASY (1 conflict region)

**Both sides add new accessor method declarations.** Resolution:

- Keep all existing declarations.
- Add PR's: `map_pane_splitter_state()`, `map_pane_popout_state()`, `map_pane_view_link_state()`, `panes_match_map_style()`
- Keep ours: `spc_outlook_expanded()`, `radar_toolbox_dock_width()`

The PR already includes `<cstdint>` transitively, so no additional includes needed.

---

### 4. `scwx-qt/source/scwx/qt/settings/ui_settings.cpp` — EASY (6 small conflict regions)

**Both sides add new `SettingsVariable` members.** Interleave as follows:

In `UiSettingsImpl` member variables block add (after `timelineExpanded_`):
```cpp
SettingsVariable<bool>        spcOutlookExpanded_ {"spc_outlook_expanded"};
SettingsVariable<std::string> mapPaneSplitterState_ {"map_pane_splitter_state"};
SettingsVariable<std::string> mapPanePopoutState_ {"map_pane_popout_state"};
SettingsVariable<std::string> mapPaneViewLinkState_ {"map_pane_view_link_state"};
SettingsVariable<bool>        panesMatchMapStyle_ {"panes_match_map_style"};
SettingsVariable<std::int64_t> radarToolboxDockWidth_ {"radar_toolbox_dock_width"};
```

Similarly interleave in: constructor initializers, `RegisterVariables()`, `Shutdown()`, accessor methods, and `operator==()`.

---

### 5. `scwx-qt/source/scwx/qt/ui/layer_dialog.cpp` — EASY (1 conflict region)

**PR adds:** `RefreshMapDisplayColumns()` public method + grid width/height signal connections in `ConnectSignals()`.
**We add:** `UpdateMapDisplayColumns()` public method (same implementation, different name).

Resolution:
- Keep PR's method name `RefreshMapDisplayColumns()` (more descriptive).
- Keep PR's grid width/height signal connections in `ConnectSignals()`.
- Update any callers of our `UpdateMapDisplayColumns()` to use `RefreshMapDisplayColumns()`.

---

### 6. `scwx-qt/source/scwx/qt/ui/layer_dialog.hpp` — EASY (1 conflict region)

Same resolution as .cpp — rename our `UpdateMapDisplayColumns()` to `RefreshMapDisplayColumns()`.

---

## Files Acceptable as-is from PR (no conflicts)

These new files should be accepted without modification:

| File | Purpose |
|---|---|
| `scwx-qt/source/scwx/qt/map/map_pane_context_menu.hpp/.cpp` | Right-click pane context menu (L2/L3 submenus, pop-out/dock/link actions) |
| `scwx-qt/source/scwx/qt/map/map_link_policy.hpp/.cpp` | Link-view sync gate for map parameter updates |
| `scwx-qt/source/scwx/qt/map/map_pane_splitter_state.hpp` | Splitter size validation helper |
| `scwx-qt/source/scwx/qt/map/map_pane_view_link_state.hpp/.cpp` | JSON parse/serialize for link state |
| `scwx-qt/source/scwx/qt/map/map_popout_frame.hpp/.cpp` | Top-level pop-out window for detached panes |
| `scwx-qt/source/scwx/qt/main/main_window.ui` changes | `menuPanes` with all grid preset actions |

Also accept PR's `scwx-qt/scwx-qt.cmake` changes (adds the 9 new source files above).

---

## Dependencies / Settings Considerations

- **`types::kMapCount_` (fixed at 9):** Our preset system and layer model both use `kMapCount_ = 9` for fixed-size arrays. With PR's dynamic grid, these should ideally support variable counts. Short-term: keep at 9 (covers 3x3 max). The `RadarPresetSettings::Preset::products` array stays at 9; only the active grid's products are meaningful.
- **`RadarPresetSettings` (new class):** Our custom settings class needs to be kept. No conflicts with PR code expected.
- **`SpcOutlookSettings` (new class):** Our custom SPC outlook settings. No conflicts.
- **`map_pane_view_link_state` vs our preset link state:** If presets should capture link state, add a `linked` field to `Preset::ProductEntry` or a separate bitmask in the preset JSON.

---

## Merge Order of Operations

1. Start from our `develop` branch
2. Merge PR #626 head, accepting PR's version for all conflicted files
3. Restore our additions layer by layer:
   a. **`ui_settings.hpp/.cpp`** — interleave new variables
   b. **`layer_dialog.hpp/.cpp`** — rename method
   c. **`map_widget.cpp`** — add lightning + convective outlook layers, re-add `MapClicked`
   d. **`main_window.cpp`** — biggest effort: adapt preset system, SPC outlook, sounding panel, dock width
4. Build and test
5. Commit

---

## Files to Watch for Regressions

- **`main_window.cpp`:** All our features must be tested: preset save/load, SPC outlook rendering, sounding panel point selection, dock width persistence, radar site presets
- **`map_widget.cpp`:** Right-click context menu (PR), left-click for sounding (ours), lightning layer, convective outlook layer
- **`ui_settings.cpp`:** All new settings variables properly registered and committed
- **`layer_dialog.cpp`:** Map columns update on grid change
- **New PR features to test:** Pop-out windows, link-view, match-map-style, panes menu, splitter state persistence
