#!/bin/bash

################################################################################
# BENCHMARK UTILITIES LIBRARY
################################################################################
# Shared functions for unified benchmark framework
# Extracted from run_parallel_core_benchmark.sh and run_parallel_progressive_benchmark.sh
################################################################################

################################################################################
# LOGGING
################################################################################

# Log levels: INFO, WARN, ERROR
LOG_LEVEL="${LOG_LEVEL:-INFO}"
LOG_FILE=""

log() {
    local level="INFO"
    local message="$*"

    # Check if first arg is a log level
    if [[ "$1" =~ ^(INFO|WARN|ERROR)$ ]]; then
        level="$1"
        shift
        message="$*"
    fi

    local timestamp="[$(date '+%Y-%m-%d %H:%M:%S')]"
    local formatted="$timestamp [$level] $message"

    # Print to stdout
    echo "$formatted"

    # Append to log file if set
    if [[ -n "$LOG_FILE" ]]; then
        echo "$formatted" >> "$LOG_FILE"
    fi
}

error() {
    log ERROR "$@" >&2
}

warn() {
    log WARN "$@"
}

################################################################################
# METRIC EXTRACTION
################################################################################

extract_metrics() {
    local log_file="$1"

    if [[ ! -f "$log_file" ]]; then
        echo "0,0,0,0,0,1"  # All zeros with exit code 1 (failed)
        return 1
    fi

    local elapsed=$(grep -E "Elapsed.*wall clock" "$log_file" | awk '{print $NF}' | sed 's/[()]//g')

    local wall_sec=0
    # Handle formats: "5.31", "76:46", "76:46.27", "1:16:46", "1:16:46.27"
    if [[ "$elapsed" =~ ^([0-9]+):([0-9]+):([0-9]+)(\.[0-9]+)?$ ]]; then
        # Hour:min:sec format (with optional decimals)
        local secs="${BASH_REMATCH[3]}${BASH_REMATCH[4]:-}"  # Append decimals if present
        wall_sec=$(awk "BEGIN {print ${BASH_REMATCH[1]} * 3600 + ${BASH_REMATCH[2]} * 60 + $secs}")
    elif [[ "$elapsed" =~ ^([0-9]+):([0-9]+)(\.[0-9]+)?$ ]]; then
        # Min:sec format (with optional decimals)
        local secs="${BASH_REMATCH[2]}${BASH_REMATCH[3]:-}"  # Append decimals if present
        wall_sec=$(awk "BEGIN {print ${BASH_REMATCH[1]} * 60 + $secs}")
    elif [[ "$elapsed" =~ ^([0-9]+)(\.[0-9]+)?$ ]]; then
        # Just seconds (with optional decimals)
        wall_sec="$elapsed"
    fi

    local user_sec=$(grep "User time" "$log_file" | awk '{print $4}' || echo "0")
    local sys_sec=$(grep "System time" "$log_file" | awk '{print $4}' || echo "0")
    local cpu_pct=$(grep "Percent of CPU" "$log_file" | awk '{print $7}' | tr -d '%' || echo "0")
    local max_rss_kb=$(grep "Maximum resident set size" "$log_file" | awk '{print $6}' || echo "0")
    local max_rss_mb=$(awk "BEGIN {printf \"%.2f\", $max_rss_kb / 1024}")
    local exit_code=$(grep "Exit status:" "$log_file" | awk '{print $NF}' || echo "0")

    echo "$wall_sec,$user_sec,$sys_sec,$cpu_pct,$max_rss_mb,$exit_code"
}

extract_bio_metrics() {
    local log_file="$1"

    if [[ ! -f "$log_file" ]]; then
        echo "0,0"
        return 1
    fi

    local iterations=$(grep -oP "iteration: \K[0-9]+" "$log_file" | tail -1 || echo "0")
    local final_cells=$(grep -oP "nb cells \K[0-9]+" "$log_file" | tail -1 || echo "0")
    echo "$iterations,$final_cells"
}

################################################################################
# ENHANCED METRICS CALCULATION
################################################################################

