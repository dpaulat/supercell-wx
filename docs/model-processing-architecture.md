# Forecast model processing architecture

This document describes the current forecast-model integration boundary in
Supercell Wx. The feature is implemented in the C++/Qt application, with model
acquisition and processing delegated to an optional standalone executable. It
does not change the existing radar or alert pipelines.

## Component and process boundaries

```mermaid
flowchart LR
    subgraph SCWX["Supercell Wx process - C++ / Qt"]
        Dock["Forecast Models dock<br/>ModelWidget"]
        Manager["ModelManager<br/>QProcess lifecycle, settings, selection"]
        Records["Typed model records<br/>ForecastModel / ModelRuns / ModelCatalog<br/>ModelFrame / ModelSounding"]
        Map["MapWidget<br/>visible bounds and map center"]
        Layer["ModelLayer<br/>PNG composite and OpenGL texture"]
        SoundingView["Sounding tab<br/>QPixmap viewer"]

        Map -->|"bounds or center"| Dock
        Dock -->|"probe, fetch, render, sounding"| Manager
        Manager -->|"parse and normalize"| Records
        Records -->|"catalogs, runs, progress"| Dock
        Records -->|"FramesSelected"| Layer
        Layer -->|"georeferenced overlay"| Map
        Records -->|"SoundingAvailable"| SoundingView
    end

    subgraph Helper["Optional child process"]
        Bridge["rw_model_bridge<br/>one CLI operation per QProcess"]
        Pipeline["source discovery, download,<br/>decode and derivation"]
        Renderers["map-overlay and<br/>point-sounding renderers"]

        Bridge --> Pipeline
        Bridge --> Renderers
    end

    subgraph Data["External and local data"]
        Sources["remote model sources"]
        Cache["raw GRIB cache"]
        Store["decoded local run store"]
        Artifacts["overlay and sounding PNGs"]
    end

    Manager -->|"command-line arguments via QProcess"| Bridge
    Bridge -->|"versioned JSONL events on stdout"| Manager
    Manager -.->|"cancel: terminate, then kill"| Bridge
    Bridge -.->|"stderr and process exit"| Manager
    Sources -->|"GRIB and indexes"| Pipeline
    Pipeline <--> Cache
    Pipeline --> Store
    Store --> Renderers
    Renderers --> Artifacts
    Artifacts -.->|"loaded from typed record paths"| Layer
    Artifacts -.->|"loaded from typed record path"| SoundingView
```

`ModelWidget` is the dedicated **View > Forecast Models** dock. It collects a
model, source, run, forecast hours, products, render bounds, and sounding point,
then calls the singleton `ModelManager`. `ModelManager` owns the child-process
lifecycle and converts bridge output into the host-owned types in
`model_types.hpp`: `ForecastModel`, `ModelProbeResult`, `ModelRuns`,
`ModelCatalog`, `ModelFrame`, and `ModelSounding`. The dock and map layer do not
read the helper's store format or model files directly.

The helper owns remote-source details, the raw download cache, decoding and
derived-field work, and the decoded run-store layout. Supercell Wx supplies
application-specific roots for three kinds of local data:

- the decoded run store under the application's local-data `models/store`
  directory;
- the raw GRIB cache under the application's cache `models/grib` directory;
- rendered overlays and soundings under the application's cache
  `models/overlays` directory.

## Host flows

For a map overlay, the dock obtains either the current `MapWidget` bounds or
explicit west/east/south/north bounds. The manager starts `render`; each
successful item becomes a `ModelFrame` containing the PNG path, forecast hour,
dimensions, valid time, and exact geographic bounds. Selecting a forecast hour
emits all matching frames. `ModelLayer` loads those PNGs, composites selected
products with `QPainter`, uploads the result as an OpenGL texture, and draws it
over the frame bounds in the MapLibre map. Opacity, visibility, forecast-hour
selection, playback, and optional main-timeline synchronization remain host
concerns.

