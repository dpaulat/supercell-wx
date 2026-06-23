#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GLSL_DIR="${ROOT}/scwx-qt/gl/vulkan"
SPIRV_DIR="${GLSL_DIR}/spirv"
COMPILER="${GLSLANG:-glslangValidator}"

if ! command -v "${COMPILER}" >/dev/null 2>&1; then
   echo "error: ${COMPILER} not found (install glslang package)" >&2
   exit 1
fi

mkdir -p "${SPIRV_DIR}"

for shader in "${GLSL_DIR}"/*.vert "${GLSL_DIR}"/*.frag; do
   [[ -f "${shader}" ]] || continue
   base="$(basename "${shader}")"
   name="${base%.*}"
   "${COMPILER}" -V "${shader}" -o "${SPIRV_DIR}/${name}.spv"
   echo "compiled ${name}.spv"
done