calculate_enhanced_metrics() {
    local wall_time="$1"
    local user_time="$2"
    local sys_time="$3"
    local cpu_pct="$4"
    local threads="$5"
    local iterations="$6"
    local final_cells="$7"
    local initial_cells="${8:-0}"

    # Thread efficiency: actual CPU% / theoretical max (threads × 100%)
    local thread_efficiency=0
    if [[ $threads -gt 0 ]]; then
        thread_efficiency=$(awk "BEGIN {printf \"%.2f\", $cpu_pct / ($threads * 100)}")
    fi

    # Parallel efficiency: (user_time / wall_time) / threads
    local parallel_efficiency=0
    if [[ $(awk "BEGIN {print ($wall_time > 0 && $threads > 0)}") -eq 1 ]]; then
        parallel_efficiency=$(awk "BEGIN {printf \"%.2f\", ($user_time / $wall_time) / $threads}")
    fi

    # I/O overhead: sys_time / wall_time ratio
    local io_overhead=0
    if [[ $(awk "BEGIN {print ($wall_time > 0)}") -eq 1 ]]; then
        io_overhead=$(awk "BEGIN {printf \"%.4f\", $sys_time / $wall_time}")
    fi

    # Iterations per second
    local iters_per_sec=0
    if [[ $(awk "BEGIN {print ($wall_time > 0)}") -eq 1 ]]; then
        iters_per_sec=$(awk "BEGIN {printf \"%.2f\", $iterations / $wall_time}")
    fi

    # Cells per second
    local cells_per_sec=0
    if [[ $(awk "BEGIN {print ($wall_time > 0)}") -eq 1 ]]; then
        cells_per_sec=$(awk "BEGIN {printf \"%.2f\", $final_cells / $wall_time}")
    fi

    # Biology score: (iterations × cells) / wall_time
    local biology_score=0
    if [[ $(awk "BEGIN {print ($wall_time > 0)}") -eq 1 ]]; then
        biology_score=$(awk "BEGIN {printf \"%.2f\", ($iterations * $final_cells) / $wall_time}")
    fi

    # Growth factor
    local growth_factor=1.0
    if [[ $initial_cells -gt 0 ]]; then
        growth_factor=$(awk "BEGIN {printf \"%.2f\", $final_cells / $initial_cells}")
    fi

    echo "$thread_efficiency,$parallel_efficiency,$io_overhead,$iters_per_sec,$cells_per_sec,$biology_score,$growth_factor"
}

################################################################################
# SYSTEM RESOURCE DETECTION
################################################################################

detect_system_resources() {
    # CPU Detection
    export TOTAL_CORES=$(nproc)
    export PHYSICAL_CORES=$(lscpu | grep "^Core(s) per socket:" | awk '{print $NF}')
    export SOCKETS=$(lscpu | grep "^Socket(s):" | awk '{print $NF}')
    export THREADS_PER_CORE=$(lscpu | grep "^Thread(s) per core:" | awk '{print $NF}')

    # Calculate effective cores (ignore hyperthreading)
    export EFFECTIVE_CORES=$((PHYSICAL_CORES * SOCKETS))

    # NUMA topology detection
    if command -v numactl &> /dev/null; then
        export NUMA_NODES=$(numactl -H | grep "available:" | awk '{print $2}')
        export NUMA_AVAILABLE=true
    else
        export NUMA_NODES=1
        export NUMA_AVAILABLE=false
    fi

    # Memory detection (in GB)
    export TOTAL_RAM_GB=$(free -g | awk '/^Mem:/ {print $2}')
    export AVAILABLE_RAM_GB=$(free -g | awk '/^Mem:/ {print $7}')

    # Disk space
    export DISK_AVAILABLE_GB=$(df -BG . | tail -1 | awk '{print $4}' | tr -d 'G')

    # CPU model and cache
    export CPU_MODEL=$(lscpu | grep "Model name:" | cut -d':' -f2 | xargs)
    export L3_CACHE_KB=$(lscpu | grep "L3 cache:" | awk '{print $3}' | tr -d 'K' || echo "0")

    log "SYSTEM RESOURCES DETECTED:"
    log "  Physical cores: $EFFECTIVE_CORES (${PHYSICAL_CORES} per socket × ${SOCKETS} sockets)"
    log "  Hyperthreading: $THREADS_PER_CORE threads/core"
    log "  NUMA nodes: $NUMA_NODES"
    log "  RAM: ${AVAILABLE_RAM_GB}GB available / ${TOTAL_RAM_GB}GB total"
    log "  Disk: ${DISK_AVAILABLE_GB}GB available"
    log "  CPU: $CPU_MODEL"

    # Validation
    if [[ $AVAILABLE_RAM_GB -lt 4 ]]; then
        error "Insufficient RAM: ${AVAILABLE_RAM_GB}GB < 4GB minimum"
        return 1
    fi
}

