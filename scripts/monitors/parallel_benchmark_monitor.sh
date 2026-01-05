#!/bin/bash
#===============================================================================
# Detachable Monitor Dashboard for Parallel Benchmark
#
# PURPOSE: Read-only dashboard that displays benchmark progress from CSV files
#          Can be attached/detached without affecting running simulations
#
# USAGE:
#   ./parallel_benchmark_monitor.sh /path/to/benchmark_dir [--watch]
#   ./parallel_benchmark_monitor.sh --latest [--watch]
#
# OPTIONS:
#   --watch, -w     Continuous refresh (default: 5 seconds)
#   --interval=N    Refresh interval in seconds (default: 5)
#   --latest        Auto-detect latest benchmark directory
#   -h, --help      Show help
#===============================================================================

set -euo pipefail

#-------------------------------------------------------------------------------
# Configuration
#-------------------------------------------------------------------------------
BENCHMARK_DIR=""
WATCH_MODE=false
REFRESH_INTERVAL=5

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'  # No color

#-------------------------------------------------------------------------------
# Argument Parsing
#-------------------------------------------------------------------------------
show_help() {
    cat <<EOF
Parallel Benchmark Monitor - Read-only Dashboard

USAGE:
    $(basename "$0") BENCHMARK_DIR [OPTIONS]
    $(basename "$0") --latest [OPTIONS]

ARGUMENTS:
    BENCHMARK_DIR   Path to benchmark output directory
    --latest        Auto-detect latest parallel_benchmark_* directory

OPTIONS:
    --watch, -w     Continuous refresh mode
    --interval=N    Refresh interval in seconds (default: 5)
    -h, --help      Show this help

EXAMPLES:
    # One-time status display
    $(basename "$0") doc/working/parallel_benchmark_20241127_100000

    # Continuous monitoring of latest benchmark
    $(basename "$0") --latest --watch

    # Watch with custom interval
    $(basename "$0") --latest -w --interval=10

EOF
    exit 0
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --latest)
                BENCHMARK_DIR="latest"
                shift
                ;;
            --watch|-w)
                WATCH_MODE=true
                shift
                ;;
            --interval=*)
                REFRESH_INTERVAL="${1#*=}"
                shift
                ;;
            -h|--help)
                show_help
                ;;
            -*)
                echo "Unknown option: $1" >&2
                show_help
                ;;
            *)
                BENCHMARK_DIR="$1"
                shift
                ;;
        esac
    done

    if [[ -z "$BENCHMARK_DIR" ]]; then
        echo "Error: No benchmark directory specified" >&2
        show_help
    fi

    # Resolve 'latest' to actual directory
    if [[ "$BENCHMARK_DIR" == "latest" ]]; then
        BENCHMARK_DIR=$(find_latest_benchmark)
        if [[ -z "$BENCHMARK_DIR" ]]; then
            echo "Error: No parallel benchmark directories found" >&2
            exit 1
        fi
    fi

    if [[ ! -d "$BENCHMARK_DIR" ]]; then
        echo "Error: Directory not found: $BENCHMARK_DIR" >&2
        exit 1
    fi
}

find_latest_benchmark() {
    # Search common locations for latest parallel_benchmark_* directory
    local search_dirs=(
        "doc/working"
        "../doc/working"
        "."
    )

    for dir in "${search_dirs[@]}"; do
        if [[ -d "$dir" ]]; then
            local latest
            latest=$(find "$dir" -maxdepth 1 -type d -name "parallel_benchmark_*" 2>/dev/null | sort -r | head -1)
            if [[ -n "$latest" ]]; then
                echo "$latest"
                return
            fi
        fi
    done
}

#-------------------------------------------------------------------------------
# Metric Reading Functions
#-------------------------------------------------------------------------------
get_latest_metric() {
    local csv_file="$1"
    local column="$2"  # Column number (1-indexed) or "last"

    if [[ ! -f "$csv_file" ]]; then
        echo "-"
        return
    fi

    local line
    line=$(tail -1 "$csv_file" 2>/dev/null)

    # Skip header
    if [[ "$line" == "timestamp"* ]]; then
        echo "-"
        return
    fi

    if [[ "$column" == "last" ]]; then
        echo "$line" | awk -F',' '{print $NF}'
    else
        echo "$line" | cut -d',' -f"$column"
    fi
}

