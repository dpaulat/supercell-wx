#!/usr/bin/env bash
# Mirror .github/workflows/clang-format-check.yml locally.
#
# Install (Fedora):
#   sudo dnf install clang21-tools-extra git-clang-format19
#
# Usage:
#   ./tools/check-clang-format.sh          # diff vs origin/develop (CI check)
#   ./tools/check-clang-format.sh fix      # apply formatting to changed lines
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

CLANG_FORMAT="${CLANG_FORMAT:-clang-format-21}"
GIT_CLANG_FORMAT="${GIT_CLANG_FORMAT:-git-clang-format-19}"
BASE_REF="${BASE_REF:-origin/develop}"

if ! command -v "${CLANG_FORMAT}" >/dev/null; then
   echo "Missing ${CLANG_FORMAT}. Install LLVM 21 tools, e.g.:" >&2
   echo "  sudo dnf install clang21-tools-extra" >&2
   exit 1
fi

if ! command -v "${GIT_CLANG_FORMAT}" >/dev/null; then
   echo "Missing ${GIT_CLANG_FORMAT}. Install git integration, e.g.:" >&2
   echo "  sudo dnf install git-clang-format19" >&2
   exit 1
fi

git fetch origin develop
MERGE_BASE="$(git merge-base "${BASE_REF}" HEAD)"
echo "Comparing against ${MERGE_BASE} (${BASE_REF})"

if [[ "${1:-check}" == "fix" ]]; then
   "${GIT_CLANG_FORMAT}" --binary "${CLANG_FORMAT}" --style=file --force \
      --extensions "c,h,C,H,cpp,hpp,cc,hh,c++,h++,cxx,hxx" \
      "${MERGE_BASE}"
   echo "Applied clang-format to lines changed since ${MERGE_BASE}"
else
   "${GIT_CLANG_FORMAT}" --binary "${CLANG_FORMAT}" --style=file --diff -v \
      --extensions "c,h,C,H,cpp,hpp,cc,hh,c++,h++,cxx,hxx" \
      "${MERGE_BASE}"
fi
