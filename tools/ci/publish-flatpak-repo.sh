#!/usr/bin/env bash
# Build a dual-arch Flatpak OSTree repo site tree for GitHub Pages.
#
# Imports x86_64 + aarch64 bundles onto the given channel (nightly|stable):
# each bundle is unpacked in a scratch OSTree repo, then published into the
# live repo with build-commit-from so only the channel ref is updated. Mirrors
# any existing Pages repo first, then prunes and writes .flatpakrepo metadata.
set -euo pipefail

APP_ID="net.supercellwx.app"
CHANNEL=""
SITE_DIR=""
BUNDLE_X64=""
BUNDLE_AARCH64=""
PAGES_URL=""
HOMEPAGE_URL="https://supercellwx.net"
GPG_KEY_ID=""
MAX_SITE_BYTES=$((900 * 1024 * 1024))

usage() {
  cat <<'EOF'
Usage: publish-flatpak-repo.sh [options]

Required:
  --channel nightly|stable
  --site-dir DIR
  --bundle-x64 PATH
  --bundle-aarch64 PATH
  --pages-url URL          Base Pages URL (no trailing slash)
  --gpg-key-id ID          GPG key id/fingerprint for signing

Optional:
  --homepage URL           Default: https://supercellwx.net
  --max-site-bytes N       Fail if site exceeds N bytes (default: 900MiB)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --channel) CHANNEL="$2"; shift 2 ;;
    --site-dir) SITE_DIR="$2"; shift 2 ;;
    --bundle-x64) BUNDLE_X64="$2"; shift 2 ;;
    --bundle-aarch64) BUNDLE_AARCH64="$2"; shift 2 ;;
    --pages-url) PAGES_URL="${2%/}"; shift 2 ;;
    --homepage) HOMEPAGE_URL="$2"; shift 2 ;;
    --gpg-key-id) GPG_KEY_ID="$2"; shift 2 ;;
    --max-site-bytes) MAX_SITE_BYTES="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ -z "${CHANNEL}" || -z "${SITE_DIR}" || -z "${BUNDLE_X64}" || \
      -z "${BUNDLE_AARCH64}" || -z "${PAGES_URL}" || -z "${GPG_KEY_ID}" ]]; then
  echo "Missing required arguments." >&2
  usage >&2
  exit 2
fi

if [[ "${CHANNEL}" != "nightly" && "${CHANNEL}" != "stable" ]]; then
  echo "Channel must be 'nightly' or 'stable' (got '${CHANNEL}')." >&2
  exit 2
fi

for bundle in "${BUNDLE_X64}" "${BUNDLE_AARCH64}"; do
  if [[ ! -f "${bundle}" ]]; then
    echo "Bundle not found: ${bundle}" >&2
    exit 1
  fi
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INDEX_HTML="${SCRIPT_DIR}/flatpak-pages/index.html"
if [[ ! -f "${INDEX_HTML}" ]]; then
  echo "Missing index template: ${INDEX_HTML}" >&2
  exit 1
fi

REPO_DIR="${SITE_DIR}/repo"
REPO_URL="${PAGES_URL}/repo/"

echo "==> Preparing site directory: ${SITE_DIR}"
rm -rf "${SITE_DIR}"
mkdir -p "${SITE_DIR}"

restore_previous_repo() {
  echo "==> Checking for existing OSTree repo at ${REPO_URL}"
  local http_code curl_status=0
  http_code="$(curl -sS -o /dev/null -w "%{http_code}" -I "${PAGES_URL}/repo/config")" \
    || curl_status=$?

  if (( curl_status != 0 )); then
    echo "ERROR: Failed to reach ${PAGES_URL}/repo/config (curl exit ${curl_status})." >&2
    exit 1
  fi

  case "${http_code}" in
    404)
      echo "==> No existing Pages repo found (HTTP 404); initializing a fresh repo"
      ostree init --repo="${REPO_DIR}" --mode=archive-z2
      return 0
      ;;
    200|301|302|303|307|308)
      ;;
    *)
      echo "ERROR: Unexpected HTTP ${http_code} checking ${PAGES_URL}/repo/config" >&2
      exit 1
      ;;
  esac

  echo "==> Mirroring existing repo from Pages (GPG-verified with ${GPG_KEY_ID})"
  ostree init --repo="${REPO_DIR}" --mode=archive-z2
  # Verify commits with the same key we use for signing. Export to a keyring
  # file because ostree remote --gpg-import expects a path.
  local gpg_import
  gpg_import="$(mktemp)"
  gpg --batch --export "${GPG_KEY_ID}" > "${gpg_import}"
  if ! ostree remote add \
      --repo="${REPO_DIR}" \
      --gpg-import="${gpg_import}" \
      pages "${REPO_URL}"; then
    rm -f "${gpg_import}"
    echo "ERROR: Failed to add Pages OSTree remote ${REPO_URL}" >&2
    exit 1
  fi
  rm -f "${gpg_import}"

  if ! ostree pull --repo="${REPO_DIR}" --mirror pages; then
    echo "ERROR: Failed to mirror existing Flatpak repo from ${REPO_URL}" >&2
    exit 1
  fi
  ostree remote delete --repo="${REPO_DIR}" pages || true
}

