#!/usr/bin/env bash
# Run clang-tidy-21 on changed lines vs origin/develop (matches PR review scope).
#
# Install (Fedora):
#   sudo dnf install clang21-tools-extra
#
# Requires compile_commands.json from a CMake build.
#
# Usage:
#   ./tools/check-clang-tidy.sh
#   ./tools/check-clang-tidy.sh fix
#   BUILD_DIR=build-local ./tools/check-clang-tidy.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

CLANG_TIDY="${CLANG_TIDY:-clang-tidy-21}"
CLANG_TIDY_DIFF="${CLANG_TIDY_DIFF:-/usr/lib64/llvm21/share/clang/clang-tidy-diff.py}"
BUILD_DIR="${BUILD_DIR:-build-local}"
BASE_REF="${BASE_REF:-origin/develop}"
JOBS="${JOBS:-4}"

if ! command -v "${CLANG_TIDY}" >/dev/null; then
   echo "Missing ${CLANG_TIDY}. Install LLVM 21 tools, e.g.:" >&2
   echo "  sudo dnf install clang21-tools-extra" >&2
   exit 1
fi

if [[ ! -f "${CLANG_TIDY_DIFF}" ]]; then
   echo "Missing ${CLANG_TIDY_DIFF}. Install clang21-tools-extra." >&2
   exit 1
fi

if [[ ! -f "${BUILD_DIR}/compile_commands.json" ]]; then
   echo "Missing ${BUILD_DIR}/compile_commands.json" >&2
   echo "Configure/build first, e.g.:" >&2
   echo "  ./tools/setup-linux-ninja-release.sh ${BUILD_DIR} scwx-linux_gcc-13" >&2
   exit 1
fi

git fetch origin develop
MERGE_BASE="$(git merge-base "${BASE_REF}" HEAD)"
echo "Checking tidy on C++ changes since ${MERGE_BASE} (${BASE_REF})"

FIX_ARGS=()
if [[ "${1:-check}" == "fix" ]]; then
   FIX_ARGS=(-fix)
fi

set +e
git diff -U0 "${MERGE_BASE}" -- \
   '*.cpp' '*.cxx' '*.cc' '*.c' \
   '*.hpp' '*.hh' '*.h' '*.ipp' '*.h++' '*.c++' \
   | rg -v '^(\+\+\+|---) external/' \
   | "${CLANG_TIDY_DIFF}" -p1 \
      -path "${BUILD_DIR}" \
      -clang-tidy-binary "${CLANG_TIDY}" \
      -config-file "${ROOT}/.clang-tidy" \
      -j "${JOBS}" \
      -extra-arg-before=-Wno-unknown-warning-option \
      "${FIX_ARGS[@]}"
STATUS=$?
set -e

if [[ ${STATUS} -ne 0 ]]; then
   echo "clang-tidy reported issues on changed lines (exit ${STATUS})." >&2
   echo "Some checks are informational; CI posts up to 25 PR comments and fails if any remain." >&2
   exit ${STATUS}
fi

echo "No clang-tidy issues on changed lines."