get_sim_status() {
    local sim="$1"
    local pid_file="$BENCHMARK_DIR/state/$sim.pid"

    if [[ ! -f "$pid_file" ]]; then
        echo "not_started"
        return
    fi

    local pid
    pid=$(cat "$pid_file" 2>/dev/null)

    if [[ -z "$pid" ]]; then
        echo "unknown"
    elif kill -0 "$pid" 2>/dev/null; then
        echo "running"
    else
        echo "stopped"
    fi
}

get_elapsed_time() {
    local start_file="$BENCHMARK_DIR/start_time"

    if [[ ! -f "$start_file" ]]; then
        echo "N/A"
        return
    fi

    local start_time
    start_time=$(cat "$start_file")
    local start_epoch
    start_epoch=$(date -d "$start_time" +%s 2>/dev/null || echo "0")

    if [[ "$start_epoch" == "0" ]]; then
        echo "N/A"
        return
    fi

    local now
    now=$(date +%s)
    local elapsed=$((now - start_epoch))

    local hours=$((elapsed / 3600))
    local mins=$(((elapsed % 3600) / 60))
    local secs=$((elapsed % 60))

    printf "%dh %dm %ds" "$hours" "$mins" "$secs"
}

#-------------------------------------------------------------------------------
# Display Functions
#-------------------------------------------------------------------------------
print_header() {
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${CYAN}  SimuCell3D Parallel Benchmark Monitor${NC}"
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "  ${BLUE}Directory:${NC} $BENCHMARK_DIR"
    echo -e "  ${BLUE}Elapsed:${NC}   $(get_elapsed_time)"
    echo -e "  ${BLUE}Updated:${NC}   $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
}

print_simulation_table() {
    echo -e "${YELLOW}SIMULATION STATUS${NC}"
    echo "───────────────────────────────────────────────────────────────────────────────"
    printf "%-12s %10s %10s %10s %8s %8s %10s %10s\n" \
        "SIMULATION" "ITERATION" "CELLS" "ITER/SEC" "CPU%" "CTX/s" "MEM(GB)" "STATUS"
    echo "───────────────────────────────────────────────────────────────────────────────"

    for sim in v1 static adaptive; do
        local csv="$BENCHMARK_DIR/metrics/$sim/computational.csv"
        local status
        status=$(get_sim_status "$sim")

        if [[ -f "$csv" ]] && [[ $(wc -l < "$csv") -gt 1 ]]; then
            local latest
            latest=$(tail -1 "$csv")

            # Updated to match new CSV format with delta context switches
            local iter cells ips cpu_user cpu_sys rss_mb vsize_mb vol_ctx invol_ctx
            IFS=',' read -r _ iter cells ips cpu_user cpu_sys rss_mb vsize_mb vol_ctx invol_ctx _ <<< "$latest"
            local ctx_total=$((${vol_ctx:-0} + ${invol_ctx:-0}))

            # Color status
            local status_color
            case "$status" in
                running)  status_color="${GREEN}$status${NC}" ;;
                stopped)  status_color="${RED}$status${NC}" ;;
                *)        status_color="${YELLOW}$status${NC}" ;;
            esac

            printf "%-12s %10s %10s %10s %8s %8s %10s %b\n" \
                "$sim" "$iter" "$cells" "$ips" "$cpu_user" "$ctx_total" "$rss_mb" "$status_color"
        else
            printf "%-12s %10s %10s %10s %8s %8s %10s %s\n" \
                "$sim" "-" "-" "-" "-" "-" "-" "$status"
        fi
    done
    echo ""
}

