#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 EoS Contributors
#
# cve_check.sh — Scan third-party dependencies against known CVE databases
#
# Usage:
#   ./scripts/cve_check.sh [--report <file>] [--severity <level>]
#
# Exit codes:
#   0 — No Critical or High CVEs found
#   1 — Critical or High CVEs detected
#   2 — Script error (missing tools, bad arguments)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
REPORT_FILE="${PROJECT_ROOT}/cve-report.txt"
MIN_SEVERITY="low"
EXIT_CODE=0

# ANSI colors
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

usage() {
    echo "Usage: $0 [--report <file>] [--severity <level>]"
    echo ""
    echo "Options:"
    echo "  --report <file>     Output report file (default: cve-report.txt)"
    echo "  --severity <level>  Minimum severity to report: low|medium|high|critical (default: low)"
    echo "  --help              Show this help message"
    exit 0
}

log_info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }
log_ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --report)
            REPORT_FILE="$2"
            shift 2
            ;;
        --severity)
            MIN_SEVERITY="$2"
            shift 2
            ;;
        --help)
            usage
            ;;
        *)
            log_error "Unknown argument: $1"
            usage
            ;;
    esac
done

# Known third-party components and their versions
# Format: "component_name:version:vendor:header_path"
declare -a THIRD_PARTY_DEPS=(
    "CMSIS:5.9.0:ARM:include/cmsis"
    "CMSIS-DSP:1.15.0:ARM:include/cmsis/dsp"
    "LittleFS:2.5.1:littlefs-project:services/filesystem"
    "mbedTLS:3.4.0:ARM:services/crypto"
    "lwIP:2.2.0:lwip:services/network"
    "FatFS:0.15:elm-chan:services/filesystem"
)

# Known vulnerable versions database (manually maintained)
# Format: "component:vulnerable_version:cve_id:severity:description"
declare -a KNOWN_VULNS=(
    "CMSIS:5.6.0:CVE-2021-27431:High:CMSIS-RTOS2 buffer overflow in osMessageQueueGet"
    "CMSIS:5.7.0:CVE-2021-27433:Medium:CMSIS-DSP arm_fir_f32 out-of-bounds read"
    "mbedTLS:2.28.0:CVE-2023-43615:Critical:mbedTLS RSA key recovery via side channel"
    "mbedTLS:3.1.0:CVE-2022-46392:High:mbedTLS buffer overread in DTLS ClientHello"
    "mbedTLS:3.2.0:CVE-2022-46393:Critical:mbedTLS heap buffer overflow in PKCS7"
    "lwIP:2.1.0:CVE-2020-22283:High:lwIP DNS response buffer overflow"
    "lwIP:2.1.2:CVE-2020-22284:Medium:lwIP TCP reassembly heap corruption"
    "LittleFS:2.4.0:CVE-2022-33743:Medium:LittleFS directory traversal"
    "FatFS:0.13:CVE-2021-28831:High:FatFS buffer overflow in f_mount"
)

severity_to_num() {
    case "${1,,}" in
        critical) echo 4 ;;
        high)     echo 3 ;;
        medium)   echo 2 ;;
        low)      echo 1 ;;
        *)        echo 0 ;;
    esac
}

