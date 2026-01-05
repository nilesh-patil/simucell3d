#!/bin/bash

################################################################################
# PARALLEL BENCHMARK UTILITIES LIBRARY (Simplified)
################################################################################
# Specialized functions for parallel benchmark comparison (v1.0 vs static vs adaptive)
# Extends benchmark_utils.sh with:
#   - Version-aware log parsing (v1.0 vs next format differences)
#   - Process health monitoring and auto-restart
#   - Multi-simulation checkpoint management
#
# NOTE: Metric collection has been removed - simulations now write their own
# performance_diagnostics.csv and simulation_statistics.csv files directly.
################################################################################

# Ensure benchmark_utils.sh is sourced first
if ! type log &>/dev/null; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    source "$SCRIPT_DIR/benchmark_utils.sh" || {
        echo "ERROR: Failed to source benchmark_utils.sh" >&2
        exit 1
    }
fi

################################################################################
# CONSTANTS
################################################################################

# Stall detection threshold (seconds without iteration progress)
readonly STALL_THRESHOLD_SEC=1800  # 30 minutes (increased for 128k cell simulations)

# Maximum restart attempts per simulation
readonly MAX_RESTARTS=3

# Health check return codes
readonly HEALTH_OK=0
readonly HEALTH_CRASHED=1
readonly HEALTH_STALLED=2
readonly HEALTH_COMPLETED=3

################################################################################
# VERSION-AWARE LOG PARSING
################################################################################

# Get iteration count from log file (version-agnostic wrapper)
# Args: $1 - log file, $2 - version ("v1" or "next")
get_iteration_from_log() {
    local log_file="$1"
    local version="${2:-next}"

    if [[ ! -f "$log_file" ]]; then
        echo "0"
        return 1
    fi

    # Both v1.0 and next use same format: "iteration: N"
    local iter
    iter=$(tail -100 "$log_file" 2>/dev/null | grep -oP 'iteration: \K[0-9]+' | tail -1)

    echo "${iter:-0}"
}

# Get cell count from log file (version-agnostic wrapper)
# Args: $1 - log file, $2 - version ("v1" or "next")
get_cells_from_log() {
    local log_file="$1"
    local version="${2:-next}"

    if [[ ! -f "$log_file" ]]; then
        echo "0"
        return 1
    fi

    # Both v1.0 and next use same format: "nb cells N"
    local cells
    cells=$(tail -100 "$log_file" 2>/dev/null | grep -oP 'nb cells \K[0-9]+' | tail -1)

    echo "${cells:-0}"
}

################################################################################
# HEALTH MONITORING
################################################################################

# Check simulation health status
# Args: $1 - simulation name, $2 - PID, $3 - log file, $4 - state directory
# Returns: HEALTH_OK, HEALTH_CRASHED, HEALTH_STALLED, or HEALTH_COMPLETED
check_simulation_health() {
    local sim="$1"
    local pid="$2"
    local log_file="$3"
    local state_dir="$4"

    # Check 1: Process still alive?
    if ! kill -0 "$pid" 2>/dev/null; then
        # Process is gone - check if it completed normally
        local exit_code=0
        if wait "$pid" 2>/dev/null; then
            exit_code=$?
        fi

        if [[ $exit_code -eq 0 ]]; then
            return $HEALTH_COMPLETED
        else
            return $HEALTH_CRASHED
        fi
    fi

    # Check 2: Stall detection (no progress for STALL_THRESHOLD_SEC)
    local iter_file="$state_dir/last_iter"
    local time_file="$state_dir/last_iter_time"
    local current_iter
    current_iter=$(get_iteration_from_log "$log_file" "next")

    local now
    now=$(date +%s)

    if [[ -f "$iter_file" ]] && [[ -f "$time_file" ]]; then
        local last_iter last_time
        last_iter=$(cat "$iter_file")
        last_time=$(cat "$time_file")

        if [[ "$current_iter" == "$last_iter" ]]; then
            # No progress since last check
            local stall_duration=$((now - last_time))
            if [[ $stall_duration -ge $STALL_THRESHOLD_SEC ]]; then
                return $HEALTH_STALLED
            fi
        else
            # Progress made - update tracking files
            echo "$current_iter" > "$iter_file"
            echo "$now" > "$time_file"
        fi
    else
        # First check - initialize tracking
        echo "$current_iter" > "$iter_file"
        echo "$now" > "$time_file"
    fi

    return $HEALTH_OK
}

