#!/usr/bin/env bash
#
# perf_wrapper.sh - Hardware counter profiling wrapper for SimuCell3D benchmarks
#
# Automates `perf stat` collection with structured JSON output for CI/CD integration.
# Collects cache misses, IPC, branch prediction, and memory bandwidth metrics.
#
# Usage:
#   ./perf_wrapper.sh <benchmark_executable> <test_name> [--output=<file.json>] [--events=<event_list>]
#   ./perf_wrapper.sh --all [--output-dir=<dir>]
#   ./perf_wrapper.sh --summary <json_file> [<json_file2> ...]
#
# Examples:
#   ./perf_wrapper.sh build/test/test_benchmarks/test_contact_latency test_uspg_latency_large
#   ./perf_wrapper.sh --all --output-dir=results/
#   ./perf_wrapper.sh --summary results/*.json
#
# Output format (JSON):
#   {
#     "benchmark": "test_uspg_latency_large",
#     "timestamp": "2026-02-15T12:00:00Z",
#     "counters": {
#       "cache-misses": 12345,
#       "cache-references": 67890,
#       "cache-miss-rate": 0.182,
#       "instructions": 123456789,
#       "cycles": 234567890,
#       "ipc": 0.526,
#       ...
#     },
#     "duration_seconds": 1.234,
#     "exit_code": 0
#   }

set -euo pipefail

# Script directory for relative path resolution
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# Default perf events for HPC profiling
DEFAULT_EVENTS="cache-misses,cache-references,instructions,cycles,branches,branch-misses,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses,task-clock,context-switches,cpu-migrations"

# Colors for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

#-------------------------------------------------------------------
# Utility functions
#-------------------------------------------------------------------

