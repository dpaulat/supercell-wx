#version 330 core

precision mediump float;

// Raw grid values (e.g. dBZ), one texel per grid cell. NEAREST-filtered
// for every fill-mode product so a sentinel like MRMS's -999 "no
// coverage" never blends into a real neighboring value at a cell
// boundary -- LINEAR-filtered instead for contour-mode products (see
// uContourInterval below and GribProductLayer::LoadFrame).
uniform sampler2D uDataTexture;

// 1D color ramp, same mechanism as radar.frag's uTexture.
uniform sampler1D uPalette;

uniform float uDataMomentOffset;
uniform float uDataMomentScale;

// Values below this are transparent -- both genuinely calm-air/no-echo
// readings and, as a side effect, MRMS's -999 "no coverage" sentinel
// (see grib-helper/README.md; eccodes' own missing-value API does not
// catch that sentinel, it has to be handled here). A real implementation
// would carry a per-product floor from the helper rather than hardcoding
// one value for every field.
uniform float uNoDataThreshold;

// 0 (the common case) means the normal palette fill below; a nonzero
// value switches this shader to drawing anti-aliased isolines at every
// multiple of this value instead (e.g. 400 for MSLP's 4 hPa/400 Pa
// synoptic convention) -- see GribManager's ProductConfig::
// contourInterval and GribProductLayer::LoadFrame's matching switch to
// LINEAR texture filtering (required for dFdx/dFdy below to reflect the
// real data gradient instead of spiking at texel edges).
uniform float uContourInterval;

layout(std140) uniform LayerState
{
   float uOpacity;
};

in vec2 texCoord;

layout (location = 0) out vec4 fragColor;

void main()
{
   float value = texture(uDataTexture, texCoord).r;

   if (value < uNoDataThreshold)
   {
      discard;
   }

   if (uContourInterval > 0.0)
   {
      // Standard fract/fwidth isoline technique: `lines` counts contour
      // intervals crossed (e.g. 101.6 means "1.6 intervals past the last
      // line"), so its fractional part hits 0 exactly on a contour and
      // 0.5 exactly halfway between two. `dist` folds that into "distance
      // from the nearest line, in interval units", and dividing by
      // fwidth(lines) (the on-screen rate of change of `lines`) converts
      // that into screen-space pixel-ish units -- so the line's apparent
      // width stays roughly constant across zoom levels instead of
      // shrinking to nothing when zoomed in. The 1.5 factor is a
      // first-guess line-width multiplier, worth eyeballing live and
      // adjusting.
      float lines    = value / uContourInterval;
      float dist     = abs(fract(lines - 0.5) - 0.5) / fwidth(lines);
      float alpha    = 1.0 - smoothstep(0.0, 1.5, dist);

      if (alpha <= 0.0)
      {
         discard;
      }

      // Fixed line color for now -- MSLP is the only contour product
      // today, so a single legible default (light, mostly-opaque, reads
      // over both light and dark basemaps) isn't worth making
      // configurable yet; revisit if a second contour product wants a
      // different look.
      fragColor   = vec4(1.0, 1.0, 1.0, alpha * 0.85);
      fragColor.a *= uOpacity;
      return;
   }

   float lutCoord = (value - uDataMomentOffset) / uDataMomentScale;

   fragColor = texture(uPalette, lutCoord);
   fragColor.a *= uOpacity;
}
