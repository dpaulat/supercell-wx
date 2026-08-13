#!/usr/bin/env bash
# Supercell Wx profiling workflows (Linux).
#
# Usage:
#   ./tools/profile-workflows.sh check          # list installed / missing tools
#   ./tools/profile-workflows.sh builtin [sec]  # SCWX_VULKAN_PERF (see profile-vulkan-grid.sh)
#   ./tools/profile-workflows.sh cpu [sec]      # perf record → perf report / Hotspot
#   ./tools/profile-workflows.sh flame [sec]    # perf record -g → flamegraph (needs inferno)
#   ./tools/profile-workflows.sh callgrind      # valgrind callgrind (slow; deep CPU)
#   ./tools/profile-workflows.sh print-install  # dnf install one-liner
#
# Before profiling interactively:
#   - 3×3 linked panes, busy radar site, alerts loaded
#   - Pan ~10s, switch sites, scrub timeline while recording
#
# Disable basemap share A/B: SCWX_BASEMAP_SHARE=0

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${SCWX_BIN:-${ROOT}/build-local/Release/bin/supercell-wx}"
OUT="${SCWX_PROFILE_DIR:-/tmp/scwx-profile}"
DURATION="${2:-30}"

have() { command -v "$1" >/dev/null 2>&1; }

print_install() {
   cat <<'EOF'
Install (Fedora; run once):

  sudo dnf install perf hotspot renderdoc kcachegrind sysprof

Optional flamegraphs:

  sudo dnf install perl-FlameGraph   # or clone github.com/brendangregg/FlameGraph

AMD GPU (RADV): RenderDoc + Sysprof cover most Vulkan work.
For vendor traces: https://gpuopen.com/rgp/ (Radeon GPU Profiler).

Allow perf user stacks (until reboot):

  sudo sysctl kernel.perf_event_paranoid=1

EOF
}

cmd_check() {
   echo "Binary: ${BIN} ($([[ -x ${BIN} ]] && echo ok || echo MISSING))"
   echo "debug_info: $(readelf -S "${BIN}" 2>/dev/null | rg -q '\.debug_info' && echo yes || echo no)"
   echo "perf_event_paranoid: $(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo '?')"
   echo ""
   for t in perf hotspot valgrind callgrind_annotate kcachegrind qrenderdoc sysprof flamegraph rg; do
      printf "  %-20s %s\n" "${t}" "$(have "${t}" && echo yes || echo no)"
   done
   echo ""
   print_install
}

require_bin() {
   if [[ ! -x "${BIN}" ]]; then
      echo "error: build first or set SCWX_BIN: ${BIN}" >&2
      exit 1
   fi
}

cmd_builtin() {
   exec "${ROOT}/tools/profile-vulkan-grid.sh" "${DURATION}" "${OUT}/vulkan-perf.csv"
}

cmd_cpu() {
   require_bin
   have perf || { echo "install perf (see print-install)" >&2; exit 1; }
   mkdir -p "${OUT}"
   local data="${OUT}/perf-${DURATION}s.data"
   echo "Recording ${DURATION}s → ${data}"
   echo "Interact: pan 3×3, switch radar sites, load alerts."
   perf record -F 997 -g --call-graph fp -o "${data}" -- \
      env SCWX_VULKAN_PERF=1 "${BIN}" &
   local pid=$!
   sleep "${DURATION}"
   kill -INT "${pid}" 2>/dev/null || true
   wait "${pid}" 2>/dev/null || true
   echo ""
   echo "Report (flat, fast):"
   perf report -i "${data}" --stdio -g none --sort comm --percent-limit 2 2>&1 | head -40 || true
   echo ""
   echo "GUI (full stacks):  hotspot ${data}"
   echo "Full CLI:           perf report -i ${data}"
}

cmd_flame() {
   require_bin
   have perf || { echo "install perf" >&2; exit 1; }
   mkdir -p "${OUT}"
   local data="${OUT}/perf-flame-${DURATION}s.data"
   perf record -F 997 -g --call-graph fp -o "${data}" -- \
      env SCWX_VULKAN_PERF=1 "${BIN}" &
   local pid=$!
   sleep "${DURATION}"
   kill -INT "${pid}" 2>/dev/null || true
   wait "${pid}" 2>/dev/null || true
   if have flamegraph; then
      perf script -i "${data}" | flamegraph > "${OUT}/flame.svg"
      echo "Wrote ${OUT}/flame.svg"
   else
      echo "perf script -i ${data} | flamegraph > flame.svg"
      echo "(install FlameGraph or perl-FlameGraph)"
   fi
}

cmd_callgrind() {
   require_bin
   have valgrind || { echo "install valgrind" >&2; exit 1; }
   mkdir -p "${OUT}"
   local log="${OUT}/callgrind.log"
   echo "Callgrind ~10–20× slower. Profile ~15s of interaction."
   echo "Log: ${log}"
   timeout 60 valgrind --tool=callgrind --callgrind-out-file="${OUT}/callgrind.out" \
      --dump-instr=yes --collect-jumps=yes \
      env SCWX_VULKAN_PERF=1 "${BIN}" 2>&1 | tee "${log}" || true
   echo ""
   echo "GUI:  kcachegrind ${OUT}/callgrind.out"
   echo "CLI:  callgrind_annotate ${OUT}/callgrind.out | head -60"
}

cmd_renderdoc() {
   cat <<EOF
RenderDoc (Vulkan frame capture):

  1. qrenderdoc &
  2. Launch → Executable: ${BIN}
  3. Environment: SCWX_VULKAN_PERF=1
  4. Capture while panning / alerts visible
  5. Inspect: MapLibre passes, QRhi overlay, texture copies

EOF
}

cmd_sysprof() {
   cat <<EOF
Sysprof 6 (whole-app + GPU timeline):

  sysprof &
  → Record new application → ${BIN}
  → Environment: SCWX_VULKAN_PERF=1
  → Compare CPU samples vs GPU queue during 3×3 pan

EOF
}

case "${1:-check}" in
   check) cmd_check ;;
   print-install) print_install ;;
   builtin) cmd_builtin ;;
   cpu) cmd_cpu ;;
   flame) cmd_flame ;;
   callgrind) cmd_callgrind ;;
   renderdoc) cmd_renderdoc ;;
   sysprof) cmd_sysprof ;;
   *)
      echo "usage: $0 {check|print-install|builtin|cpu|flame|callgrind|renderdoc|sysprof} [seconds]" >&2
      exit 1
      ;;
esac