# Interpret exit code and return human-readable status
# Args: $1 - exit code
interpret_exit_code() {
    local exit_code="$1"

    case $exit_code in
        0)   echo "COMPLETED" ;;
        1)   echo "ERROR" ;;
        134) echo "SIGABRT" ;;
        137) echo "OOM_KILLED" ;;
        139) echo "SIGSEGV" ;;
        143) echo "SIGTERM" ;;
        *)   echo "EXIT_$exit_code" ;;
    esac
}

# Log health event to CSV
# Args: $1 - health log file, $2 - simulation name, $3 - event type, $4 - message
log_health_event() {
    local health_file="$1"
    local sim="$2"
    local event="$3"
    local message="$4"

    local timestamp epoch
    timestamp=$(date +%Y-%m-%d_%H:%M:%S)
    epoch=$(date +%s)

    echo "$timestamp,$epoch,$sim,$event,$message" >> "$health_file"
    log "[$sim] Health event: $event - $message"
}

################################################################################
# AUTO-RESTART LOGIC
################################################################################

# Get restart count for a simulation
# Args: $1 - state directory, $2 - simulation name
get_restart_count() {
    local state_dir="$1"
    local sim="$2"
    local count_file="$state_dir/${sim}_restart_count"

    if [[ -f "$count_file" ]]; then
        cat "$count_file"
    else
        echo "0"
    fi
}

# Increment restart count for a simulation
# Args: $1 - state directory, $2 - simulation name
increment_restart_count() {
    local state_dir="$1"
    local sim="$2"
    local count_file="$state_dir/${sim}_restart_count"

    local current
    current=$(get_restart_count "$state_dir" "$sim")
    echo $((current + 1)) > "$count_file"
}

# Check if restart is allowed (under MAX_RESTARTS limit)
# Args: $1 - state directory, $2 - simulation name
can_restart() {
    local state_dir="$1"
    local sim="$2"

    local count
    count=$(get_restart_count "$state_dir" "$sim")

    if [[ $count -lt $MAX_RESTARTS ]]; then
        return 0  # Can restart
    else
        return 1  # Max restarts exceeded
    fi
}

################################################################################
# CHECKPOINT MANAGEMENT (Multi-simulation)
################################################################################

# Create checkpoint JSON for parallel benchmark
# Args: $1 - checkpoint file, $2 - wall_elapsed
#       Remaining args: sim_name:status:pid:iteration:cells:restarts pairs
save_parallel_checkpoint() {
    local checkpoint_file="$1"
    local wall_elapsed="$2"
    shift 2

    # Remaining args: sim_name:status:pid:iteration:cells:restarts pairs
    local timestamp
    timestamp=$(date -Iseconds)

    # Build simulations JSON object
    local sims_json="{"
    local first=true

    for sim_data in "$@"; do
        IFS=':' read -r name status pid iter cells restarts <<< "$sim_data"

        [[ "$first" == "true" ]] || sims_json+=","
        first=false

        sims_json+="\"$name\":{\"status\":\"$status\",\"pid\":$pid,\"iteration\":$iter,\"cells\":$cells,\"restarts\":$restarts}"
    done
    sims_json+="}"

    cat > "$checkpoint_file" <<EOF
{
    "version": "2.0",
    "timestamp": "$timestamp",
    "wall_time_elapsed_sec": $wall_elapsed,
    "simulations": $sims_json
}
EOF

    log "Checkpoint saved: $checkpoint_file"
}

