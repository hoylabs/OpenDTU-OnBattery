#!/usr/bin/env bash
#
# Setup script for a Claude Code cloud environment working on OpenDTU-OnBattery.
#
# Paste this into the environment's "Setup script" field (or call it from there).
# It mirrors what the CI workflows install (.github/workflows/*.yml) so that a
# session can:
#   - run the host unit tests          (cd test && make test)
#   - lint C++                         (cpplint, same filters as cpplint.yml)
#   - install/build/lint the webapp    (cd webapp && yarn build / yarn lint)
#   - compile the firmware             (pio run -e <env>)
#
# Required network access (beyond the defaults): PlatformIO's registry,
# i.e. api.registry.platformio.org and dl.registry.platformio.org
# (allowing *.platformio.org is simplest). github.com is needed for the git
# based lib_deps (espMqttClient, MCP_CAN_lib).
#
# The script is idempotent. It aborts with a non-zero exit code if anything
# required to compile the firmware (PlatformIO, its registry, the toolchains)
# is unavailable, so a misconfigured environment is noticed immediately.

set -uo pipefail

log()  { printf '\n==> %s\n' "$*"; }
warn() { printf 'WARNING: %s\n' "$*" >&2; }
die()  { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

# PlatformIO environments to pre-fetch toolchains/libraries for. The default
# env (ESP32-S3) plus a classic ESP32 env covers both xtensa toolchains used
# by the CI build matrix.
PIO_PREFETCH_ENVS="${PIO_PREFETCH_ENVS:-generic_esp32s3_usb generic_esp32_8mb}"

export PLATFORMIO_SETTING_ENABLE_TELEMETRY=No
export PLATFORMIO_SETTING_CHECK_PLATFORMIO_INTERVAL=9999

# --- locate the repository -------------------------------------------------
REPO_DIR=""
for candidate in "${CLAUDE_PROJECT_DIR:-}" "$PWD" /home/user/OpenDTU-OnBattery /home/user/*; do
    if [ -n "$candidate" ] && [ -f "$candidate/platformio.ini" ] && [ -d "$candidate/webapp" ]; then
        REPO_DIR="$candidate"
        break
    fi
done
[ -n "$REPO_DIR" ] || die "OpenDTU-OnBattery repository not found (set CLAUDE_PROJECT_DIR)"
log "Repository: $REPO_DIR"

# --- system packages ---------------------------------------------------------
# build-essential (g++/make) for the host unit tests; jq is used by the CI
# to list the build matrix. Usually preinstalled, only install if missing.
missing_pkgs=""
for bin_pkg in g++:build-essential make:build-essential jq:jq git:git; do
    command -v "${bin_pkg%%:*}" >/dev/null 2>&1 || missing_pkgs="$missing_pkgs ${bin_pkg##*:}"
done
if [ -n "$missing_pkgs" ]; then
    log "Installing system packages:$missing_pkgs"
    SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"
    $SUDO apt-get update -qq && $SUDO DEBIAN_FRONTEND=noninteractive apt-get install -y -qq $missing_pkgs \
        || die "apt-get install of$missing_pkgs failed"
fi

# --- python tooling: PlatformIO, cpplint, dulwich ---------------------------
# dulwich is pip-installed on the fly by pio-scripts/auto_firmware_version.py;
# installing it up front avoids that during the first build.
log "Installing PlatformIO, cpplint and dulwich"
python3 -m pip install --quiet --upgrade --root-user-action=ignore \
        platformio setuptools cpplint dulwich 2>/dev/null \
    || python3 -m pip install --quiet --upgrade --break-system-packages \
        platformio setuptools cpplint dulwich \
    || die "pip install of platformio/cpplint/dulwich failed"
command -v pio >/dev/null 2>&1 || die "pio not on PATH after installing platformio"

pio settings set enable_telemetry No >/dev/null 2>&1 || true
pio settings set check_platformio_interval 9999 >/dev/null 2>&1 || true

# --- node / yarn (webapp) ----------------------------------------------------
# CI uses Node 24; Node >= 22.12 works as well (vite requirement). Yarn 1 is
# provided through corepack according to webapp/package.json "packageManager".
if command -v node >/dev/null 2>&1; then
    log "Node $(node --version) found"
    node_major="$(node -p 'process.versions.node.split(".")[0]')"
    [ "$node_major" -ge 22 ] || die "Node $node_major is too old for the webapp toolchain (need >= 22.12)"
else
    die "node is not installed; the webapp (embedded in the firmware) cannot be built"
fi
command -v corepack >/dev/null 2>&1 && { corepack enable || die "corepack enable failed"; }
export COREPACK_ENABLE_DOWNLOAD_PROMPT=0

command -v yarn >/dev/null 2>&1 || die "yarn not available (corepack missing?)"
log "Installing webapp dependencies"
(cd "$REPO_DIR/webapp" && yarn --version >/dev/null && yarn install --frozen-lockfile --non-interactive) \
    || die "yarn install failed"

# --- PlatformIO platform, toolchains and libraries ---------------------------
# This is the slow part (~1 GB download) and needs the PlatformIO registry,
# without which the firmware cannot be compiled. Fail fast with a clear message
# instead of waiting for pio's retries to time out.
for host in api.registry.platformio.org dl.registry.platformio.org; do
    curl -sS -o /dev/null --max-time 20 "https://$host/" \
        || die "cannot reach $host; allow *.platformio.org in the environment's network access settings"
done
for env in $PIO_PREFETCH_ENVS; do
    log "Pre-fetching PlatformIO packages for env '$env'"
    (cd "$REPO_DIR" && pio pkg install -e "$env") \
        || die "pio pkg install -e $env failed"
done

# --- summary ----------------------------------------------------------------
log "Tool versions"
for cmd in "node --version" "yarn --version" "python3 --version" "pio --version" "cpplint --version" "g++ --version"; do
    printf '%-20s %s\n' "${cmd%% *}" "$( (cd "$REPO_DIR/webapp" 2>/dev/null || true; $cmd 2>/dev/null | head -1) || echo 'missing')"
done

cat <<'EOF'

Common commands (from the repository root):
  cd test && make test                         # host unit tests (as in test.yml)
  cd webapp && yarn build && yarn lint         # webapp build + lint (as in build.yml/yarnlint.yml)
  pio run -e generic_esp32s3_usb               # firmware build (default env)
  cpplint --repository=. --recursive \
    --filter=-build/c++11,-runtime/references,-readability/braces,-whitespace,-legal,-build/include \
    ./src ./include ./lib/Hoymiles ./lib/MqttSubscribeParser ./lib/TimeoutHelper ./lib/ResetReason
EOF

exit 0