log_info()  { echo -e "${BLUE}[INFO]${NC} $*"; }
log_ok()    { echo -e "${GREEN}[OK]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $*" >&2; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

check_perf() {
    if ! command -v perf &>/dev/null; then
        log_error "perf not found. Install with: sudo apt-get install linux-tools-\$(uname -r)"
        exit 1
    fi

    # Check if perf has permission to read HW counters
    if ! perf stat -e cycles true 2>/dev/null; then
        log_warn "perf lacks permissions for hardware counters."
        log_warn "Try: sudo sysctl -w kernel.perf_event_paranoid=-1"
        log_warn "Falling back to software-only counters."
        return 1
    fi
    return 0
}

#-------------------------------------------------------------------
# Run a single benchmark with perf stat and output JSON
#-------------------------------------------------------------------
run_benchmark() {
    local executable="$1"
    local test_name="$2"
    local output_file="${3:-}"
    local events="${4:-${DEFAULT_EVENTS}}"

    if [[ ! -x "${executable}" ]]; then
        log_error "Executable not found or not executable: ${executable}"
        return 1
    fi

    local timestamp
    timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

    log_info "Running: ${executable} ${test_name}"
    log_info "Events:  ${events}"

    # Create temp file for perf output
    local perf_output
    perf_output=$(mktemp /tmp/perf_stat_XXXXXX.txt)
    local bench_output
    bench_output=$(mktemp /tmp/bench_output_XXXXXX.txt)

    local exit_code=0
    local duration_seconds=0

    # Run perf stat
    local start_time
    start_time=$(date +%s%N)

    if perf stat -e "${events}" -o "${perf_output}" -- "${executable}" "${test_name}" > "${bench_output}" 2>&1; then
        exit_code=0
    else
        exit_code=$?
    fi

    local end_time
    end_time=$(date +%s%N)
    duration_seconds=$(echo "scale=6; (${end_time} - ${start_time}) / 1000000000" | bc 2>/dev/null || echo "0")

    # Parse perf output into JSON
    local json="{"
    json+="\"benchmark\": \"${test_name}\","
    json+="\"executable\": \"${executable}\","
    json+="\"timestamp\": \"${timestamp}\","
    json+="\"duration_seconds\": ${duration_seconds},"
    json+="\"exit_code\": ${exit_code},"
    json+="\"counters\": {"

    local first=true
    local cache_misses=0
    local cache_refs=0
    local instructions=0
    local cycles=0
    local branches=0
    local branch_misses=0

    while IFS= read -r line; do
        # Skip empty lines, comments, and header
        [[ -z "${line}" || "${line}" =~ ^# || "${line}" =~ ^$ ]] && continue

        # Parse lines like: "  12,345      cache-misses"
        local count event
        count=$(echo "${line}" | awk '{print $1}' | tr -d ',')
        event=$(echo "${line}" | awk '{print $2}')

        # Skip non-numeric
        [[ ! "${count}" =~ ^[0-9]+$ ]] && continue
        [[ -z "${event}" ]] && continue

        if [[ "${first}" == "true" ]]; then
            first=false
        else
            json+=","
        fi
        json+="\"${event}\": ${count}"

        # Track key counters for derived metrics
        case "${event}" in
            cache-misses)      cache_misses=${count} ;;
            cache-references)  cache_refs=${count} ;;
            instructions)      instructions=${count} ;;
            cycles)            cycles=${count} ;;
            branches)          branches=${count} ;;
            branch-misses)     branch_misses=${count} ;;
        esac
    done < "${perf_output}"

    json+="},"

    # Compute derived metrics
    local cache_miss_rate=0
    local ipc=0
    local branch_miss_rate=0

    if [[ ${cache_refs} -gt 0 ]]; then
        cache_miss_rate=$(echo "scale=6; ${cache_misses} / ${cache_refs}" | bc 2>/dev/null || echo "0")
    fi
    if [[ ${cycles} -gt 0 ]]; then
        ipc=$(echo "scale=4; ${instructions} / ${cycles}" | bc 2>/dev/null || echo "0")
    fi
    if [[ ${branches} -gt 0 ]]; then
        branch_miss_rate=$(echo "scale=6; ${branch_misses} / ${branches}" | bc 2>/dev/null || echo "0")
    fi

    json+="\"derived\": {"
    json+="\"cache_miss_rate\": ${cache_miss_rate},"
    json+="\"ipc\": ${ipc},"
    json+="\"branch_miss_rate\": ${branch_miss_rate}"
    json+="},"

    # Include benchmark stdout
    local bench_stdout
    bench_stdout=$(cat "${bench_output}" | tr '\n' '|' | sed 's/"/\\"/g')
    json+="\"output\": \"${bench_stdout}\""
    json+="}"

    # Cleanup temp files
    rm -f "${perf_output}" "${bench_output}"

    # Output JSON
    if [[ -n "${output_file}" ]]; then
        echo "${json}" | python3 -m json.tool > "${output_file}" 2>/dev/null || echo "${json}" > "${output_file}"
        log_ok "Results written to: ${output_file}"
    else
        echo "${json}" | python3 -m json.tool 2>/dev/null || echo "${json}"
    fi

    # Print key metrics summary
    echo ""
    log_info "=== Key Metrics ==="
    log_info "Cache miss rate:   ${cache_miss_rate} (${cache_misses}/${cache_refs})"
    log_info "IPC:               ${ipc}"
    log_info "Branch miss rate:  ${branch_miss_rate}"
    log_info "Duration:          ${duration_seconds}s"
    log_info "Exit code:         ${exit_code}"

    return ${exit_code}
}

