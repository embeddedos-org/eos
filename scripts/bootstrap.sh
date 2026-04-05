#!/bin/bash
# SPDX-License-Identifier: MIT
# EoS Bootstrap — Clone-time dependency setup
#
# Run once after cloning the eos repo:
#   ./scripts/bootstrap.sh
#
# This script:
#   1. Clones eboot and ebuild into ~/.eos/repos/
#   2. Installs the ebuild Python CLI (optional)
#   3. Verifies the environment is ready for building
#
# Options:
#   --no-ebuild-cli    Skip Python ebuild CLI installation
#   --deps-dir <path>  Override dependency cache directory
#   --shallow          Use shallow clones (default)
#   --full             Use full clones (for development)
#   --update           Update existing repos (git pull)
#   --clean            Remove cached repos and re-clone
#   --help             Show this help

set -euo pipefail

# ====================================================================
# Configuration
# ====================================================================
EBOOT_REPO="https://github.com/AethOS-EoS/eBoot.git"
EBOOT_BRANCH="main"
EBUILD_REPO="https://github.com/AethOS-EoS/ebuild.git"
EBUILD_BRANCH="main"

# Default cache directory
if [ -n "${HOME:-}" ]; then
    DEPS_DIR="${HOME}/.eos/repos"
elif [ -n "${USERPROFILE:-}" ]; then
    DEPS_DIR="${USERPROFILE}/.eos/repos"
else
    DEPS_DIR="/tmp/.eos/repos"
fi

INSTALL_CLI=true
SHALLOW=true
UPDATE=false
CLEAN=false

# ====================================================================
# Parse arguments
# ====================================================================
while [ $# -gt 0 ]; do
    case "$1" in
        --no-ebuild-cli)
            INSTALL_CLI=false
            shift
            ;;
        --deps-dir)
            DEPS_DIR="$2"
            shift 2
            ;;
        --shallow)
            SHALLOW=true
            shift
            ;;
        --full)
            SHALLOW=false
            shift
            ;;
        --update)
            UPDATE=true
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --help|-h)
            head -20 "$0" | tail -14
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# ====================================================================
# Colors
# ====================================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info()  { echo -e "${BLUE}[eos]${NC} $*"; }
ok()    { echo -e "${GREEN}[eos]${NC} $*"; }
warn()  { echo -e "${YELLOW}[eos]${NC} $*"; }
err()   { echo -e "${RED}[eos]${NC} $*" >&2; }

# ====================================================================
# Detect script location (eos repo root)
# ====================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EOS_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

info "EoS Bootstrap"
info "  EoS repo:  ${EOS_ROOT}"
info "  Deps dir:  ${DEPS_DIR}"
echo ""

# ====================================================================
# Clean if requested
# ====================================================================
if [ "$CLEAN" = true ]; then
    warn "Cleaning cached repos..."
    rm -rf "${DEPS_DIR}/eboot" "${DEPS_DIR}/ebuild"
fi

# ====================================================================
# Clone/update helper
# ====================================================================
clone_or_update() {
    local name="$1"
    local repo="$2"
    local branch="$3"
    local dest="${DEPS_DIR}/${name}"

    if [ -d "${dest}/.git" ]; then
        if [ "$UPDATE" = true ]; then
            info "Updating ${name}..."
            (cd "${dest}" && git fetch origin && git checkout "${branch}" && git pull origin "${branch}")
            ok "${name} updated"
        else
            ok "${name} already cloned: ${dest}"
        fi
    else
        info "Cloning ${name}..."
        mkdir -p "${DEPS_DIR}"
        local clone_args=("clone")
        if [ "$SHALLOW" = true ]; then
            clone_args+=("--depth" "1")
        fi
        clone_args+=("-b" "${branch}" "${repo}" "${dest}")
        git "${clone_args[@]}"
        ok "${name} cloned: ${dest}"
    fi
}

# ====================================================================
# Clone dependencies
# ====================================================================
info "--- Cloning dependencies ---"
clone_or_update "eboot"  "${EBOOT_REPO}"  "${EBOOT_BRANCH}"
clone_or_update "ebuild" "${EBUILD_REPO}" "${EBUILD_BRANCH}"
echo ""

# ====================================================================
# Install ebuild Python CLI
# ====================================================================
if [ "$INSTALL_CLI" = true ]; then
    info "--- Installing ebuild CLI ---"
    if command -v pip3 &>/dev/null; then
        pip3 install -e "${DEPS_DIR}/ebuild" --quiet 2>/dev/null && \
            ok "ebuild CLI installed (pip3 install -e)" || \
            warn "ebuild CLI install failed (non-fatal, pip3 error)"
    elif command -v pip &>/dev/null; then
        pip install -e "${DEPS_DIR}/ebuild" --quiet 2>/dev/null && \
            ok "ebuild CLI installed (pip install -e)" || \
            warn "ebuild CLI install failed (non-fatal, pip error)"
    else
        warn "pip not found — skipping ebuild CLI install"
        warn "  Install manually: pip install -e ${DEPS_DIR}/ebuild"
    fi
    echo ""
fi

# ====================================================================
# Verify
# ====================================================================
info "--- Verification ---"
PASS=true

if [ -f "${DEPS_DIR}/eboot/CMakeLists.txt" ]; then
    ok "eBoot:  OK (${DEPS_DIR}/eboot)"
else
    err "eBoot:  MISSING"
    PASS=false
fi

if [ -f "${DEPS_DIR}/ebuild/CMakeLists.txt" ]; then
    ok "ebuild: OK (${DEPS_DIR}/ebuild)"
else
    err "ebuild: MISSING"
    PASS=false
fi

if [ "$INSTALL_CLI" = true ] && command -v ebuild &>/dev/null; then
    ok "ebuild CLI: $(ebuild --version 2>/dev/null || echo 'installed')"
else
    warn "ebuild CLI: not in PATH (optional)"
fi

if command -v cmake &>/dev/null; then
    ok "cmake: $(cmake --version | head -1)"
else
    err "cmake: NOT FOUND (required)"
    PASS=false
fi

echo ""

# ====================================================================
# Done
# ====================================================================
if [ "$PASS" = true ]; then
    ok "=========================================="
    ok " EoS bootstrap complete!"
    ok "=========================================="
    echo ""
    info "Build the entire platform:"
    echo "  cd ${EOS_ROOT}"
    echo "  cmake -B build"
    echo "  cmake --build build"
    echo ""
    info "Or with ebuild CLI:"
    echo "  cd ${EOS_ROOT}"
    echo "  ebuild build"
    echo ""
    info "Build options:"
    echo "  cmake -B build -DEOS_WITH_EBOOT=ON          # Include bootloader (default)"
    echo "  cmake -B build -DEOS_WITH_EBUILD=ON         # Include ebuild layers"
    echo "  cmake -B build -DEOS_BUILD_TESTS=ON         # Build tests"
    echo "  cmake -B build -DEOS_BOARD=stm32f4          # Target board"
    echo "  cmake -B build -DEOS_PRODUCT=robot           # Product profile"
    echo ""
else
    err "Bootstrap incomplete — see errors above"
    exit 1
fi