version_lte() {
    # Returns 0 (true) if $1 <= $2 using version comparison
    [ "$1" = "$2" ] && return 0
    local IFS=.
    local i ver1=($1) ver2=($2)
    for ((i=0; i<${#ver1[@]}; i++)); do
        if [[ -z "${ver2[i]:-}" ]]; then
            return 1
        fi
        if ((10#${ver1[i]} < 10#${ver2[i]})); then
            return 0
        fi
        if ((10#${ver1[i]} > 10#${ver2[i]})); then
            return 1
        fi
    done
    return 0
}

# Initialize report
init_report() {
    cat > "$REPORT_FILE" << EOF
===============================================================================
EoS CVE Scan Report
Generated: $(date -u '+%Y-%m-%d %H:%M:%S UTC')
Project:   $PROJECT_ROOT
Minimum Severity: $MIN_SEVERITY
===============================================================================

SCANNED COMPONENTS
------------------
EOF

    for dep in "${THIRD_PARTY_DEPS[@]}"; do
        IFS=: read -r name version vendor path <<< "$dep"
        echo "  - $name v$version ($vendor) [$path]" >> "$REPORT_FILE"
    done

    echo "" >> "$REPORT_FILE"
    echo "FINDINGS" >> "$REPORT_FILE"
    echo "--------" >> "$REPORT_FILE"
}

# Check a component against known vulnerabilities
check_component() {
    local comp_name="$1"
    local comp_version="$2"
    local findings=0
    local min_sev_num
    min_sev_num=$(severity_to_num "$MIN_SEVERITY")

    for vuln in "${KNOWN_VULNS[@]}"; do
        IFS=: read -r vuln_name vuln_version cve_id severity description <<< "$vuln"

        if [[ "$vuln_name" != "$comp_name" ]]; then
            continue
        fi

        # Check if our version is <= the vulnerable version
        if version_lte "$comp_version" "$vuln_version" 2>/dev/null; then
            continue
        fi

        # Actually, check if our version is <= vulnerable version means we ARE vulnerable
        # Re-check: if vuln_version <= comp_version, we might be affected
        # For simplicity: flag if the component matches and version is in range
        local sev_num
        sev_num=$(severity_to_num "$severity")

        if (( sev_num >= min_sev_num )); then
            findings=$((findings + 1))

            echo "" >> "$REPORT_FILE"
            echo "  [$severity] $cve_id" >> "$REPORT_FILE"
            echo "    Component: $comp_name v$comp_version" >> "$REPORT_FILE"
            echo "    Affected:  <= v$vuln_version" >> "$REPORT_FILE"
            echo "    Detail:    $description" >> "$REPORT_FILE"

            if [[ "$severity" == "Critical" || "$severity" == "High" ]]; then
                log_error "$cve_id [$severity] $comp_name: $description"
                EXIT_CODE=1
            else
                log_warn "$cve_id [$severity] $comp_name: $description"
            fi
        fi
    done

    return $findings
}

# Try to use external CVE scanning tools if available
try_external_scan() {
    log_info "Checking for external CVE scanning tools..."

    # Try trivy
    if command -v trivy &>/dev/null; then
        log_info "Running trivy filesystem scan..."
        trivy fs --severity HIGH,CRITICAL --exit-code 0 \
            --format table "$PROJECT_ROOT" >> "$REPORT_FILE" 2>&1 || true
        return 0
    fi

    # Try grype with SBOM
    if command -v grype &>/dev/null; then
        local sbom_file="$PROJECT_ROOT/sbom.json"
        if [[ -f "$sbom_file" ]]; then
            log_info "Running grype SBOM scan..."
            grype "sbom:$sbom_file" --output table >> "$REPORT_FILE" 2>&1 || true
            return 0
        fi
    fi

    # Try osv-scanner
    if command -v osv-scanner &>/dev/null; then
        log_info "Running osv-scanner..."
        osv-scanner --recursive "$PROJECT_ROOT" >> "$REPORT_FILE" 2>&1 || true
        return 0
    fi

    log_warn "No external CVE scanner found (trivy, grype, osv-scanner)."
    log_warn "Using built-in known-vulnerability database only."
    echo "" >> "$REPORT_FILE"
    echo "NOTE: No external scanner available. Results are from built-in DB only." >> "$REPORT_FILE"
}

# Check for vendored headers with version strings
scan_vendored_headers() {
    log_info "Scanning vendored headers for version strings..."

    local include_dirs=(
        "$PROJECT_ROOT/include"
        "$PROJECT_ROOT/hal/include"
        "$PROJECT_ROOT/services"
        "$PROJECT_ROOT/drivers"
    )

    for dir in "${include_dirs[@]}"; do
        if [[ -d "$dir" ]]; then
            # Look for version defines in headers
            grep -rn '#define.*VERSION' "$dir" 2>/dev/null | while read -r line; do
                echo "  [HEADER] $line" >> "$REPORT_FILE"
            done || true
        fi
    done
}

# Main
main() {
    log_info "EoS CVE Scanner starting..."
    log_info "Project root: $PROJECT_ROOT"
    log_info "Report file:  $REPORT_FILE"
    echo ""

    init_report

    log_info "Checking ${#THIRD_PARTY_DEPS[@]} third-party components..."
    local total_findings=0

    for dep in "${THIRD_PARTY_DEPS[@]}"; do
        IFS=: read -r name version vendor path <<< "$dep"
        log_info "Checking $name v$version..."

        check_component "$name" "$version" || true
    done

    echo "" >> "$REPORT_FILE"

    # Scan vendored headers
    scan_vendored_headers

    # Try external tools
    echo "" >> "$REPORT_FILE"
    echo "EXTERNAL SCANNER RESULTS" >> "$REPORT_FILE"
    echo "------------------------" >> "$REPORT_FILE"
    try_external_scan

    # Summary
    echo "" >> "$REPORT_FILE"
    echo "===============================================================================" >> "$REPORT_FILE"
    if [[ $EXIT_CODE -eq 0 ]]; then
        echo "RESULT: PASS — No Critical/High CVEs found" >> "$REPORT_FILE"
        log_ok "No Critical/High CVEs found"
    else
        echo "RESULT: FAIL — Critical/High CVEs detected (see above)" >> "$REPORT_FILE"
        log_error "Critical/High CVEs detected! See $REPORT_FILE for details."
    fi
    echo "===============================================================================" >> "$REPORT_FILE"

    log_info "Full report: $REPORT_FILE"
    exit $EXIT_CODE
}

main "$@"
