#!/usr/bin/env bash
#
# publish-repo.sh — sign XMMS packages and publish signed apt/dnf/apk
# repositories to the TacitSoft S3 + CloudFront distribution.
#
# Consumes already-built packages (the .deb/.rpm/.apk produced by package.yml)
# from a staging directory, signs the repository metadata with the archive GPG
# key, lays out standard repo trees, syncs them to S3, and invalidates the
# CloudFront cache.
#
# This script is deployment-agnostic: every environment-specific value comes
# from the environment, so nothing is hard-coded. It is a no-op-safe dry run
# unless PUBLISH=1 is set.
#
# Required environment:
#   STAGING_DIR      Directory containing the input packages (recursively).
#   GPG_KEY_ID       Long key id / fingerprint of the imported archive key.
#   S3_BUCKET        Target bucket name (e.g. packages.tacitsoft.dev).
#   REPO_BASE_URL    Public base URL (e.g. https://packages.tacitsoft.dev).
# Optional:
#   CF_DISTRIBUTION_ID   CloudFront distribution to invalidate after sync.
#   APT_SUITE            apt suite name (default: stable).
#   APT_COMPONENT        apt component (default: main).
#   WORK_DIR             Build scratch dir (default: mktemp).
#   ABUILD_PRIVKEY       Path to abuild private key for Alpine signing.
#   PUBLISH              "1" to actually push to S3/CloudFront (default: dry run).
#
set -euo pipefail

# --- Config / validation ----------------------------------------------------
: "${STAGING_DIR:?set STAGING_DIR to the directory holding built packages}"
: "${GPG_KEY_ID:?set GPG_KEY_ID to the archive signing key id}"
: "${S3_BUCKET:?set S3_BUCKET to the target bucket}"
: "${REPO_BASE_URL:?set REPO_BASE_URL to the public repo URL}"

APT_SUITE="${APT_SUITE:-stable}"
APT_COMPONENT="${APT_COMPONENT:-main}"
PUBLISH="${PUBLISH:-0}"
WORK_DIR="${WORK_DIR:-$(mktemp -d)}"
REPO_OUT="${WORK_DIR}/repo"

log() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }

# Architecture mapping is x86_64/amd64 only for now (matches package.yml).
mkdir -p "${REPO_OUT}"

# Export the public key once — every package manager needs it to verify.
gpg --armor --export "${GPG_KEY_ID}" > "${REPO_OUT}/xmms-archive-keyring.asc"
log "Exported public key to xmms-archive-keyring.asc"

# --- APT (Debian/Ubuntu) ----------------------------------------------------
build_apt() {
    local debs
    debs=$(find "${STAGING_DIR}" -name '*.deb' | sort)
    [ -z "${debs}" ] && { log "apt: no .deb found — skipping"; return 0; }

    local aptroot="${REPO_OUT}/deb"
    mkdir -p "${aptroot}/conf"
    cat > "${aptroot}/conf/distributions" <<EOF
Origin: TacitSoft
Label: XMMS Resurrection
Suite: ${APT_SUITE}
Codename: ${APT_SUITE}
Architectures: amd64 source
Components: ${APT_COMPONENT}
Description: XMMS Resurrection package repository
SignWith: ${GPG_KEY_ID}
EOF
    local d
    while IFS= read -r d; do
        [ -n "${d}" ] || continue
        log "apt: including $(basename "${d}")"
        reprepro -b "${aptroot}" includedeb "${APT_SUITE}" "${d}"
    done <<< "${debs}"
    # reprepro keeps its db under the root; drop it from what we publish.
    rm -rf "${aptroot}/conf" "${aptroot}/db"
}

# --- DNF (Fedora/RHEL) ------------------------------------------------------
build_dnf() {
    local rpms
    rpms=$(find "${STAGING_DIR}" -name '*.rpm' ! -name '*.src.rpm' | sort)
    [ -z "${rpms}" ] && { log "dnf: no .rpm found — skipping"; return 0; }

    local rpmroot="${REPO_OUT}/rpm"
    mkdir -p "${rpmroot}"
    local r
    while IFS= read -r r; do
        [ -n "${r}" ] || continue
        cp -f "${r}" "${rpmroot}/"
    done <<< "${rpms}"

    createrepo_c --update "${rpmroot}"
    # Sign repomd.xml (dnf verifies this detached signature with repo_gpgcheck=1).
    gpg --batch --yes --local-user "${GPG_KEY_ID}" \
        --detach-sign --armor "${rpmroot}/repodata/repomd.xml"
    log "dnf: signed repodata/repomd.xml.asc"

    # Drop a ready-to-use .repo file for end users.
    cat > "${rpmroot}/xmms.repo" <<EOF
[xmms]
name=XMMS Resurrection
baseurl=${REPO_BASE_URL}/rpm
enabled=1
gpgcheck=1
repo_gpgcheck=1
gpgkey=${REPO_BASE_URL}/xmms-archive-keyring.asc
EOF
}

# --- APK (Alpine) -----------------------------------------------------------
build_apk() {
    local apks
    apks=$(find "${STAGING_DIR}" -name '*.apk' | sort)
    [ -z "${apks}" ] && { log "apk: no .apk found — skipping"; return 0; }
    if [ -z "${ABUILD_PRIVKEY:-}" ]; then
        log "apk: ABUILD_PRIVKEY unset — copying packages without an index"
    fi

    local apkroot="${REPO_OUT}/apk/x86_64"
    mkdir -p "${apkroot}"
    local a
    while IFS= read -r a; do
        [ -n "${a}" ] || continue
        cp -f "${a}" "${apkroot}/"
    done <<< "${apks}"

    if command -v apk >/dev/null 2>&1 && [ -n "${ABUILD_PRIVKEY:-}" ]; then
        ( cd "${apkroot}" && apk index -o APKINDEX.tar.gz ./*.apk )
        abuild-sign -k "${ABUILD_PRIVKEY}" "${apkroot}/APKINDEX.tar.gz"
        log "apk: built + signed APKINDEX.tar.gz"
    fi
}

build_apt
build_dnf
build_apk

# --- Publish ----------------------------------------------------------------
log "Repository tree built under ${REPO_OUT}"
find "${REPO_OUT}" -type f | sort | sed 's/^/    /'

if [ "${PUBLISH}" != "1" ]; then
    log "DRY RUN (PUBLISH != 1) — not syncing to S3. Set PUBLISH=1 to publish."
    exit 0
fi

log "Syncing to s3://${S3_BUCKET}/ ..."
aws s3 sync "${REPO_OUT}/" "s3://${S3_BUCKET}/" --delete --no-progress

if [ -n "${CF_DISTRIBUTION_ID:-}" ]; then
    log "Invalidating CloudFront ${CF_DISTRIBUTION_ID} ..."
    aws cloudfront create-invalidation \
        --distribution-id "${CF_DISTRIBUTION_ID}" \
        --paths '/*' >/dev/null
fi
log "Publish complete: ${REPO_BASE_URL}"
