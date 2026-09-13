#!/usr/bin/env bash
# Quick Vulkan backend smoke check — run locally before asking user to test.
set -euo pipefail

for cmd in timeout rg python3; do
   if ! command -v "${cmd}" >/dev/null 2>&1; then
      echo "Missing required command: ${cmd}" >&2
      exit 1
   fi
done

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT}/build-local/Release/bin/supercell-wx"
LOG="$(mktemp /tmp/scwx-vulkan-smoke.XXXXXX.log)"
CAPTURE="$(mktemp /tmp/scwx-vulkan-smoke.XXXXXX.png)"
export SCWX_VULKAN_SMOKE=1
export SCWX_VULKAN_SMOKE_CAPTURE="${CAPTURE}"
export SCWX_VULKAN_SMOKE_CAPTURE_FRAMES="${SCWX_VULKAN_SMOKE_CAPTURE_FRAMES:-60}"

if [[ ! -x "${BIN}" ]]; then
   echo "Missing binary: ${BIN}" >&2
   exit 1
fi

echo "Running ${BIN} for 45s (log: ${LOG}, capture: ${CAPTURE})"
set +e
timeout 45 "${BIN}" >"${LOG}" 2>&1
EXIT=$?
set -e

echo "--- results ---"
if [[ ${EXIT} -eq 0 ]]; then
   echo "OK: app exited after smoke capture"
elif [[ ${EXIT} -eq 124 ]]; then
   echo "FAIL: timed out before smoke capture" >&2
   rg -n "Vulkan smoke capture|Vulkan radar draw|Vulkan overlay smoke|error|Failed" "${LOG}" | tail -40 || true
   exit 1
elif [[ ${EXIT} -eq 139 || ${EXIT} -eq 134 ]]; then
   echo "FAIL: crashed (exit ${EXIT})" >&2
   rg -n "SIGSEGV|error|ImGui_ImplVulkan" "${LOG}" | tail -20 || true
   exit 1
else
   echo "Exit code: ${EXIT}"
fi

REINIT=$(rg -c "ImGui Vulkan renderer reinitialized" "${LOG}" || true)
INIT=$(rg -c "ImGui Vulkan renderer initialized" "${LOG}" || true)
RADAR=$(rg -c "Vulkan radar draw:" "${LOG}" || true)
SMOKE=$(rg -c "Vulkan overlay smoke:" "${LOG}" || true)
SWEEP=$(rg -c "UpdateSweep" "${LOG}" || true)

echo "ImGui init: ${INIT}, reinit: ${REINIT}"
echo "ImGui smoke lines: ${SMOKE}, UpdateSweep: ${SWEEP}, radar draw: ${RADAR}"

if [[ "${REINIT}" -gt 4 ]]; then
   echo "WARN: excessive ImGui reinit (${REINIT})" >&2
fi

if [[ "${SMOKE}" -eq 0 ]]; then
   echo "No overlay smoke logs before capture"
else
   rg "Vulkan overlay smoke:" "${LOG}" | tail -3
fi

if [[ "${RADAR}" -eq 0 ]]; then
   echo "No radar draw logs before capture"
else
   rg "Vulkan radar draw:" "${LOG}" | tail -3
fi

if rg -q "Vulkan perf:" "${LOG}"; then
   rg "Vulkan perf:" "${LOG}" | tail -5
fi

if rg -q "ImGui_ImplVulkan_Init failed|Vulkan call failed" "${LOG}"; then
   echo "FAIL: ImGui Vulkan errors in log" >&2
   rg "ImGui_ImplVulkan_Init failed|Vulkan call failed" "${LOG}" | tail -5
   exit 1
fi

# Multi-pane smoke writes CAPTURE.paneN.png; single-pane may write CAPTURE.
CAPTURES=()
shopt -s nullglob
for f in "${CAPTURE%.png}".pane*.png "${CAPTURE}"; do
   [[ -s "${f}" ]] && CAPTURES+=("${f}")
done
shopt -u nullglob

if [[ ${#CAPTURES[@]} -eq 0 ]]; then
   echo "FAIL: smoke capture missing or empty: ${CAPTURE}" >&2
   rg -n "Vulkan smoke capture|error|Failed" "${LOG}" | tail -20 || true
   exit 1
fi

FAIL=0
for f in "${CAPTURES[@]}"; do
   echo "Analyzing ${f}"
   python3 "${ROOT}/tools/analyze-vulkan-capture.py" "${f}" || FAIL=1
done
if [[ ${FAIL} -ne 0 ]]; then
   echo "Capture(s) kept for inspection under ${CAPTURE%/*}" >&2
   exit 1
fi

echo "Smoke check complete. Full log: ${LOG}"
