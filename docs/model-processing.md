# Forecast model processing

Supercell Wx can download, process, and display operational forecast-model
data through the optional Rusty Weather model bridge. Open the dedicated
**View > Forecast Models** dock to use the feature. The dock is separate from
the Radar Toolbox and can be docked, floated, hidden, or restored with the
other application docks.

The integration uses a versioned JSON-lines protocol between Supercell Wx and
the bridge. Supercell Wx therefore does not link a Rust ABI, and the C++ and
Rust implementations can evolve independently.

## Capabilities

- Model, cycle, source, and product catalogs are reported by the bridge instead
  of being duplicated in Supercell Wx.
- Run probing reports the forecast hours currently published by the selected
  source.
- Downloads are cached, decoded, and retained in a reusable local run store.
  A view-oriented profile limits downloads to map fields, while the full and
  sounding profiles include the additional data needed by their workflows.
- Heavy ECAPE diagnostics are opt-in. Normal `full` processing retains the
  five sounding volumes without running the minute-scale heavy compute stage.
- Direct, derived, heavy, and windowed products use Rusty Weather's production
  recipes and palettes.
- One or more products and forecast hours can be rendered as transparent
  Web-Mercator PNG overlays for the current map view or explicit bounds.
- Product favorites, overlay visibility, opacity, and main-timeline syncing are
  persisted. The dock also supports stepping and looping through forecast
  hours independently of the radar timeline.
- Point soundings can be generated from a downloaded sounding-profile run. The
  bridge samples the stored model grid at the requested location and renders
  the complete `sharppyrs` sounding view, including the Skew-T, hodograph, and
  diagnostics, without a second model-data download.
- Fetch, render, and sounding work runs outside the UI process and reports
  progress. Canceling an operation terminates the bridge child process without
  blocking the map.

## Installing the bridge

Build `rw_model_bridge` from Rusty Weather and place it beside the Supercell Wx
executable, select it in the **Processor** section of the Forecast Models dock,
or make it available on `PATH`.

Packagers can supply a prebuilt bridge without adding Rust to the Supercell Wx
build:

```text
-DSCWX_MODEL_BRIDGE_EXECUTABLE=/path/to/rw_model_bridge
-DSCWX_MODEL_BRIDGE_LICENSE=/path/to/rusty-weather/LICENSE
```

This copies the executable beside the application and includes it in the
install component. The license option adds Rusty Weather's MIT license to the
install artifact; packagers must include it when distributing the bridge. If
no bridge is packaged, radar and alert features continue to work normally and
the Forecast Models dock reports the missing optional processor.

## Map workflow

1. Open **View > Forecast Models**, reload the processor catalog, and select a
   supported model and source.
2. Find the latest run, or choose a UTC date and cycle explicitly.
3. Select forecast hours and a data profile, then download and process the run.
4. Search the product catalog, optionally mark frequently used products as
   favorites, and select one or more products.
5. Display the selection on the current map view. Use the forecast slider,
   playback controls, or main-timeline sync to select valid times.

Map overlays are georeferenced raster snapshots rendered for the bounds
selected at render time. If **Use current viewport bounds** is enabled, zooming
or panning beyond that area exposes the edge of the raster; rerender from the
new view or disable that option and provide broader bounds.

## Point-sounding workflow

1. Download the required forecast hours with the **sounding** data profile (or
   a profile that contains the same pressure-level and surface fields).
2. Open the sounding view, choose a stored run and forecast hour, and enter a
   latitude and longitude or use the current map center.
3. Generate the sounding. The completed `sharppyrs` image is displayed in the
   dock and retained with the other generated model output. Use **Open sounding
   in window** for a separate resizable viewer with fit-to-window and actual-size
   modes.

Soundings are sampled from the nearest usable stored grid location. They do not
replace observed soundings, and their availability depends on the fields in the
selected download profile. If an hour was previously downloaded with the
lighter `view` profile, select `sounding` and process that hour again. The hour
store is replaced with the requested field set, while cached source files are
reused when available.

## Data ownership and application architecture

The Rusty Weather bridge acts as the external provider boundary: it owns remote
discovery and acquisition, decoding, and its local cache/run-store format.
Supercell Wx consumes normalized, typed model, run, product, frame, and sounding
records. Its model manager coordinates those records for the dock and map layer
without exposing Rusty Weather storage details to either UI.

The manager/provider boundary leaves a clean path for a generic product
datastore: storage behind the manager can change without redesigning the
Forecast Models dock or model map layer.

By default, the bridge stores model data beneath Supercell Wx's standard local
cache/data directories. No forecast-model data is uploaded to Supercell Wx
services.
