#!/usr/bin/env bash
# Build a dual-arch Flatpak OSTree repo site tree for GitHub Pages.
#
# Imports x86_64 + aarch64 bundles onto the given channel (nightly|stable),
# mirrors any existing Pages repo first, then prunes and writes .flatpakrepo
# metadata.
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
  if curl -fsI "${PAGES_URL}/repo/config" >/dev/null 2>&1; then
    echo "==> Mirroring existing repo from Pages (GPG-verified with ${GPG_KEY_ID})"
    ostree init --repo="${REPO_DIR}" --mode=archive-z2
    # Verify commits with the same key we use for signing. Export to a keyring
    # file because ostree remote --gpg-import expects a path.
    local gpg_import
    gpg_import="$(mktemp)"
    gpg --batch --export "${GPG_KEY_ID}" > "${gpg_import}"
    ostree remote add \
      --repo="${REPO_DIR}" \
      --gpg-import="${gpg_import}" \
      pages "${REPO_URL}"
    rm -f "${gpg_import}"
    if ostree pull --repo="${REPO_DIR}" --mirror pages; then
      ostree remote delete --repo="${REPO_DIR}" pages || true
      return 0
    fi
    echo "Warning: mirror pull failed; initializing a fresh repo" >&2
    rm -rf "${REPO_DIR}"
  else
    echo "==> No existing Pages repo found"
  fi
  ostree init --repo="${REPO_DIR}" --mode=archive-z2
}

restore_previous_repo

# Return the xa.ref binding embedded in a commit (e.g. app/id/arch/master).
commit_bound_ref() {
  local commit="$1"
  ostree show --repo="${REPO_DIR}" --print-metadata-key=xa.ref "${commit}" 2>/dev/null \
    | tr -d "'\"" \
    | tr -d '\n' \
    || true
}

import_bundle() {
  local arch="$1"
  local bundle="$2"
  local update_appstream="${3:-false}"
  local dest_ref="app/${APP_ID}/${arch}/${CHANNEL}"
  local src_ref=""
  local ref rev bound
  local -A revs_before
  revs_before=()

  while IFS= read -r ref; do
    [[ -z "${ref}" ]] && continue
    revs_before["${ref}"]="$(ostree rev-parse --repo="${REPO_DIR}" "${ref}")"
  done < <(ostree refs --repo="${REPO_DIR}" | grep "^app/${APP_ID}/${arch}/" || true)

  echo "==> Importing ${bundle} at its native branch (not rewriting xa.ref yet)"
  flatpak build-import-bundle \
    --gpg-sign="${GPG_KEY_ID}" \
    --no-update-summary \
    "${REPO_DIR}" \
    "${bundle}"

  # The native bundle branch is the ref whose commit changed (or appeared) and
  # whose xa.ref binding matches the ref name. Using --ref= on import only moves
  # the ref pointer and leaves xa.ref as master, which clients reject.
  while IFS= read -r ref; do
    [[ -z "${ref}" ]] && continue
    rev="$(ostree rev-parse --repo="${REPO_DIR}" "${ref}")"
    if [[ -z "${revs_before[${ref}]:-}" || "${revs_before[${ref}]}" != "${rev}" ]]; then
      bound="$(commit_bound_ref "${rev}")"
      if [[ "${bound}" == "${ref}" ]]; then
        src_ref="${ref}"
        break
      fi
      # Keep a fallback if xa.ref is missing/unexpected.
      if [[ -z "${src_ref}" ]]; then
        src_ref="${ref}"
      fi
    fi
  done < <(ostree refs --repo="${REPO_DIR}" | grep "^app/${APP_ID}/${arch}/" || true)

  if [[ -z "${src_ref}" ]]; then
    echo "Import of ${bundle} did not update any app/${APP_ID}/${arch}/* refs" >&2
    ostree refs --repo="${REPO_DIR}" >&2 || true
    exit 1
  fi

  if [[ "${src_ref}" != "${dest_ref}" ]]; then
    echo "==> Rebinding ${src_ref} -> ${dest_ref} (new commit with matching xa.ref)"
    local appstream_args=()
    if [[ "${update_appstream}" == "true" ]]; then
      appstream_args+=(--update-appstream)
    fi
    flatpak build-commit-from \
      --gpg-sign="${GPG_KEY_ID}" \
      --src-ref="${src_ref}" \
      --no-update-summary \
      "${appstream_args[@]}" \
      "${REPO_DIR}" \
      "${dest_ref}"
    echo "==> Deleting temporary ref ${src_ref}"
    ostree refs --repo="${REPO_DIR}" --delete "${src_ref}"
  else
    echo "==> Bundle already on ${dest_ref} with matching ref bindings"
    if [[ "${update_appstream}" == "true" ]]; then
      # Refresh appstream from current heads without changing the app commit.
      flatpak build-commit-from \
        --gpg-sign="${GPG_KEY_ID}" \
        --src-ref="${dest_ref}" \
        --update-appstream \
        --no-update-summary \
        "${REPO_DIR}" \
        "${dest_ref}"
    fi
  fi
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
gpg --export --armor "${GPG_KEY_ID}" > "${SITE_DIR}/supercell-wx.gpg"
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
