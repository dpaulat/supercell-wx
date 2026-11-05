#!/usr/bin/env bash
# Profile Supercell Wx Vulkan render path (especially multi-pane grids).
#
# Usage:
#   ./tools/profile-vulkan-grid.sh [seconds] [csv_path]
#
# Before running:
#   1. Set map grid to 3x3 in the app (Panes menu) or restore saved layout.
#   2. Load alerts / pick a busy radar site for realistic overlay cost.
#
# While running, exercise:
#   - Pan/zoom active pane (map move)
#   - Switch radar sites (Level 2 site picker)
#   - Toggle alert layers / timeline scrub
#
# Logs: SCWX_VULKAN_PERF=1 → spdlog "Vulkan perf" lines every 120 frames per pane
#       plus grid aggregate. CSV rows appended when SCWX_VULKAN_PERF_CSV is set.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT}/build-local/Release/bin/supercell-wx"
DURATION="${1:-45}"
CSV="${2:-/tmp/scwx-vulkan-perf.csv}"

if [[ ! -x "${BIN}" ]]; then
   echo "error: build supercell-wx first: ${BIN}" >&2
   exit 1
fi

export SCWX_VULKAN_PERF=1
export SCWX_VULKAN_PERF_CSV="${CSV}"
: > "${CSV}"

echo "Profiling ${BIN} for ${DURATION}s"
echo "CSV: ${CSV}"
echo "Interact with the app (pan map, switch sites, alerts)."

LOG="/tmp/scwx-vulkan-perf.log"
if command -v xvfb-run >/dev/null 2>&1 && [[ -z "${DISPLAY:-}" ]]; then
   echo "No DISPLAY; using xvfb-run (limited — manual interaction unavailable)"
   timeout "${DURATION}" xvfb-run -a "${BIN}" 2>&1 | tee "${LOG}" || true
else
   timeout "${DURATION}" "${BIN}" 2>&1 | tee "${LOG}" || true
fi

echo ""
echo "=== Perf log summary (Vulkan perf lines) ==="
rg "Vulkan perf" "${LOG}" 2>/dev/null | tail -20 || grep "Vulkan perf" "${LOG}" | tail -20 || true

echo ""
echo "=== CSV tail ==="
tail -5 "${CSV}" 2>/dev/null || true

echo ""
echo "Done. Full log: ${LOG}"