For a point sounding, the dock sends a stored run, forecast hour, and either
entered coordinates or the current map center. The helper locates the point on
the stored grid, loads the sounding fields, and renders the complete
`sharppyrs` Skew-T, hodograph, and diagnostics view. The `sounding_complete`
event becomes a `ModelSounding`; the sounding tab loads its PNG and displays
the selected grid location and available metadata. It can also open the image
in a separate resizable fit/actual-size window. A sounding-profile fetch is
required when the map-oriented profile does not contain the needed vertical
fields. Reprocessing the same hour with the sounding profile replaces that
stored hour with the requested field set and reuses cached source files when
they remain available.

## Versioned process protocol

The current schema identifier is `rusty-weather.model-bridge.v1`. Every stdout
line is one JSON object with the envelope:

```json
{"schema":"rusty-weather.model-bridge.v1","event":"...","data":{}}
```

Commands are passed as CLI arguments; JSONL is the event/result channel, not a
linked API. Diagnostic text and fatal errors use stderr. The manager requires a
recognized schema, ignores unknown event names and object members, and turns
known payloads into typed records or Qt signals. The current command/event
surface is:

| Command | Events consumed by Supercell Wx | Host result |
| --- | --- | --- |
| `capabilities` | `capabilities` | model, source, cycle, and product definitions |
| `probe` | `probe_complete` | latest available run containing the requested forecast hour |
| `fetch` | `fetch_progress`, `fetch_hour_complete`, `fetch_complete` | progress plus stored run/hour availability |
| `runs` | `runs_complete` | locally stored runs and hours |
| `catalog` | `catalog_complete` | products renderable from a stored hour |
| `render` | `render_started`, `render_item_started`, `render_item_complete`, `render_item_skipped`, `render_item_failed`, `render_complete` | progress and zero or more `ModelFrame` records |
| `sounding` | `sounding_started`, `sounding_complete` | progress and one `ModelSounding` record |
| `cache-status` | `cache_status` | cache byte and file counts |

The normal `probe` path is intentionally bounded: it checks scheduled cycles
until it finds the newest run containing the requested forecast hour. It does
not inventory the model's entire forecast horizon. The response distinguishes
the confirmed hour from the cycle's supported-hour schedule with
`forecast_hours_complete: false`; exact all-hour publication discovery remains
an explicit `probe --all-hours` helper operation rather than part of
**Find latest**.

The helper also currently emits `render_hour_started`; it is intentionally safe
for an older host to ignore that informational event. A nonzero exit or process
crash is reported through stderr/process status rather than a separate JSON
error event.

Only one model operation runs at a time. Cancel first requests graceful process
termination, then kills the child after 1.5 seconds if it has not exited. A
failed start, nonzero exit, or crash clears the manager's busy state and is
reported to the dock. Because processing is in a separate OS process, a decoder
or renderer failure does not execute inside or unwind through the Supercell Wx
process. The ingest implementation uses staged/atomic store paths; cancellation
is still not presented as a transaction or rollback of every cache artifact.

## Build and runtime isolation

This integration adds no Rust source, Rust toolchain step, Rust library, or Rust
ABI to the Supercell Wx build or process. `SCWX_MODEL_BRIDGE_EXECUTABLE` is an
optional CMake file path: when set, CMake copies a prebuilt executable beside
`supercell-wx` and installs it with its license. Nothing from that executable is
added to `target_link_libraries`.

At runtime the manager looks beside the application, on `PATH`, or at the path
chosen in the dock. If no helper is present, model commands report that the
optional processor is unavailable. Radar loading, radar rendering, alerts, and
the rest of the application continue through their existing C++ paths and do
not depend on the model helper.

## Reviewer entry points

- [`ModelWidget`](../scwx-qt/source/scwx/qt/ui/model_widget.cpp) owns the dock,
  controls, playback, sounding preview, and user interaction.
- [`ModelManager`](../scwx-qt/source/scwx/qt/manager/model_manager.cpp) owns the
  `QProcess`, JSONL parsing, paths, settings, cancellation, and typed signals.
- [`model_types.hpp`](../scwx-qt/source/scwx/qt/types/model_types.hpp) is the
  provider-neutral record boundary consumed by the host.
- [`ModelLayer`](../scwx-qt/source/scwx/qt/map/model_layer.cpp) composites and
  draws georeferenced model frames on the existing map.
- [`scwx-qt.cmake`](../scwx-qt/scwx-qt.cmake) contains the optional packaging
  switch; no helper is required for a normal Supercell Wx build.