print_comparison() {
    local csv="$BENCHMARK_DIR/metrics/comparison.csv"

    if [[ ! -f "$csv" ]] || [[ $(wc -l < "$csv") -le 1 ]]; then
        return
    fi

    echo -e "${YELLOW}PERFORMANCE COMPARISON${NC}"
    echo "───────────────────────────────────────────────────────────────────────────────"

    local latest
    latest=$(tail -1 "$csv")

    local static_vs_v1 adaptive_vs_v1 adaptive_vs_static
    IFS=',' read -r _ _ _ _ _ _ _ _ _ _ static_vs_v1 adaptive_vs_v1 adaptive_vs_static <<< "$latest"

    echo -e "  Speedup vs v1.0:"
    printf "    %-12s ${GREEN}%sx${NC}\n" "static:" "$static_vs_v1"
    printf "    %-12s ${GREEN}%sx${NC}\n" "adaptive:" "$adaptive_vs_v1"
    echo ""
    printf "  adaptive vs static: ${GREEN}%sx${NC}\n" "$adaptive_vs_static"
    echo ""
}

print_workload_metrics() {
    local csv="$BENCHMARK_DIR/metrics/adaptive/workload.csv"

    if [[ ! -f "$csv" ]] || [[ $(wc -l < "$csv") -le 1 ]]; then
        return
    fi

    echo -e "${YELLOW}ADAPTIVE SCHEDULER METRICS${NC}"
    echo "───────────────────────────────────────────────────────────────────────────────"

    local latest
    latest=$(tail -1 "$csv")

    local iter cov imbalance phase
    IFS=',' read -r _ iter cov imbalance phase <<< "$latest"

    printf "  %-25s %s\n" "Coefficient of Variation:" "$cov"
    printf "  %-25s %s%%\n" "Thread Imbalance:" "$imbalance"
    printf "  %-25s %s\n" "Simulation Phase:" "$phase"
    echo ""
}

print_biological_metrics() {
    echo -e "${YELLOW}BIOLOGICAL METRICS${NC}"
    echo "───────────────────────────────────────────────────────────────────────────────"
    printf "%-12s %10s %12s %12s %12s\n" \
        "SIMULATION" "DIVISIONS" "ENERGY/SEC" "CELL*IPS" "PRESS(min/max)"
    echo "───────────────────────────────────────────────────────────────────────────────"

    for sim in v1 static adaptive; do
        local csv="$BENCHMARK_DIR/metrics/$sim/biological.csv"

        if [[ -f "$csv" ]] && [[ $(wc -l < "$csv") -gt 1 ]]; then
            local latest
            latest=$(tail -1 "$csv")

            # Format: timestamp,iteration,cell_count,ke,pe,p_mean,p_std,p_min,p_max,v_mean,v_std,v_min,v_max,divisions,energy_drift,cell_ips
            local divisions energy_drift cell_ips p_min p_max
            IFS=',' read -r _ _ _ _ _ _ _ p_min p_max _ _ _ _ divisions energy_drift cell_ips <<< "$latest"

            # Format pressure range
            local press_range="${p_min:-?}/${p_max:-?}"

            printf "%-12s %10s %12s %12s %15s\n" \
                "$sim" "${divisions:-0}" "${energy_drift:-0}" "${cell_ips:-0}" "$press_range"
        else
            printf "%-12s %10s %12s %12s %15s\n" "$sim" "-" "-" "-" "-"
        fi
    done
    echo ""
}

print_cache_metrics() {
    # Check if any cache metrics exist
    local has_cache=false
    for sim in v1 static adaptive; do
        local csv="$BENCHMARK_DIR/metrics/$sim/cache_metrics.csv"
        if [[ -f "$csv" ]] && [[ $(wc -l < "$csv") -gt 1 ]]; then
            has_cache=true
            break
        fi
    done

    if [[ "$has_cache" != "true" ]]; then
        return
    fi

    echo -e "${YELLOW}CACHE PERFORMANCE (via perf)${NC}"
    echo "───────────────────────────────────────────────────────────────────────────────"
    printf "%-12s %12s %12s %12s %8s\n" \
        "SIMULATION" "L1 MISSES" "L1 RATE%" "LLC MISSES" "IPC"
    echo "───────────────────────────────────────────────────────────────────────────────"

    for sim in v1 static adaptive; do
        local csv="$BENCHMARK_DIR/metrics/$sim/cache_metrics.csv"

        if [[ -f "$csv" ]] && [[ $(wc -l < "$csv") -gt 1 ]]; then
            local latest
            latest=$(tail -1 "$csv")

            # Format: timestamp,iteration,l1_misses,l1_rate,l2_misses,l3_misses,ipc,instructions,cycles,duration
            local l1_misses l1_rate l2_misses l3_misses ipc
            IFS=',' read -r _ _ l1_misses l1_rate l2_misses l3_misses ipc _ <<< "$latest"

            printf "%-12s %12s %12s %12s %8s\n" \
                "$sim" "${l1_misses:-0}" "${l1_rate:-0}" "${l3_misses:-0}" "${ipc:-0}"
        else
            printf "%-12s %12s %12s %12s %8s\n" "$sim" "-" "-" "-" "-"
        fi
    done
    echo ""
}