################################################################################
# RESOURCE ALLOCATION
################################################################################

allocate_resources() {
    local mode_count="${1:-4}"  # Default to 4 modes
    local threads_per_mode="${2:-auto}"  # Default to auto
    local parallel="${3:-true}"  # Default to parallel execution

    # Declare global arrays
    declare -gA CORE_ASSIGNMENT

    # Safety check
    if [[ $EFFECTIVE_CORES -lt $mode_count ]] && [[ "$parallel" == "true" ]]; then
        warn "Insufficient cores for parallel: ${EFFECTIVE_CORES} < ${mode_count} modes"
        warn "Switching to sequential execution"
        parallel="false"
    fi

    # Calculate allocation
    if [[ "$parallel" == "true" ]]; then
        export CORES_PER_MODE=$((EFFECTIVE_CORES / mode_count))
        export ALLOCATION_STRATEGY="parallel"
    else
        export CORES_PER_MODE=$EFFECTIVE_CORES
        export ALLOCATION_STRATEGY="sequential"
    fi

    # Thread count selection
    if [[ "$threads_per_mode" == "auto" ]]; then
        # Use empirical finding: 4 threads optimal (59-69% efficiency)
        if [[ $CORES_PER_MODE -ge 4 ]]; then
            export THREADS_PER_MODE=4
        else
            export THREADS_PER_MODE=$CORES_PER_MODE
        fi
    else
        export THREADS_PER_MODE=$threads_per_mode
    fi

    log "RESOURCE ALLOCATION:"
    log "  Strategy: $ALLOCATION_STRATEGY"
    log "  Cores per mode: $CORES_PER_MODE"
    log "  Threads per mode: $THREADS_PER_MODE"
    log "  Mode count: $mode_count"
}