#-------------------------------------------------------------------
# Run all benchmarks
#-------------------------------------------------------------------
run_all_benchmarks() {
    local output_dir="${1:-results}"
    mkdir -p "${output_dir}"

    log_info "Running all benchmarks, output to: ${output_dir}/"

    local all_tests=(
        # Contact latency
        "test_contact_latency:test_uspg_latency_small"
        "test_contact_latency:test_sap_latency_small"
        "test_contact_latency:test_uspg_latency_medium"
        "test_contact_latency:test_sap_latency_medium"
        "test_contact_latency:test_uspg_latency_large"
        "test_contact_latency:test_sap_latency_large"
        "test_contact_latency:test_scaling_behavior"
        # Atomic contention
        "test_atomic_contention:test_uncontended_mutex_cost"
        "test_atomic_contention:test_contended_mutex_scaling"
        "test_atomic_contention:test_atomic_vs_mutex_accumulation"
        "test_atomic_contention:test_false_sharing_detection"
        "test_atomic_contention:test_per_node_mutex_pattern"
        # Memory bandwidth
        "test_memory_bandwidth:test_sequential_read_bandwidth"
        "test_memory_bandwidth:test_vec3_aos_bandwidth"
        "test_memory_bandwidth:test_random_access_latency"
        "test_memory_bandwidth:test_soa_vs_aos_comparison"
        "test_memory_bandwidth:test_parallel_bandwidth_scaling"
    )

    local passed=0
    local failed=0
    local total=${#all_tests[@]}

    for entry in "${all_tests[@]}"; do
        local exe_name="${entry%%:*}"
        local test_name="${entry##*:}"
        local exe_path="${BUILD_DIR}/test/test_benchmarks/${exe_name}"
        local json_file="${output_dir}/${test_name}.json"

        if run_benchmark "${exe_path}" "${test_name}" "${json_file}"; then
            ((passed++))
        else
            ((failed++))
            log_warn "FAILED: ${test_name}"
        fi
        echo "---"
    done

    echo ""
    log_info "=== Summary ==="
    log_info "Total:   ${total}"
    log_ok   "Passed:  ${passed}"
    if [[ ${failed} -gt 0 ]]; then
        log_error "Failed:  ${failed}"
    fi

    return ${failed}
}

#-------------------------------------------------------------------
# Print summary from JSON result files
#-------------------------------------------------------------------
print_summary() {
    local files=("$@")

    echo "| Benchmark | Cache Miss Rate | IPC | Duration (s) | Status |"
    echo "|-----------|----------------|-----|-------------|--------|"

    for f in "${files[@]}"; do
        if [[ ! -f "${f}" ]]; then
            continue
        fi

        local name miss_rate ipc duration status
        name=$(python3 -c "import json; d=json.load(open('${f}')); print(d.get('benchmark','?'))" 2>/dev/null || echo "?")
        miss_rate=$(python3 -c "import json; d=json.load(open('${f}')); print(f\"{d.get('derived',{}).get('cache_miss_rate',0):.4f}\")" 2>/dev/null || echo "?")
        ipc=$(python3 -c "import json; d=json.load(open('${f}')); print(f\"{d.get('derived',{}).get('ipc',0):.2f}\")" 2>/dev/null || echo "?")
        duration=$(python3 -c "import json; d=json.load(open('${f}')); print(f\"{d.get('duration_seconds',0):.3f}\")" 2>/dev/null || echo "?")
        status=$(python3 -c "import json; d=json.load(open('${f}')); print('PASS' if d.get('exit_code',1)==0 else 'FAIL')" 2>/dev/null || echo "?")

        echo "| ${name} | ${miss_rate} | ${ipc} | ${duration} | ${status} |"
    done
}

#-------------------------------------------------------------------
# Main
#-------------------------------------------------------------------
usage() {
    cat <<EOF
Usage: $(basename "$0") <command> [options]

Commands:
  <executable> <test_name>   Run a single benchmark with perf stat
    --output=<file.json>     Write JSON results to file
    --events=<event_list>    Custom perf events (comma-separated)

  --all                      Run all registered benchmarks
    --output-dir=<dir>       Output directory for JSON files (default: results/)

  --summary <file.json> ...  Print summary table from JSON results

  --help                     Show this help

Examples:
  $(basename "$0") build/test/test_benchmarks/test_contact_latency test_uspg_latency_large
  $(basename "$0") --all --output-dir=benchmark_results/
  $(basename "$0") --summary benchmark_results/*.json
EOF
}

main() {
    if [[ $# -lt 1 ]]; then
        usage
        exit 1
    fi

    case "$1" in
        --help|-h)
            usage
            exit 0
            ;;
        --all)
            shift
            local output_dir="results"
            for arg in "$@"; do
                case "${arg}" in
                    --output-dir=*) output_dir="${arg#*=}" ;;
                esac
            done
            check_perf || true
            run_all_benchmarks "${output_dir}"
            ;;
        --summary)
            shift
            print_summary "$@"
            ;;
        *)
            # Single benchmark mode
            local executable="$1"
            local test_name="${2:-}"
            local output_file=""
            local events="${DEFAULT_EVENTS}"

            shift 2 || { usage; exit 1; }

            for arg in "$@"; do
                case "${arg}" in
                    --output=*)  output_file="${arg#*=}" ;;
                    --events=*)  events="${arg#*=}" ;;
                esac
            done

            check_perf || true
            run_benchmark "${executable}" "${test_name}" "${output_file}" "${events}"
            ;;
    esac
}

main "$@"