# Load parallel checkpoint and set global variables
# Args: $1 - checkpoint file
# Sets: CHECKPOINT_SIMS associative array
load_parallel_checkpoint() {
    local checkpoint_file="$1"

    if [[ ! -f "$checkpoint_file" ]]; then
        error "Checkpoint file not found: $checkpoint_file"
        return 1
    fi

    if ! command -v jq &>/dev/null; then
        error "jq required for checkpoint parsing"
        return 1
    fi

    log "Loading checkpoint: $checkpoint_file"

    # Parse into associative array
    declare -gA CHECKPOINT_SIMS

    local sims
    sims=$(jq -r '.simulations | keys[]' "$checkpoint_file")

    for sim in $sims; do
        local status iter cells restarts
        status=$(jq -r ".simulations.$sim.status" "$checkpoint_file")
        iter=$(jq -r ".simulations.$sim.iteration" "$checkpoint_file")
        cells=$(jq -r ".simulations.$sim.cells" "$checkpoint_file")
        restarts=$(jq -r ".simulations.$sim.restarts" "$checkpoint_file")

        CHECKPOINT_SIMS[$sim]="$status:$iter:$cells:$restarts"
        log "  $sim: status=$status, iteration=$iter, cells=$cells, restarts=$restarts"
    done

    export CHECKPOINT_WALL_TIME
    CHECKPOINT_WALL_TIME=$(jq -r '.wall_time_elapsed_sec' "$checkpoint_file")
}

################################################################################
# CSV INITIALIZATION
################################################################################

# Initialize health log CSV with header
# Args: $1 - output file path
init_health_log_csv() {
    local file="$1"
    echo "timestamp,epoch,simulation,event,message" > "$file"
}

################################################################################
# METADATA RECORDING
################################################################################

# Record benchmark metadata to JSON
# Args: $1 - output file, $2 - param file, $3 - cores per sim, $4 - monitor interval
record_benchmark_metadata() {
    local output_file="$1"
    local param_file="$2"
    local cores_per_sim="$3"
    local monitor_interval="$4"

    local timestamp hostname cpu_model git_hash kernel
    timestamp=$(date -Iseconds)
    hostname=$(hostname)
    cpu_model=$(lscpu 2>/dev/null | grep "Model name:" | cut -d':' -f2 | xargs || echo "unknown")
    git_hash=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
    kernel=$(uname -r)

    cat > "$output_file" <<EOF
{
    "benchmark_type": "parallel_comparison",
    "timestamp": "$timestamp",
    "hostname": "$hostname",
    "cpu_model": "$cpu_model",
    "kernel": "$kernel",
    "git_hash": "$git_hash",
    "config": {
        "param_file": "$param_file",
        "cores_per_sim": $cores_per_sim,
        "monitor_interval_sec": $monitor_interval,
        "simulations": ["v1", "static", "adaptive"]
    },
    "omp_settings": {
        "OMP_NUM_THREADS": "${OMP_NUM_THREADS:-auto}",
        "OMP_WAIT_POLICY": "${OMP_WAIT_POLICY:-active}",
        "OMP_DYNAMIC": "${OMP_DYNAMIC:-true}"
    }
}
EOF

    log "Metadata recorded: $output_file"
}

################################################################################
# UTILITY FUNCTIONS
################################################################################

# Check if any simulation from list is still running
# Args: PID list
any_process_running() {
    for pid in "$@"; do
        if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
            return 0  # At least one running
        fi
    done
    return 1  # None running
}

################################################################################
# EXPORT FUNCTIONS
################################################################################

export -f get_iteration_from_log get_cells_from_log
export -f check_simulation_health interpret_exit_code log_health_event
export -f get_restart_count increment_restart_count can_restart
export -f save_parallel_checkpoint load_parallel_checkpoint
export -f init_health_log_csv
export -f record_benchmark_metadata
export -f any_process_running