allocate_cores_to_modes() {
    local modes=("$@")
    local mode_count=${#modes[@]}

    if [[ "$ALLOCATION_STRATEGY" == "sequential" ]]; then
        # All modes get all cores (run one at a time)
        for mode in "${modes[@]}"; do
            CORE_ASSIGNMENT[$mode]="0-$((EFFECTIVE_CORES - 1))"
        done
        log "  Sequential allocation: All modes use cores 0-$((EFFECTIVE_CORES - 1))"
    else
        # Parallel: Distribute cores evenly
        local start_core=0
        for mode in "${modes[@]}"; do
            local end_core=$((start_core + CORES_PER_MODE - 1))
            CORE_ASSIGNMENT[$mode]="${start_core}-${end_core}"
            log "  ${mode}: cores ${start_core}-${end_core}"
            start_core=$((end_core + 1))
        done
    fi
}

################################################################################
# CHECKPOINT / RESUME
################################################################################

save_checkpoint() {
    local checkpoint_file="$1"
    local current_config="$2"
    shift 2
    local completed_configs=("$@")

    # Create JSON checkpoint
    cat > "$checkpoint_file" <<EOF
{
  "timestamp": "$(date -Iseconds)",
  "current_config": "$current_config",
  "completed_configs": [
$(printf '    "%s"' "${completed_configs[0]}")
$(for cfg in "${completed_configs[@]:1}"; do printf ',\n    "%s"' "$cfg"; done)
  ],
  "system_info": {
    "cores": $EFFECTIVE_CORES,
    "ram_gb": $TOTAL_RAM_GB,
    "hostname": "$(hostname)"
  }
}
EOF

    log "Checkpoint saved: $checkpoint_file"
}

load_checkpoint() {
    local checkpoint_file="$1"

    if [[ ! -f "$checkpoint_file" ]]; then
        error "Checkpoint file not found: $checkpoint_file"
        return 1
    fi

    log "Loading checkpoint: $checkpoint_file"

    # Parse JSON (requires jq or manual parsing)
    if command -v jq &> /dev/null; then
        export CHECKPOINT_CURRENT=$(jq -r '.current_config' "$checkpoint_file")
        mapfile -t CHECKPOINT_COMPLETED < <(jq -r '.completed_configs[]' "$checkpoint_file")
    else
        error "jq not installed - cannot parse checkpoint"
        return 1
    fi

    log "  Last config: $CHECKPOINT_CURRENT"
    log "  Completed: ${CHECKPOINT_COMPLETED[*]}"
}

################################################################################
# UTILITIES
################################################################################

format_duration() {
    local seconds="$1"
    local hours=$((seconds / 3600))
    local mins=$(((seconds % 3600) / 60))
    local secs=$((seconds % 60))
    printf "%dh %dm %ds" "$hours" "$mins" "$secs"
}

extract_xml_value() {
    local xml_file="$1"
    local tag="$2"
    grep -oP "<${tag}>.*?\K[^<]+" "$xml_file" | head -1 || echo ""
}

is_simulation_running() {
    local pid="$1"
    if ps -p "$pid" > /dev/null 2>&1; then
        return 0  # Running
    else
        return 1  # Not running
    fi
}

wait_for_simulations() {
    local pids=("$@")
    local failed=0

    for pid in "${pids[@]}"; do
        if wait "$pid"; then
            log "  PID $pid completed successfully"
        else
            local exit_code=$?
            error "  PID $pid failed with exit code $exit_code"
            ((failed++))
        fi
    done

    return $failed
}

################################################################################
# VALIDATION
################################################################################

validate_parameter_file() {
    local param_file="$1"

    if [[ ! -f "$param_file" ]]; then
        error "Parameter file not found: $param_file"
        return 1
    fi

    if [[ ! "$param_file" =~ \.xml$ ]]; then
        error "Parameter file must be .xml: $param_file"
        return 1
    fi

    # Basic XML syntax check
    if ! grep -q "<simulation_parameters>" "$param_file"; then
        error "Invalid XML: missing <simulation_parameters> tag"
        return 1
    fi

    return 0
}

################################################################################
# ENVIRONMENT VALIDATION
################################################################################

# Validate entire environment before starting benchmark
# Returns: 0 if all checks pass, 1 if any fail
validate_environment() {
    local errors=0

    log "ENVIRONMENT VALIDATION:"
    log ""

    # Check 1: Simucell3d binary exists
    if [[ ! -f "./simucell3d" ]]; then
        error "  ✗ simucell3d binary not found (must run from build/)"
        ((errors++))
    else
        log "  ✓ simucell3d binary found"
    fi

    # Check 2: Parameter files exist (for core mode)
    if [[ -n "${CORE_CONFIGS:-}" && ${#CORE_CONFIGS[@]} -gt 0 ]]; then
        log "  Parameter files:"
        for config in "${CORE_CONFIGS[@]}"; do
            # Use same logic as run_parallel_config
            local param_suffix=""
            if [[ "$config" == "tube" || "$config" == "sheet" || "$config" == "benchmark" || "$config" == "vesicle_realistic_biology" ]]; then
                param_suffix="_fast"
            fi
            local param_file="${PARAM_DIR_CORE:-../parameters/benchmarking}/parameters_${config}${param_suffix}.xml"

            if [[ -f "$param_file" ]]; then
                log "    ✓ $config"
            else
                error "    ✗ $config: $param_file not found"
                ((errors++))
            fi
        done
    fi

    # Check 3: Disk space
    local available=$(get_disk_space_gb)
    local required="${DISK_SPACE_MIN_GB:-20}"
    if [[ $available -lt $required ]]; then
        error "  ✗ Insufficient disk space: ${available}GB < ${required}GB"
        ((errors++))
    else
        log "  ✓ Disk space: ${available}GB available (required: ${required}GB)"
    fi

    # Check 4: Output directory writable
    local output_dir="${OUTPUT_BASE_DIR:-./doc/working}"
    if [[ ! -w "$output_dir" ]]; then
        if [[ ! -d "$output_dir" ]]; then
            # Try to create it
            if mkdir -p "$output_dir" 2>/dev/null; then
                log "  ✓ Output directory created: $output_dir"
            else
                error "  ✗ Cannot create output directory: $output_dir"
                ((errors++))
            fi
        else
            error "  ✗ Output directory not writable: $output_dir"
            ((errors++))
        fi
    else
        log "  ✓ Output directory writable: $output_dir"
    fi

    # Check 5: Memory (RAM)
    if [[ -n "${AVAILABLE_RAM_GB:-}" ]]; then
        if [[ $AVAILABLE_RAM_GB -lt 4 ]]; then
            error "  ✗ Insufficient RAM: ${AVAILABLE_RAM_GB}GB < 4GB minimum"
            ((errors++))
        else
            log "  ✓ RAM: ${AVAILABLE_RAM_GB}GB available"
        fi
    fi

    # Check 6: Optional dependencies
    if [[ "${ENABLE_REPORT:-false}" == "true" ]]; then
        if command -v python3 &> /dev/null; then
            if python3 -c "import pandas, matplotlib, seaborn" 2>/dev/null; then
                log "  ✓ Report generation dependencies available (python3, pandas, matplotlib, seaborn)"
            else
                warn "  ⚠ Python packages missing (pandas, matplotlib, seaborn) - reports will fail"
                warn "    Install with: pip3 install pandas matplotlib seaborn"
            fi
        else
            warn "  ⚠ python3 not found - reports will fail"
        fi
    fi

    if [[ "${ENABLE_LIVE_MONITOR:-false}" == "true" ]]; then
        if command -v tmux &> /dev/null || command -v screen &> /dev/null; then
            log "  ✓ Live monitor dependencies available (tmux or screen)"
        else
            warn "  ⚠ tmux/screen not found - live monitor will fail"
            warn "    Install with: sudo apt install tmux"
        fi
    fi

    log ""
    if [[ $errors -gt 0 ]]; then
        error "Environment validation FAILED with $errors error(s)"
        return 1
    else
        log "Environment validation PASSED ✓"
        return 0
    fi
}

################################################################################
# DISK SPACE MANAGEMENT
################################################################################

# Get current available disk space in GB
get_disk_space_gb() {
    df -BG . | tail -1 | awk '{print $4}' | tr -d 'G'
}

# Check if sufficient disk space is available
# Args: $1 - required GB (default: 10)
# Returns: 0 if OK, 1 if insufficient
check_disk_space() {
    local required_gb="${1:-10}"
    local current_gb=$(get_disk_space_gb)

    if [[ $current_gb -lt $required_gb ]]; then
        error "Insufficient disk space: ${current_gb}GB available < ${required_gb}GB required"
        return 1
    fi

    log "  Disk space check: ${current_gb}GB available (required: ${required_gb}GB) ✓"
    return 0
}

# Estimate VTK space required for a simulation (conservative estimate)
# Args: $1 - parameter file path
# Returns: estimated GB via echo
estimate_vtk_space_required() {
    local param_file="$1"

    # Parse key parameters from XML
    local sim_duration=$(extract_xml_value "$param_file" "simulation_duration" || echo "1.0")
    local sampling_period=$(extract_xml_value "$param_file" "sampling_period" || echo "0.01")

    # Estimate: (duration / sampling) × 4 modes × avg_vtk_size_mb / 1024
    # Conservative avg_vtk_size: 5 MB per file
    local num_timesteps=$(awk "BEGIN {printf \"%.0f\", $sim_duration / $sampling_period}")
    local total_files=$((num_timesteps * 4))  # 4 OpenMP modes
    local estimated_gb=$(awk "BEGIN {printf \"%.0f\", ($total_files * 5) / 1024}")

    # Minimum estimate: 5 GB
    if [[ $estimated_gb -lt 5 ]]; then
        estimated_gb=5
    fi

    echo "$estimated_gb"
}

# Clean old VTK files from all simulation results directories
# Useful for recovering disk space before starting new benchmarks
cleanup_all_old_vtk_files() {
    local sim_results_dir="${1:-$PROJECT_ROOT/simulation_results}"
    local age_days="${2:-1}"  # Default: files older than 1 day

    if [[ ! -d "$sim_results_dir" ]]; then
        return 0
    fi

    log "Cleaning VTK files older than $age_days day(s) from $sim_results_dir..."

    local count_before=$(find "$sim_results_dir" -name "*.vtk" -type f 2>/dev/null | wc -l)
    local size_before_mb=$(du -sm "$sim_results_dir" 2>/dev/null | awk '{print $1}')

    # Find and delete old VTK files
    find "$sim_results_dir" -name "*.vtk" -type f -mtime "+$age_days" -delete 2>/dev/null

    local count_after=$(find "$sim_results_dir" -name "*.vtk" -type f 2>/dev/null | wc -l)
    local size_after_mb=$(du -sm "$sim_results_dir" 2>/dev/null | awk '{print $1}')
    local removed=$((count_before - count_after))
    local saved_mb=$((size_before_mb - size_after_mb))

    if [[ $removed -gt 0 ]]; then
        log "  ✓ Removed $removed old VTK files, saved ${saved_mb}MB"
    else
        log "  No old VTK files found to remove"
    fi
}

# Check disk space and perform emergency cleanup if below critical threshold
# Returns 0 if space is sufficient, 1 if cleanup was needed and failed
check_and_cleanup_if_critical() {
    local context="${1:-simulation}"  # Context for logging (e.g., "simulation", "config")

    if [[ "${ENABLE_DISK_CHECKS:-true}" != "true" ]]; then
        return 0  # Disk checks disabled
    fi

    local available=$(get_disk_space_gb)
    local critical_threshold="${DISK_SPACE_CRITICAL_GB:-10}"

    # Check if we're below critical threshold
    if (( $(awk "BEGIN {print ($available < $critical_threshold)}") )); then
        warn "⚠ CRITICAL: Disk space ${available}GB < ${critical_threshold}GB threshold before $context"
        log "  Performing emergency VTK cleanup (all ages)..."

        # Aggressive cleanup: remove ALL old VTK files regardless of age
        cleanup_all_old_vtk_files "$PROJECT_ROOT/simulation_results" 0

        # Recheck disk space after cleanup
        local available_after=$(get_disk_space_gb)
        log "  After cleanup: ${available_after}GB available (gained $((available_after - available))GB)"

        # Check if cleanup was sufficient
        local min_threshold="${DISK_SPACE_MIN_GB:-20}"
        if (( $(awk "BEGIN {print ($available_after < $min_threshold)}") )); then
            error "EMERGENCY: Still insufficient space after cleanup (${available_after}GB < ${min_threshold}GB required)"
            return 1
        fi

        log "  ✓ Emergency cleanup successful, continuing..."
    fi

    return 0
}

################################################################################
# EXPORT ALL FUNCTIONS
################################################################################

export -f log error warn
export -f extract_metrics extract_bio_metrics calculate_enhanced_metrics
export -f detect_system_resources allocate_resources allocate_cores_to_modes
export -f save_checkpoint load_checkpoint
export -f format_duration extract_xml_value is_simulation_running wait_for_simulations
export -f validate_parameter_file validate_environment
export -f get_disk_space_gb check_disk_space estimate_vtk_space_required cleanup_all_old_vtk_files check_and_cleanup_if_critical
