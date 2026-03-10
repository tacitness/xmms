#!/usr/bin/env bash
# =============================================================================
# create-gh-labels.sh — Create GitHub issue labels for the XMMS project
#
# Usage:
#   gh auth login                     # authenticate once
#   ./scripts/create-gh-labels.sh     # creates labels in current repo
#   ./scripts/create-gh-labels.sh OWNER/REPO   # explicit repo
#
# Requires:  GitHub CLI (gh) — https://cli.github.com/
# =============================================================================

set -euo pipefail

REPO="${1:-}"
GH_ARGS=()
if [[ -n "$REPO" ]]; then
    GH_ARGS+=(--repo "$REPO")
fi

if ! command -v gh &>/dev/null; then
    echo "Error: GitHub CLI (gh) not found. Install from https://cli.github.com/" >&2
    exit 1
fi

gh auth status &>/dev/null || { echo "Error: Not authenticated. Run: gh auth login" >&2; exit 1; }

# Helper: create or update a label
label() {
    local name="$1" color="$2" description="$3"
    echo "  → $name"
    if gh label list "${GH_ARGS[@]}" --search "$name" --json name -q '.[0].name' 2>/dev/null | grep -qF "$name"; then
        gh label edit "$name" "${GH_ARGS[@]}" --color "$color" --description "$description" 2>/dev/null || true
    else
        gh label create "$name" "${GH_ARGS[@]}" --color "$color" --description "$description" --force 2>/dev/null || true
    fi
}

# ── Type labels ───────────────────────────────────────────────────────────────
echo "Creating TYPE labels..."
label "bug"              "d73a4a" "Something isn't working correctly"
label "feature"          "0075ca" "New feature or enhancement request"
label "refactor"         "e4e669" "Code restructuring without behaviour change"
label "docs"             "0052cc" "Documentation improvements"
label "question"         "d876e3" "Question or discussion"
label "security"         "b60205" "Security vulnerability or hardening"
label "chore"            "fef2c0" "Maintenance: deps, tooling, admin tasks"
label "test"             "bfd4f2" "Adding or fixing automated tests"

# ── Component labels ─────────────────────────────────────────────────────────
echo "Creating COMPONENT labels..."
label "comp:core"           "c2e0c6" "Core player engine"
label "comp:skin"           "c2e0c6" "Skin / theme rendering"
label "comp:playlist"       "c2e0c6" "Playlist management"
label "comp:equalizer"      "c2e0c6" "Equalizer subsystem"
label "comp:visualizer"     "c2e0c6" "Visualizations (spectrum, oscilloscope)"
label "comp:input-plugin"   "c2e0c6" "Input decoder plugins (mp3, ogg, wav, cd…)"
label "comp:output-plugin"  "c2e0c6" "Audio output plugins (ALSA, PulseAudio, OSS…)"
label "comp:effect-plugin"  "c2e0c6" "Effect/DSP plugins (EQ, echo, stereo…)"
label "comp:general-plugin" "c2e0c6" "General plugins (joystick, song_change…)"
label "comp:prefs"          "c2e0c6" "Preferences dialog"
label "comp:libxmms"        "c2e0c6" "Shared libxmms utility library"
label "comp:i18n"           "c2e0c6" "Internationalization / translations"
label "comp:build"          "c2e0c6" "Build system (Autotools / CMake / Meson)"
label "comp:ci-cd"          "c2e0c6" "CI/CD pipelines and automation"
label "comp:packaging"      "c2e0c6" "RPM, DEB, APK packaging"
label "comp:wayland"        "c2e0c6" "Wayland display server compatibility"

# ── GTK Migration labels ──────────────────────────────────────────────────────
echo "Creating GTK MIGRATION labels..."
label "gtk-migration"    "1d76db" "GTK version migration work"
label "gtk3"             "0e8a16" "Specific to GTK3 migration"
label "gtk4"             "006b75" "Specific to GTK4 migration"
label "compat-break"     "e11d48" "Introduces a compatibility break"
label "gtknative"        "5319e7" "Requires GTK-native Wayland/X11 abstraction"

# ── Platform labels ───────────────────────────────────────────────────────────
echo "Creating PLATFORM labels..."
label "platform:linux"         "ededed" "Generic Linux issue"
label "platform:fedora"        "3c1ef7" "Fedora / RHEL / CentOS Stream"
label "platform:debian-ubuntu" "e96c50" "Debian / Ubuntu / Mint"
label "platform:arch"          "1793d1" "Arch Linux / Manjaro"
label "platform:alpine"        "0d8e8e" "Alpine Linux"
label "platform:wayland"       "7057ff" "Wayland compositor compatibility"
label "platform:x11"           "ffa500" "X11 / Xorg compatibility"

# ── Priority labels ───────────────────────────────────────────────────────────
echo "Creating PRIORITY labels..."
label "P0-critical"  "b60205" "Critical — blocks release or data loss"
label "P1-high"      "e4e669" "High priority — should be fixed this cycle"
label "P2-medium"    "fef2c0" "Medium priority — planned work"
label "P3-low"       "c2e0c6" "Low priority — nice to have"

# ── Status labels ─────────────────────────────────────────────────────────────
echo "Creating STATUS labels..."
label "needs-triage"  "ededed" "Not yet triaged or categorised"
label "confirmed"     "0e8a16" "Confirmed bug or accepted feature"
label "in-progress"   "0075ca" "Actively being worked on"
label "blocked"       "d73a4a" "Blocked on external dependency or decision"
label "wontfix"       "ffffff" "Will not be fixed / implemented"
label "duplicate"     "cfd3d7" "Duplicate of another issue"
label "ready-for-review" "0e8a16" "PR / change is ready for code review"
label "needs-more-info"  "d4c5f9" "Requires more information from reporter"

# ── Effort labels ─────────────────────────────────────────────────────────────
echo "Creating EFFORT labels..."
label "effort-small"   "e6f0ff" "< 1 day of work"
label "effort-medium"  "bde0ff" "1–3 days of work"
label "effort-large"   "7abaff" "1–2 weeks of work"
label "effort-xlarge"  "4099ff" "Multi-week or milestone-level effort"

# ── Good first issue / help labels ────────────────────────────────────────────
echo "Creating COMMUNITY labels..."
label "good first issue" "7057ff" "Good for newcomers — well scoped"
label "help wanted"      "008672" "Open to contributions"
label "hacktoberfest"    "ff6600" "Eligible for Hacktoberfest contributions"

echo ""
echo "✅  All labels created/updated successfully!"