restore_previous_repo

# Import a bundle into a disposable scratch repo, then publish only the channel
# ref into REPO_DIR via build-commit-from. That keeps temporary native refs
# (e.g. master, or nightly when publishing stable) out of the live repository
# so we never delete another channel's published ref.
import_bundle() {
  local arch="$1"
  local bundle="$2"
  local update_appstream="${3:-false}"
  local dest_ref="app/${APP_ID}/${arch}/${CHANNEL}"
  local scratch src_ref
  local appstream_args=()

  scratch="$(mktemp -d "${TMPDIR:-/tmp}/flatpak-scratch.XXXXXX")"
  # shellcheck disable=SC2064
  trap "rm -rf -- $(printf '%q' "${scratch}")" RETURN

  echo "==> Importing ${bundle} into scratch repo"
  ostree init --repo="${scratch}" --mode=archive-z2
  flatpak build-import-bundle \
    --no-update-summary \
    "${scratch}" \
    "${bundle}"

  src_ref="$(ostree refs --repo="${scratch}" \
    | grep "^app/${APP_ID}/${arch}/" \
    | head -n1 \
    || true)"
  if [[ -z "${src_ref}" ]]; then
    echo "Scratch import of ${bundle} produced no app/${APP_ID}/${arch}/* ref" >&2
    ostree refs --repo="${scratch}" >&2 || true
    return 1
  fi

  if [[ "${update_appstream}" == "true" ]]; then
    appstream_args+=(--update-appstream)
  fi

  echo "==> Publishing scratch ${src_ref} -> live ${dest_ref}"
  flatpak build-commit-from \
    --gpg-sign="${GPG_KEY_ID}" \
    --src-repo="${scratch}" \
    --src-ref="${src_ref}" \
    --no-update-summary \
    "${appstream_args[@]}" \
    "${REPO_DIR}" \
    "${dest_ref}"
}

import_bundle "x86_64" "${BUNDLE_X64}" false
import_bundle "aarch64" "${BUNDLE_AARCH64}" true

echo "==> Updating repository summary, deltas, and prune"
flatpak build-update-repo \
  --gpg-sign="${GPG_KEY_ID}" \
  --generate-static-deltas \
  --prune \
  --prune-depth=1 \
  "${REPO_DIR}"

echo "==> Writing public GPG key and .flatpakrepo files"
gpg --export --armor "${GPG_KEY_ID}" > "${SITE_DIR}/supercell-wx-flatpak.gpg"
GPG_KEY_BASE64="$(gpg --export "${GPG_KEY_ID}" | base64 -w0)"

write_flatpakrepo() {
  local path="$1"
  local title="$2"
  local default_branch="$3"
  cat > "${path}" <<EOF
[Flatpak Repo]
Title=${title}
Url=${REPO_URL}
Homepage=${HOMEPAGE_URL}
GPGKey=${GPG_KEY_BASE64}
DefaultBranch=${default_branch}
EOF
}

write_flatpakrepo \
  "${SITE_DIR}/supercell-wx.flatpakrepo" \
  "Supercell Wx" \
  "stable"
write_flatpakrepo \
  "${SITE_DIR}/supercell-wx-nightly.flatpakrepo" \
  "Supercell Wx Nightly" \
  "nightly"

cp "${INDEX_HTML}" "${SITE_DIR}/index.html"
# Actions Pages does not run Jekyll, but keep this for safety if hosting changes.
touch "${SITE_DIR}/.nojekyll"

SITE_BYTES="$(du -sb "${SITE_DIR}" | awk '{print $1}')"
echo "==> Site size: ${SITE_BYTES} bytes"
if [[ "${SITE_BYTES}" -gt "${MAX_SITE_BYTES}" ]]; then
  echo "ERROR: Flatpak Pages site is ${SITE_BYTES} bytes (limit ${MAX_SITE_BYTES})." >&2
  echo "Prune failed to keep size in check; see the VPS migration runbook." >&2
  exit 1
fi

echo "==> Refs in repo:"
ostree refs --repo="${REPO_DIR}" || true
echo "==> Flatpak Pages site ready at ${SITE_DIR}"