- [`model_types.test.cpp`](../test/source/scwx/qt/types/model_types.test.cpp)
  exercises the versioned host contract, including additive response fields.

The helper implementation and toolchain are intentionally outside this branch.
The preview release supplies a prebuilt `rw_model_bridge` and its notices; the
reviewable contract on the Supercell Wx side is the command/event table above
and the typed records at the host boundary.

## Raster extent behavior

The default render mode is deliberately viewport-bounded. Clicking render
captures the visible map bounds at that moment and requests a single
georeferenced raster for that rectangle. Panning or zooming out beyond it does
not reveal more model data, because `ModelLayer` correctly draws the texture
only over the bounds recorded in `ModelFrame`; rendering again captures the new
view.

For a broad or full-domain image, the user can clear **Use current viewport
bounds** and enter explicit bounds. This is an explicit-bounds path, not
automatic discovery of the model's native domain. The current renderer produces
one image per product/hour, preserves the Web-Mercator aspect ratio, and caps
either image dimension at 4096 pixels. A large domain therefore trades spatial
detail for coverage.

A future tiled renderer could request or cache georeferenced tiles as the map
moves, avoiding manual rerenders and keeping individual textures bounded. That
would require a tile or manifest record and corresponding `ModelLayer` support;
the current implementation is a single-raster overlay and does not provide
automatic tiling.

## Replacement and extension points

The stable seam for the host UI is the typed record/signal boundary, not the
Rust implementation or its on-disk store. A built-in C++ provider could perform
discovery, acquisition, storage, rendering, and sounding generation and then
produce the same records. Likewise, a future plugin system could supply a model
provider behind that boundary. The current code does not yet define a provider
interface or plugin ABI: `ModelManager` directly implements the QProcess/JSONL
adapter, so formalizing that interface would be the first refactor. The dock,
timeline behavior, and `ModelLayer` need not depend on which provider produces
the typed results.

| Integration shape | Advantages | Cost or constraint |
| --- | --- | --- |
| Built-in C++ provider | One process and toolchain; direct access to host services and any future generic datastore | Requires a native GRIB acquisition, decode, derivation, rendering, and sounding implementation before feature parity |
| Optional helper (current preview) | Reuses the complete processing pipeline; failures and high-memory work stay outside the UI process; protocol can evolve additively | Distribution must include and update a separate executable; data crosses a process boundary |
| Provider plugin | Multiple implementations could share the dock, timeline, and map layer without living in the core executable | Requires a supported provider API plus lifecycle, ABI/versioning, packaging, and trust decisions |

A low-risk next step for either a built-in implementation or plugin system is
to extract a `ModelProvider` interface from `ModelManager`. The current JSONL
adapter would become one provider, while the dock, typed records, and map layer
would remain unchanged.

The current [ProductDatastore work in PR
#643](https://github.com/dpaulat/supercell-wx/pull/643) is not yet that generic
provider boundary. Its public API and storage are specifically expressed in
terms of `RadarProductRecord`, `NexradFile`, and NEXRAD Level 2/Level 3 lookup
and cache semantics. Forecast-model runs contain multi-field grids, profiles,
derived products, and rendered artifacts, so this integration does not force
them through radar-specific records.

| Model artifact | Current owner | Possible generalized datastore role |
| --- | --- | --- |
| Raw GRIB and source indexes | Helper cache | Usually provider-private; expose cache accounting rather than file semantics |
| Decoded grids, profiles, and run metadata | Helper run store | A future datastore could index provider-neutral run/hour handles while leaving field layout behind the provider |
| Rendered overlay frames | Supercell Wx consumes `ModelFrame` records | Strong candidate for a generic georeferenced artifact record with product, valid time, bounds, and path |
| Sounding images and metadata | Supercell Wx consumes `ModelSounding` records | Candidate for a generic derived artifact record; profile arrays could remain provider-owned until a native sounding view needs them |

If ProductDatastore becomes provider-neutral, the model provider can register
those run or artifact records behind `ModelManager` without exposing helper
storage details to the dock or map layer. The unresolved design choice is
whether that datastore owns only discoverable artifacts or also the lifecycle
of provider-private decoded stores.