print_health_events() {
    local csv="$BENCHMARK_DIR/metrics/health_log.csv"

    echo -e "${YELLOW}RECENT HEALTH EVENTS${NC}"
    echo "───────────────────────────────────────────────────────────────────────────────"

    if [[ -f "$csv" ]] && [[ $(wc -l < "$csv") -gt 1 ]]; then
        tail -5 "$csv" | grep -v "^timestamp" | while IFS=',' read -r ts sim event msg; do
            local color
            case "$event" in
                COMPLETED) color="${GREEN}" ;;
                CRASHED|SIGSEGV|OOM*) color="${RED}" ;;
                STALLED|RESTARTING) color="${YELLOW}" ;;
                *) color="${NC}" ;;
            esac
            printf "  %s  ${color}%-10s${NC}  %-12s  %s\n" "$ts" "$event" "$sim" "$msg"
        done
    else
        echo "  (no events recorded)"
    fi
    echo ""
}

print_phase_timings() {
    echo -e "${YELLOW}PHASE TIMING BREAKDOWN (ms)${NC}"
    echo "───────────────────────────────────────────────────────────────────────────────"
    printf "%-12s %12s %12s %12s %12s %12s\n" \
        "SIMULATION" "MESH" "CONTACT" "POLAR" "INTEG" "TOTAL"
    echo "───────────────────────────────────────────────────────────────────────────────"

    for sim in static adaptive; do
        local csv="$BENCHMARK_DIR/metrics/$sim/phase_timings.csv"

        if [[ -f "$csv" ]] && [[ $(wc -l < "$csv") -gt 1 ]]; then
            local latest
            latest=$(tail -1 "$csv")

            local mesh contact polar integ total
            IFS=',' read -r _ _ mesh contact polar integ total <<< "$latest"

            printf "%-12s %12s %12s %12s %12s %12s\n" \
                "$sim" "$mesh" "$contact" "$polar" "$integ" "$total"
        else
            printf "%-12s %12s %12s %12s %12s %12s\n" "$sim" "-" "-" "-" "-" "-"
        fi
    done

    echo "  (v1.0 does not export phase timings)"
    echo ""
}

print_footer() {
    echo -e "${CYAN}───────────────────────────────────────────────────────────────────────────────${NC}"
    if [[ "$WATCH_MODE" == "true" ]]; then
        echo -e "  Refreshing every ${REFRESH_INTERVAL}s | Press ${YELLOW}Ctrl+C${NC} to exit"
    else
        echo -e "  Use ${YELLOW}--watch${NC} for continuous monitoring"
    fi
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════════════════════${NC}"
}

print_dashboard() {
    clear
    print_header
    print_simulation_table
    print_comparison
    print_biological_metrics
    print_workload_metrics
    print_cache_metrics
    print_phase_timings
    print_health_events
    print_footer
}

#-------------------------------------------------------------------------------
# Main
#-------------------------------------------------------------------------------
main() {
    parse_args "$@"

    if [[ "$WATCH_MODE" == "true" ]]; then
        # Continuous monitoring
        while true; do
            print_dashboard
            sleep "$REFRESH_INTERVAL"
        done
    else
        # One-time display
        print_dashboard
    fi
}

main "$@"
