# grib-helper

`decode_grib` decodes a GRIB2 field with eccodes and writes it out in the
binary-framed wire format `manager::GribProductLayer`/`WindBarbLayer`
read (JSON metadata line, then raw float32 grid bytes -- no PNG, no
per-value text encoding). `manager::GribManager` and
`manager::WindBarbManager` invoke it out-of-process via `QProcess`; it
isn't linked directly into `wxdata` because eccodes isn't on conancenter,
and going out-of-process keeps it that way regardless of platform.

Because it depends on a system package rather than a submodule or Conan
recipe, building it is optional: `grib-helper/CMakeLists.txt` only
defines the `decode_grib` target when `pkg-config` finds `eccodes`. If it
isn't found, the rest of the project still configures and builds; the
GRIB and RTMA wind-barb overlays are simply unavailable at runtime
(`GribManager`/`WindBarbManager` log a warning and no-op rather than
crash).

## Installing eccodes

- Arch / CachyOS: `yay -S eccodes` (AUR)
- Debian / Ubuntu: `apt install libeccodes-dev`
- Fedora: `dnf install eccodes-devel`
- macOS: `brew install eccodes`
- Windows: no known package manager recipe yet -- `decode_grib` is not
  currently built or verified on Windows. This is a known gap, not a
  deliberate exclusion.

## Packaging status

- **Linux CI apt install / plain builds**: `libeccodes-dev` is in the CI
  apt-get list (`.github/workflows/ci.yml`), so `decode_grib` builds
  normally there and in any local build with the package installed.
- **AppImage**: the `linuxdeploy` step passes `decode_grib` via `-e` (see
  the "Build AppImage" job step) specifically so its own `libeccodes`
  dependency gets bundled into `usr/lib/` too -- without that flag,
  linuxdeploy only resolves dependencies for the executable named by the
  `.desktop` file's `Exec` line (`supercell-wx`), not other binaries
  dropped into `usr/bin/`, so `decode_grib` would silently only work on
  systems that happen to already have eccodes installed system-wide.
- **Flatpak: known gap, not yet fixed.** The Flatpak build copies the same
  raw `cmake --install` tree the AppImage step starts from (see
  `tools/net.supercellwx.app.yml`'s `supercell-wx` module), not the
  linuxdeploy-bundled AppDir -- so `libeccodes.so` never travels with it,
  and Flatpak's stricter sandboxing means there's no host-library fallback
  the way a plain Linux install might have. Fixing this needs either a
  real eccodes Flatpak module (build-from-source, no existing freedesktop
  SDK extension known) or explicitly staging the built `.so` into `/app/lib`
  as part of the manifest's `build-commands` -- neither attempted here,
  since verifying either needs an actual Flatpak build/run loop this
  environment doesn't have.

## Other tools in src/

`gen_wind_barbs.cpp` and `validate_lcc.cpp` are one-off scratch tools (asset
generation for the wind-barb icon sheets, and Lambert-projection math
validation against real eccodes output) -- not CMake targets, compile them
directly if you need to rerun them (each file's own header comment has the
command). Kept for provenance, not part of the regular build.

## Usage

```
decode_grib <input.grib2> <output.frame> [colorOffset colorScale noDataThreshold [shortName]]
```

`shortName` selects one message out of a multi-field file (RTMA/RRFS
bundle many fields per file); omit it for a single-message file (MRMS).

## Notes from the original MRMS validation

Source: `CONUS/MergedReflectivityQCComposite_00.50` from the public
`noaa-mrms-pds` S3 bucket.

- eccodes reads the file with no issues -- no MRMS-specific template
  problems.
- Grid: 7000 x 3500 (0.01 deg CONUS), matches NSSL's published spec.
  `longitudeOfFirstGridPointInDegrees` comes back in 0-360 convention
  (230.005) -- normalize to -180..180 before using it.
- `shortName` reports as `"unknown"` for MRMS -- eccodes has no name
  mapping for this local table entry. Harmless here (MRMS products are
  matched by file path, not shortName), but rules out relying on
  eccodes' human-readable field names for MRMS.
- **MRMS does not use the GRIB bitmap/missingValue mechanism.**
  `numberOfMissing` reports `0` even though the actual value range is
  `[-999, 58]` -- the `-999` "no coverage" cells are an in-band sentinel
  baked into the data array, not flagged via eccodes' missing-value API.
  Callers must special-case this value explicitly per product rather
  than trusting eccodes to report missing cells for MRMS the way it
  would for a well-behaved model GRIB file.
- Frame size for a full CONUS field: 98,000,000 bytes (24.5M cells x 4
  bytes) plus a ~170 byte header -- confirms the case for a binary
  wire format over JSON/text/base64 encoding.
