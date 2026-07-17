#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GLSL_DIR="${ROOT}/scwx-qt/gl/vulkan"
QSB_DIR="${GLSL_DIR}/qsb"
QSB="${QSB:-qsb}"

if ! command -v "${QSB}" >/dev/null 2>&1; then
   for candidate in \
      "/usr/lib64/qt6/bin/qsb" \
      "/usr/lib/qt6/bin/qsb" \
      "${QT_ROOT_DIR:-}/bin/qsb" \
      "${QTDIR:-}/bin/qsb"; do
      if [[ -x "${candidate}" ]]; then
         QSB="${candidate}"
         break
      fi
   done
fi

if ! command -v "${QSB}" >/dev/null 2>&1 && [[ ! -x "${QSB}" ]]; then
   echo "error: qsb not found (install Qt Shader Tools / set QSB=)" >&2
   exit 1
fi

mkdir -p "${QSB_DIR}"

# Emit SPIR-V (always) + MSL 1.2 for QRhi Metal. One GLSL source → one .qsb.
for shader in "${GLSL_DIR}"/*.vert "${GLSL_DIR}"/*.frag; do
   [[ -f "${shader}" ]] || continue
   base="$(basename "${shader}")"
   out="${QSB_DIR}/${base}.qsb"
   "${QSB}" --msl 12 -o "${out}" "${shader}"
   echo "compiled ${base}.qsb"
done
