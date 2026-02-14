#!/bin/bash
#===============================================================================
# SimuCell3D Parallel Benchmark: v1.0 vs Next-static vs Next-adaptive
#
# PURPOSE:
#   Run multiple simulations in parallel with equal resources.
#   Collect comprehensive biological & computational metrics.
#   Support auto-restart, checkpoint/resume for multi-day runs.
#
# USAGE:
#   ./run_parallel_benchmark_comparison.sh [OPTIONS]
#
# QUICK START:
#   ./run_parallel_benchmark_comparison.sh --modes=static,adaptive
#   ./run_parallel_benchmark_comparison.sh --dry-run --all-modes
#   ./run_parallel_benchmark_comparison.sh --quick-test --only=adaptive
#
# REQUIREMENTS:
#   CPU cores (48 ideal, adapts to available), RAM, disk space
#===============================================================================

set -euo pipefail

#===============================================================================
# SECTION 1: SCRIPT PATHS AND LIBRARY IMPORTS
#===============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Source utility libraries
if [[ ! -f "$SCRIPT_DIR/lib/benchmark_utils.sh" ]]; then
    echo "ERROR: benchmark_utils.sh not found at $SCRIPT_DIR/lib/benchmark_utils.sh" >&2
    exit 1
fi
source "$SCRIPT_DIR/lib/benchmark_utils.sh"

if [[ ! -f "$SCRIPT_DIR/lib/parallel_benchmark_utils.sh" ]]; then
    echo "ERROR: parallel_benchmark_utils.sh not found at $SCRIPT_DIR/lib/parallel_benchmark_utils.sh" >&2
    exit 1
fi
source "$SCRIPT_DIR/lib/parallel_benchmark_utils.sh"

#===============================================================================
# SECTION 2: DEFAULT CONFIGURATION
#
# These values can be overridden via CLI arguments or environment variables.
# Configuration is organized by category for clarity.
#===============================================================================

# --- Path Configuration ---
# V1_ROOT: Location of v1.0 SimuCell3D installation
# Priority: --v1-root CLI flag > V1_ROOT env var > hardcoded default
V1_ROOT_DEFAULT="/home/nilesh-patil/projects/version-cpp-v1.0"
V1_ROOT="${V1_ROOT:-$V1_ROOT_DEFAULT}"
NEXT_ROOT="$PROJECT_ROOT"

# --- Simulation Parameters ---
PARAM_FILE="parameters/parameters_paper_exact_128k.xml"

# --- Resource Allocation ---
# Cores per simulation (adjusted automatically based on available cores)
CORES_PER_SIM=16
FORCE_CORES_PER_SIM=""  # Override via --cores-per-sim=N
V1_CORES=""
STATIC_CORES=""
ADAPTIVE_CORES=""

# --- Monitoring Configuration ---
MONITOR_INTERVAL_SEC=60

# --- Feature Flags ---
# All features default to sensible values for typical usage
DRY_RUN=false           # Show plan without running
QUICK_TEST=false        # 1-minute timeout for testing setup
ENABLE_AUTO_RESTART=true
CLEAN_REBUILD=false     # Default OFF - opt-in with --clean-rebuild
RESUME_PATH=""

# --- Mode Selection ---
# Valid modes that can be run
VALID_MODES=("v1" "static" "adaptive")
ENABLED_MODES=()        # Populated by resolve_enabled_modes()
MODE_ARG=""             # Raw --modes= argument
SKIP_V1=false           # --skip-v1 flag
ONLY_MODE=""            # --only= argument
ALL_MODES=false         # --all-modes flag


# --- Runtime State (populated during execution) ---
OUTPUT_BASE=""
METRICS_DIR=""
LOGS_DIR=""
STATE_DIR=""
START_TIME=""
START_EPOCH=""

# --- Process Tracking ---
declare -A SIM_PIDS=()
declare -A SIM_STATUS=()

#===============================================================================
# SECTION 3: HELP AND CLI ARGUMENT PARSING
#===============================================================================

show_help() {
    cat <<EOF
SimuCell3D Parallel Benchmark Comparison

USAGE:
    $(basename "$0") [OPTIONS]

GENERAL OPTIONS:
    -h, --help              Show this help message
    --dry-run               Show execution plan without running simulations
    --quick-test            Run with 1-minute timeout for testing setup
    --resume=DIR            Resume interrupted benchmark from directory

CONFIGURATION OPTIONS:
    --param-file=PATH       Override default parameter file
                            (default: $PARAM_FILE)
    --v1-root=PATH          Path to v1.0 SimuCell3D installation
                            (default: \$V1_ROOT env var or $V1_ROOT_DEFAULT)
    --monitor-interval=N    Metric collection interval in seconds
                            (default: $MONITOR_INTERVAL_SEC)

BUILD OPTIONS:
    --clean-rebuild         Clean rebuild binaries for ENABLED modes before running
                            (default: OFF - uses existing binaries)
    --no-auto-restart       Disable automatic restart on crash/stall
    --cores-per-sim=N       Force exactly N cores per simulation
                            (default: auto-calculated based on available cores)

MODE SELECTION:
    --modes=MODE1,MODE2,... Comma-separated list of modes to run
                            Valid modes: v1, static, adaptive
    --skip-v1               Exclude v1.0 from the benchmark (even if in --modes)
    --only=MODE             Run only a single mode
    --all-modes             Run all three modes (v1, static, adaptive)

    Default: runs v1 and adaptive (most common comparison).
    If v1.0 binary not found, it is automatically skipped with a warning.

EXAMPLES:
    # Run static and adaptive comparison (skip v1.0)
    $(basename "$0") --modes=static,adaptive

    # Run all three modes with clean rebuild
    $(basename "$0") --all-modes --clean-rebuild

    # Quick test to verify setup
    $(basename "$0") --quick-test --only=adaptive

    # Dry run to see what would happen
    $(basename "$0") --dry-run --all-modes

    # Resume an interrupted run
    $(basename "$0") --resume=doc/working/parallel_benchmark_20241127_100000

    # Use custom v1.0 path
    $(basename "$0") --v1-root=/path/to/v1.0 --all-modes

OUTPUT:
    Creates directory: doc/working/parallel_benchmark_TIMESTAMP/
    With metrics CSVs, logs, and checkpoint files for analysis.

ENVIRONMENT VARIABLES:
    V1_ROOT     Path to v1.0 SimuCell3D (overridden by --v1-root)

EOF
    exit 0
}

parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            # --- General Options ---
            -h|--help)
                show_help
                ;;
            --dry-run)
                DRY_RUN=true
                shift
                ;;
            --quick-test)
                QUICK_TEST=true
                shift
                ;;
            --resume=*)
                RESUME_PATH="${1#*=}"
                shift
                ;;

            # --- Configuration Options ---
            --param-file=*)
                PARAM_FILE="${1#*=}"
                shift
                ;;
            --v1-root=*)
                V1_ROOT="${1#*=}"
                shift
                ;;
            --monitor-interval=*)
                MONITOR_INTERVAL_SEC="${1#*=}"
                shift
                ;;

            # --- Build Options ---
            --clean-rebuild)
                CLEAN_REBUILD=true
                shift
                ;;
            --no-auto-restart)
                ENABLE_AUTO_RESTART=false
                shift
                ;;
            --cores-per-sim=*)
                FORCE_CORES_PER_SIM="${1#*=}"
                if ! [[ "$FORCE_CORES_PER_SIM" =~ ^[0-9]+$ ]] || [[ "$FORCE_CORES_PER_SIM" -lt 1 ]]; then
                    error "--cores-per-sim must be a positive integer"
                    exit 1
                fi
                shift
                ;;

            # --- Mode Selection ---
            --modes=*)
                MODE_ARG="${1#*=}"
                shift
                ;;
            --skip-v1)
                SKIP_V1=true
                shift
                ;;
            --only=*)
                ONLY_MODE="${1#*=}"
                shift
                ;;
            --all-modes)
                ALL_MODES=true
                shift
                ;;

            # --- Unknown ---
            *)
                error "Unknown option: $1"
                echo "Use --help for usage information."
                exit 1
                ;;
        esac
    done
}

#===============================================================================
# SECTION 4: MODE SELECTION LOGIC
#
# This section handles which simulation modes (v1, static, adaptive) to run.
# The resolution follows a priority order:
#   1. --only=MODE (single mode)
#   2. --modes=MODE1,MODE2 (explicit list)
#   3. --all-modes (all three)
#   4. Default: v1 and adaptive
# Then --skip-v1 filters out v1 from whatever was selected.
#===============================================================================

# Check if a mode name is valid
validate_mode() {
    local mode="$1"
    for valid in "${VALID_MODES[@]}"; do
        if [[ "$mode" == "$valid" ]]; then
            return 0
        fi
    done
    return 1
}

# Check if a mode is in ENABLED_MODES array
mode_is_enabled() {
    local mode="$1"
    for enabled in "${ENABLED_MODES[@]}"; do
        if [[ "$mode" == "$enabled" ]]; then
            return 0
        fi
    done
    return 1
}

# Resolve which modes to run based on CLI arguments
resolve_enabled_modes() {
    log "Resolving benchmark modes..."

    # Priority 1: --only takes precedence over everything
    if [[ -n "$ONLY_MODE" ]]; then
        if ! validate_mode "$ONLY_MODE"; then
            error "Invalid mode '$ONLY_MODE'. Valid modes: ${VALID_MODES[*]}"
            return 1
        fi
        ENABLED_MODES=("$ONLY_MODE")
        log "  --only=$ONLY_MODE: running single mode"
        return 0
    fi

    # Priority 2: --modes explicit list
    if [[ -n "$MODE_ARG" ]]; then
        IFS=',' read -ra requested_modes <<< "$MODE_ARG"
        ENABLED_MODES=()
        for mode in "${requested_modes[@]}"; do
            mode=$(echo "$mode" | tr -d '[:space:]')  # trim whitespace
            if ! validate_mode "$mode"; then
                error "Invalid mode '$mode'. Valid modes: ${VALID_MODES[*]}"
                return 1
            fi
            ENABLED_MODES+=("$mode")
        done
        log "  --modes=$MODE_ARG: ${#ENABLED_MODES[@]} modes selected"

    # Priority 3: --all-modes
    elif [[ "$ALL_MODES" == "true" ]]; then
        ENABLED_MODES=("${VALID_MODES[@]}")
        log "  --all-modes: running all three modes (v1, static, adaptive)"

    # Priority 4: Default (v1 + adaptive)
    else
        ENABLED_MODES=("v1" "adaptive")
        log "  Using default modes: v1, adaptive (use --all-modes for all three)"
    fi

    # Apply --skip-v1 filter
    if [[ "$SKIP_V1" == "true" ]]; then
        local filtered=()
        for mode in "${ENABLED_MODES[@]}"; do
            if [[ "$mode" != "v1" ]]; then
                filtered+=("$mode")
            fi
        done
        ENABLED_MODES=("${filtered[@]}")
        log "  --skip-v1: v1 mode excluded"
    fi

    # Auto-detect v1.0 availability (only filter if using default, not explicit request)
    if mode_is_enabled "v1" && [[ -z "$MODE_ARG" ]] && [[ "$SKIP_V1" != "true" ]]; then
        if [[ ! -x "$V1_ROOT/build/simucell3d" ]]; then
            warn "v1.0 binary not found at $V1_ROOT/build/simucell3d"
            warn "  Automatically skipping v1 mode"
            warn "  (Use --v1-root=PATH to specify correct location)"
            local filtered=()
            for mode in "${ENABLED_MODES[@]}"; do
                if [[ "$mode" != "v1" ]]; then
                    filtered+=("$mode")
                fi
            done
            ENABLED_MODES=("${filtered[@]}")
        fi
    fi

    # Validate at least one mode remains
    if [[ ${#ENABLED_MODES[@]} -eq 0 ]]; then
        error "No modes enabled! At least one mode must be selected."
        return 1
    fi

    # Error if v1 was explicitly requested but not available
    if mode_is_enabled "v1" && [[ ! -x "$V1_ROOT/build/simucell3d" ]]; then
        if [[ -n "$MODE_ARG" ]] || [[ -n "$ONLY_MODE" ]]; then
            error "v1 mode explicitly requested but binary not found: $V1_ROOT/build/simucell3d"
            error "Use --v1-root=PATH to specify correct location, or remove v1 from --modes"
            return 1
        fi
    fi

    log "  Enabled modes: ${ENABLED_MODES[*]}"
}

#===============================================================================
# SECTION 5: INITIALIZATION AND PREREQUISITES
#
# This section validates the environment, checks dependencies, and creates
# output directories. Clean rebuild logic respects enabled modes.
#===============================================================================

sanity_check() {
    local param_file="$1"

    log "Running sanity check..."

    # Check file exists
    if [[ ! -f "$param_file" ]]; then
        error "Parameter file not found: $param_file"
        return 1
    fi

    # Check mesh file path exists
    local mesh_path
    mesh_path=$(grep -oP '<input_mesh_file_path>\K[^<]+' "$param_file" || true)
    if [[ -n "$mesh_path" ]]; then
        local full_path="$PROJECT_ROOT/$mesh_path"
        if [[ ! -f "$full_path" ]] && [[ ! -f "$mesh_path" ]]; then
            warn "Mesh file may not exist: $mesh_path"
        fi
    fi

    # Quick 10-iteration test with NEXT binary
    log "  Testing parameter file with quick simulation..."
    if ! timeout 30 "$NEXT_ROOT/build/simucell3d" --max-iter=10 "$param_file" > /dev/null 2>&1; then
        error "Sanity check failed - parameter file may cause crash"
        return 1
    fi

    log "  Sanity check: PASSED"
}

# Clean rebuild binaries for ENABLED modes only
perform_clean_rebuild() {
    log "Clean rebuilding binaries for enabled modes..."

    # Rebuild v1.0 only if v1 mode is enabled
    if mode_is_enabled "v1"; then
        log "  Rebuilding v1.0 at $V1_ROOT..."
        if [[ ! -d "$V1_ROOT/build" ]]; then
            error "v1.0 build directory not found: $V1_ROOT/build"
            return 1
        fi
        (cd "$V1_ROOT/build" && rm -rf ./* && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j16) || {
            error "v1.0 clean rebuild failed"
            return 1
        }
        log "    v1.0 rebuild: COMPLETE"
    else
        log "  Skipping v1.0 rebuild (v1 mode not enabled)"
    fi

    # Rebuild NEXT only if static or adaptive modes are enabled
    if mode_is_enabled "static" || mode_is_enabled "adaptive"; then
        log "  Rebuilding NEXT at $NEXT_ROOT..."
        if [[ ! -d "$NEXT_ROOT/build" ]]; then
            error "NEXT build directory not found: $NEXT_ROOT/build"
            return 1
        fi
        (cd "$NEXT_ROOT/build" && rm -rf ./* && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j16) || {
            error "NEXT clean rebuild failed"
            return 1
        }
        log "    NEXT rebuild: COMPLETE"
    else
        log "  Skipping NEXT rebuild (no static/adaptive modes enabled)"
    fi

    log "  Clean rebuild: COMPLETE"
}

check_prerequisites() {
    log "Checking prerequisites..."

    # Resolve modes first (affects which binaries we check)
    resolve_enabled_modes || return 1

    # Check CPU cores available
    local total_cores num_modes
    total_cores=$(nproc)
    num_modes=${#ENABLED_MODES[@]}

    # Core allocation: use forced value or auto-calculate
    if [[ -n "$FORCE_CORES_PER_SIM" ]]; then
        CORES_PER_SIM=$FORCE_CORES_PER_SIM
        local required_cores=$((CORES_PER_SIM * num_modes))
        if [[ $total_cores -lt $required_cores ]]; then
            error "Requested $CORES_PER_SIM cores/sim × $num_modes modes = $required_cores cores, but only $total_cores available"
            return 1
        fi
        log "  Using forced core allocation: $CORES_PER_SIM cores per simulation"
    else
        # Dynamic core allocation based on number of modes
        local ideal_total=$((16 * num_modes))
        if [[ $total_cores -lt $ideal_total ]]; then
            warn "Only $total_cores cores available (ideal for $num_modes modes: $ideal_total)"
            warn "Adjusting core allocation..."
            CORES_PER_SIM=$((total_cores / num_modes))
        else
            CORES_PER_SIM=$((total_cores / num_modes))
            # Cap at 16 for cache efficiency
            [[ $CORES_PER_SIM -gt 16 ]] && CORES_PER_SIM=16
        fi
    fi

    # Calculate core ranges based on enabled modes
    log "  Core allocation (${num_modes} modes, ${CORES_PER_SIM} cores each):"
    local core_start=0
    for mode in "${ENABLED_MODES[@]}"; do
        local core_end=$((core_start + CORES_PER_SIM - 1))
        case "$mode" in
            v1)       V1_CORES="$core_start-$core_end" ;;
            static)   STATIC_CORES="$core_start-$core_end" ;;
            adaptive) ADAPTIVE_CORES="$core_start-$core_end" ;;
        esac
        log "    $mode: cores $core_start-$core_end"
        core_start=$((core_end + 1))
    done

    # Check builds exist (only for enabled modes)
    if mode_is_enabled "v1"; then
        if [[ ! -x "$V1_ROOT/build/simucell3d" ]]; then
            error "v1.0 build not found: $V1_ROOT/build/simucell3d"
            error "Use --v1-root=PATH to specify correct location"
            return 1
        fi
        log "  v1.0 binary: OK ($V1_ROOT/build/simucell3d)"
    fi

    if mode_is_enabled "static" || mode_is_enabled "adaptive"; then
        if [[ ! -x "$NEXT_ROOT/build/simucell3d" ]]; then
            error "NEXT build not found: $NEXT_ROOT/build/simucell3d"
            return 1
        fi
        log "  NEXT binary: OK ($NEXT_ROOT/build/simucell3d)"
    fi

    # Check parameter file
    if [[ ! -f "$NEXT_ROOT/$PARAM_FILE" ]]; then
        error "Parameter file not found: $NEXT_ROOT/$PARAM_FILE"
        return 1
    fi
    log "  Parameter file: OK"

    # Check disk space
    check_disk_space 20 || return 1

    log "Prerequisites check: PASSED"

    # Clean rebuild if requested (now respects enabled modes)
    if [[ "$CLEAN_REBUILD" == "true" ]]; then
        perform_clean_rebuild || return 1
    fi
}

init_directories() {
    log "Creating output directories..."

    local timestamp
    timestamp=$(date +%Y%m%d_%H%M%S)
    OUTPUT_BASE="$NEXT_ROOT/doc/working/parallel_benchmark_$timestamp"
    METRICS_DIR="$OUTPUT_BASE/metrics"
    LOGS_DIR="$OUTPUT_BASE/logs"
    STATE_DIR="$OUTPUT_BASE/state"

    # Create base directories
    mkdir -p "$LOGS_DIR"
    mkdir -p "$STATE_DIR"
    mkdir -p "$METRICS_DIR"

    # Create directories only for enabled modes
    for sim in "${ENABLED_MODES[@]}"; do
        mkdir -p "$METRICS_DIR/$sim"
        mkdir -p "$OUTPUT_BASE/sim_$sim"
    done

    # Initialize health log CSV only (other metrics come from simulation output)
    init_health_log_csv "$METRICS_DIR/health_log.csv"

    # Record metadata
    record_benchmark_metadata "$OUTPUT_BASE/metadata.json" \
        "$PARAM_FILE" "$CORES_PER_SIM" "$MONITOR_INTERVAL_SEC"

    # Save enabled modes to state for resume functionality
    printf '%s\n' "${ENABLED_MODES[@]}" > "$STATE_DIR/enabled_modes"

    # Set log file for benchmark_utils
    LOG_FILE="$LOGS_DIR/benchmark.log"

    log "Output directory: $OUTPUT_BASE"
    log "Enabled modes: ${ENABLED_MODES[*]}"

    # Validate critical directories were created
    if [[ ! -d "$METRICS_DIR" ]]; then
        error "CRITICAL: Failed to create metrics directory: $METRICS_DIR"
        return 1
    fi
    for sim in "${ENABLED_MODES[@]}"; do
        if [[ ! -d "$METRICS_DIR/$sim" ]]; then
            error "CRITICAL: Failed to create metrics subdirectory: $METRICS_DIR/$sim"
            return 1
        fi
        # Note: performance_diagnostics.csv is created by simulations, not the script
    done
    log "  ✓ Metrics directory structure validated"
}

init_from_resume() {
    log "Resuming from: $RESUME_PATH"

    if [[ ! -d "$RESUME_PATH" ]]; then
        error "Resume directory not found: $RESUME_PATH"
        return 1
    fi

    OUTPUT_BASE="$RESUME_PATH"
    METRICS_DIR="$OUTPUT_BASE/metrics"
    LOGS_DIR="$OUTPUT_BASE/logs"
    STATE_DIR="$OUTPUT_BASE/state"
    LOG_FILE="$LOGS_DIR/benchmark.log"

    # Load saved enabled modes
    if [[ -f "$STATE_DIR/enabled_modes" ]]; then
        mapfile -t ENABLED_MODES < "$STATE_DIR/enabled_modes"
        log "Loaded enabled modes from checkpoint: ${ENABLED_MODES[*]}"
    fi

    # Load checkpoint
    local checkpoint_file="$OUTPUT_BASE/checkpoint.json"
    if [[ -f "$checkpoint_file" ]]; then
        load_parallel_checkpoint "$checkpoint_file"
        log "Checkpoint loaded successfully"
    else
        warn "No checkpoint file found, starting fresh metrics collection"
    fi

    log "Resumed. Output directory: $OUTPUT_BASE"
}

#===============================================================================
# SECTION 6: SIMULATION LAUNCHERS
#
# Each simulation type (v1, static, adaptive) has its own launcher function.
# All launchers:
#   - Set up OpenMP environment variables
#   - Use taskset for CPU affinity
#   - Use nohup for background execution
#   - Write PID to state directory for tracking
#===============================================================================

prepare_v1_param_file() {
    local param_basename
    param_basename=$(basename "$PARAM_FILE")
    local v1_param="$V1_ROOT/parameters/$param_basename"

    # Check if parameter file exists in v1.0
    if [[ ! -f "$v1_param" ]]; then
        # Copy from NEXT if not present
        mkdir -p "$V1_ROOT/parameters"
        cp "$NEXT_ROOT/$PARAM_FILE" "$v1_param"
        log "  Copied parameter file to v1.0: $v1_param"
    fi

    # Create temp file with only output path modified (no mesh path changes)
    # Mesh paths work unchanged because v1.0/data is a symlink to next/data
    local temp_param="$V1_ROOT/parameters/${param_basename%.xml}_benchmark.xml"
    cp "$v1_param" "$temp_param"

    # Only update output path (mesh paths stay unchanged - rely on symlink)
    sed -i "s|<output_mesh_folder_path>.*</output_mesh_folder_path>|<output_mesh_folder_path>$OUTPUT_BASE/sim_v1</output_mesh_folder_path>|g" "$temp_param"

    echo "$temp_param"
}

launch_v1_simulation() {
    log "Launching v1.0 simulation..."

    local param_file
    param_file=$(prepare_v1_param_file)

    if [[ "$DRY_RUN" == "true" ]]; then
        log "[DRY-RUN] Would launch: taskset -c $V1_CORES ./simucell3d $param_file"
        SIM_PIDS[v1]="0"
        SIM_STATUS[v1]="dry_run"
        return
    fi

    cd "$V1_ROOT/build"

    # Set OpenMP environment
    # NOTE: V1 uses `#pragma omp parallel for` without explicit schedule clause,
    # which reads OMP_SCHEDULE env var. Set to "static" for fair comparison with
    # Next-static mode (previously was "dynamic,10" which made V1 != Static).
    export OMP_NUM_THREADS=$CORES_PER_SIM
    export OMP_WAIT_POLICY=passive
    export OMP_DYNAMIC=false
    export OMP_PROC_BIND=close
    export OMP_SCHEDULE="static"

    local timeout_cmd=""
    if [[ "$QUICK_TEST" == "true" ]]; then
        timeout_cmd="timeout 60"
    fi

    # Launch with nohup (includes --diagnostics-csv flag for metrics collection)
    # Use absolute path for param_file since we cd to build directory
    nohup $timeout_cmd taskset -c "$V1_CORES" \
        /usr/bin/time -v ./simucell3d \
        --diagnostics-csv="$OUTPUT_BASE/sim_v1/performance_diagnostics.csv" \
        "$param_file" \
        > "$LOGS_DIR/v1.log" 2>&1 &

    SIM_PIDS[v1]=$!
    SIM_STATUS[v1]="running"
    echo "${SIM_PIDS[v1]}" > "$STATE_DIR/v1.pid"

    log "  v1.0 launched: PID ${SIM_PIDS[v1]} on cores $V1_CORES"

    cd "$PROJECT_ROOT"
}

launch_static_simulation() {
    log "Launching Next-static simulation..."

    if [[ "$DRY_RUN" == "true" ]]; then
        log "[DRY-RUN] Would launch: taskset -c $STATIC_CORES ./simucell3d --schedule=static ../$PARAM_FILE"
        SIM_PIDS[static]="0"
        SIM_STATUS[static]="dry_run"
        return
    fi

    cd "$NEXT_ROOT/build"

    # Set OpenMP environment
    export OMP_NUM_THREADS=$CORES_PER_SIM
    export OMP_WAIT_POLICY=passive
    export OMP_DYNAMIC=false
    export OMP_PROC_BIND=close

    local timeout_cmd=""
    if [[ "$QUICK_TEST" == "true" ]]; then
        timeout_cmd="timeout 60"
    fi

    # Launch with diagnostics CSV enabled
    nohup $timeout_cmd taskset -c "$STATIC_CORES" \
        /usr/bin/time -v ./simucell3d \
        --schedule=static \
        --output-dir="$OUTPUT_BASE/sim_static" \
        --diagnostics-csv="$OUTPUT_BASE/sim_static/performance_diagnostics.csv" \
        "../$PARAM_FILE" \
        > "$LOGS_DIR/static.log" 2>&1 &

    SIM_PIDS[static]=$!
    SIM_STATUS[static]="running"
    echo "${SIM_PIDS[static]}" > "$STATE_DIR/static.pid"

    log "  static launched: PID ${SIM_PIDS[static]} on cores $STATIC_CORES"

    cd "$PROJECT_ROOT"
}

launch_adaptive_simulation() {
    log "Launching Next-adaptive simulation..."

    if [[ "$DRY_RUN" == "true" ]]; then
        log "[DRY-RUN] Would launch: taskset -c $ADAPTIVE_CORES ./simucell3d --schedule=adaptive ../$PARAM_FILE"
        SIM_PIDS[adaptive]="0"
        SIM_STATUS[adaptive]="dry_run"
        return
    fi

    cd "$NEXT_ROOT/build"

    # Set OpenMP environment
    export OMP_NUM_THREADS=$CORES_PER_SIM
    export OMP_WAIT_POLICY=passive
    export OMP_DYNAMIC=false
    export OMP_PROC_BIND=close

    local timeout_cmd=""
    if [[ "$QUICK_TEST" == "true" ]]; then
        timeout_cmd="timeout 60"
    fi

    # Launch with diagnostics CSV enabled
    nohup $timeout_cmd taskset -c "$ADAPTIVE_CORES" \
        /usr/bin/time -v ./simucell3d \
        --schedule=adaptive \
        --output-dir="$OUTPUT_BASE/sim_adaptive" \
        --diagnostics-csv="$OUTPUT_BASE/sim_adaptive/performance_diagnostics.csv" \
        "../$PARAM_FILE" \
        > "$LOGS_DIR/adaptive.log" 2>&1 &

    SIM_PIDS[adaptive]=$!
    SIM_STATUS[adaptive]="running"
    echo "${SIM_PIDS[adaptive]}" > "$STATE_DIR/adaptive.pid"

    log "  adaptive launched: PID ${SIM_PIDS[adaptive]} on cores $ADAPTIVE_CORES"

    cd "$PROJECT_ROOT"
}

restart_simulation() {
    local sim="$1"

    if ! can_restart "$STATE_DIR" "$sim"; then
        log_health_event "$METRICS_DIR/health_log.csv" "$sim" "ABANDONED" \
            "Max restarts ($MAX_RESTARTS) exceeded"
        SIM_STATUS[$sim]="abandoned"
        return 1
    fi

    log "Restarting $sim simulation..."
    increment_restart_count "$STATE_DIR" "$sim"

    local restart_count
    restart_count=$(get_restart_count "$STATE_DIR" "$sim")
    log_health_event "$METRICS_DIR/health_log.csv" "$sim" "RESTARTING" \
        "Attempt $restart_count of $MAX_RESTARTS"

    # Kill old process if still running (stall case)
    local old_pid="${SIM_PIDS[$sim]:-}"
    if [[ -n "$old_pid" ]] && kill -0 "$old_pid" 2>/dev/null; then
        kill -9 "$old_pid" 2>/dev/null || true
        sleep 2
    fi

    # Relaunch
    case "$sim" in
        v1)       launch_v1_simulation ;;
        static)   launch_static_simulation ;;
        adaptive) launch_adaptive_simulation ;;
    esac
}

#===============================================================================
# SECTION 7: STATUS DISPLAY
#
# Simplified monitoring - metrics are now collected directly by the simulations
# via --diagnostics-csv flag. This section only handles status display.
#===============================================================================

# Get simulation metrics from performance_diagnostics.csv (written by simulation)
get_sim_metrics_from_diagnostics() {
    local sim="$1"
    local diag_file="$OUTPUT_BASE/sim_$sim/performance_diagnostics.csv"

    if [[ ! -f "$diag_file" ]]; then
        echo "0,0,0"
        return
    fi

    # Read last line: sim_time,wall_epoch,iteration,cells,divisions,cov,phase,...,total_iteration_ms
    local last_line
    last_line=$(tail -1 "$diag_file" 2>/dev/null)

    if [[ -z "$last_line" ]] || [[ "$last_line" == "sim_time"* ]]; then
        echo "0,0,0"
        return
    fi

    local iter cells total_ms
    iter=$(echo "$last_line" | awk -F',' '{print $3}')
    cells=$(echo "$last_line" | awk -F',' '{print $4}')
    total_ms=$(echo "$last_line" | awk -F',' '{print $12}')

    # Calculate iterations per second from total_iteration_ms
    local iter_per_sec="0"
    if [[ -n "$total_ms" ]] && [[ "$total_ms" != "0" ]]; then
        iter_per_sec=$(awk "BEGIN {printf \"%.2f\", 1000 / $total_ms}")
    fi

    echo "${iter:-0},${cells:-0},${iter_per_sec:-0}"
}

#===============================================================================
# SECTION 8: HEALTH MONITORING
#
# Monitors simulation processes for crashes and stalls.
# Can automatically restart simulations if enabled.
#===============================================================================

check_all_health() {
    for sim in "${ENABLED_MODES[@]}"; do
        local pid="${SIM_PIDS[$sim]:-}"
        local status="${SIM_STATUS[$sim]:-}"

        if [[ "$status" != "running" ]] || [[ -z "$pid" ]] || [[ "$pid" == "0" ]]; then
            continue
        fi

        local health_result
        check_simulation_health "$sim" "$pid" "$LOGS_DIR/$sim.log" "$STATE_DIR"
        health_result=$?

        case $health_result in
            $HEALTH_OK)
                ;;
            $HEALTH_COMPLETED)
                log "[$sim] Simulation completed normally"
                SIM_STATUS[$sim]="completed"
                log_health_event "$METRICS_DIR/health_log.csv" "$sim" "COMPLETED" "Normal exit"
                ;;
            $HEALTH_CRASHED)
                local exit_code
                wait "${SIM_PIDS[$sim]}" 2>/dev/null
                exit_code=$?
                local exit_reason
                exit_reason=$(interpret_exit_code "$exit_code")
                log_health_event "$METRICS_DIR/health_log.csv" "$sim" "CRASHED" \
                    "$exit_reason (exit code $exit_code)"

                if [[ "$ENABLE_AUTO_RESTART" == "true" ]]; then
                    restart_simulation "$sim" || SIM_STATUS[$sim]="failed"
                else
                    SIM_STATUS[$sim]="failed"
                fi
                ;;
            $HEALTH_STALLED)
                log_health_event "$METRICS_DIR/health_log.csv" "$sim" "STALLED" \
                    "No progress for ${STALL_THRESHOLD_SEC}s"

                if [[ "$ENABLE_AUTO_RESTART" == "true" ]]; then
                    restart_simulation "$sim" || SIM_STATUS[$sim]="failed"
                else
                    SIM_STATUS[$sim]="stalled"
                fi
                ;;
        esac
    done
}

any_simulation_active() {
    for sim in "${ENABLED_MODES[@]}"; do
        local status="${SIM_STATUS[$sim]:-}"
        if [[ "$status" == "running" ]]; then
            return 0
        fi
    done
    return 1
}

#===============================================================================
# SECTION 9: CHECKPOINT AND STATUS
#===============================================================================

save_current_checkpoint() {
    local wall_elapsed=$(($(date +%s) - START_EPOCH))

    local sim_data=()
    for sim in "${ENABLED_MODES[@]}"; do
        local pid="${SIM_PIDS[$sim]:-0}"
        local status="${SIM_STATUS[$sim]:-unknown}"
        local iter
        iter=$(get_iteration_from_log "$LOGS_DIR/$sim.log" "next")
        local cells
        cells=$(get_cells_from_log "$LOGS_DIR/$sim.log" "next")
        local restarts
        restarts=$(get_restart_count "$STATE_DIR" "$sim")

        sim_data+=("$sim:$status:$pid:$iter:$cells:$restarts")
    done

    save_parallel_checkpoint "$OUTPUT_BASE/checkpoint.json" "$wall_elapsed" "${sim_data[@]}"
}

print_status() {
    clear
    echo "==============================================================================="
    echo "  SimuCell3D Parallel Benchmark - Live Status"
    echo "  Started: $START_TIME"
    echo "  Output: $OUTPUT_BASE"
    echo "  Enabled modes: ${ENABLED_MODES[*]}"
    echo "==============================================================================="
    echo ""
    printf "%-12s %10s %10s %10s %10s\n" \
        "SIMULATION" "ITERATION" "CELLS" "ITER/SEC" "STATUS"
    echo "-------------------------------------------------------------------------------"

    # Collect metrics from simulation output files (performance_diagnostics.csv)
    declare -A sim_metrics
    for sim in "${ENABLED_MODES[@]}"; do
        sim_metrics[$sim]=$(get_sim_metrics_from_diagnostics "$sim")
    done

    for sim in "${ENABLED_MODES[@]}"; do
        local iter cells ips
        IFS=',' read -r iter cells ips <<< "${sim_metrics[$sim]:-0,0,0}"
        local status="${SIM_STATUS[$sim]:-unknown}"

        if [[ "$iter" != "0" ]] || [[ "$cells" != "0" ]]; then
            printf "%-12s %10s %10s %10s %10s\n" \
                "$sim" "$iter" "$cells" "$ips" "$status"
        else
            printf "%-12s %10s %10s %10s %10s\n" \
                "$sim" "-" "-" "-" "$status"
        fi
    done

    echo ""
    echo "-------------------------------------------------------------------------------"

    # Calculate and display speedups if multiple modes running
    if [[ ${#ENABLED_MODES[@]} -gt 1 ]]; then
        local v1_ips=0 static_ips=0 adaptive_ips=0

        for sim in "${ENABLED_MODES[@]}"; do
            local ips
            ips=$(echo "${sim_metrics[$sim]:-0,0,0}" | cut -d',' -f3)
            case "$sim" in
                v1)       v1_ips="$ips" ;;
                static)   static_ips="$ips" ;;
                adaptive) adaptive_ips="$ips" ;;
            esac
        done

        if mode_is_enabled "v1" && mode_is_enabled "adaptive" && [[ "$v1_ips" != "0" ]]; then
            local speedup
            speedup=$(awk "BEGIN {printf \"%.2f\", $adaptive_ips / $v1_ips}")
            echo "SPEEDUP adaptive vs v1.0: ${speedup}x"
        fi
        if mode_is_enabled "static" && mode_is_enabled "adaptive" && [[ "$static_ips" != "0" ]]; then
            local speedup
            speedup=$(awk "BEGIN {printf \"%.2f\", $adaptive_ips / $static_ips}")
            echo "SPEEDUP adaptive vs static: ${speedup}x"
        fi
    fi

    echo ""
    echo "RECENT HEALTH EVENTS:"
    tail -3 "$METRICS_DIR/health_log.csv" 2>/dev/null | grep -v "^timestamp" | column -t -s',' || echo "  (none)"
    echo ""
    echo "Press Ctrl+C to stop monitoring (simulations continue in background)"
}

#===============================================================================
# SECTION 10: SUMMARY REPORT
#===============================================================================

generate_summary() {
    log "Generating summary report..."

    local end_time
    end_time=$(date)
    local total_wall=$(($(date +%s) - START_EPOCH))
    local total_wall_formatted
    total_wall_formatted=$(format_duration "$total_wall")

    cat > "$OUTPUT_BASE/SUMMARY.md" <<EOF
# Parallel Benchmark Summary

## Configuration
- **Parameter file:** $PARAM_FILE
- **Cores per simulation:** $CORES_PER_SIM
- **Monitor interval:** ${MONITOR_INTERVAL_SEC}s
- **Auto-restart:** $ENABLE_AUTO_RESTART
- **Clean rebuild:** $CLEAN_REBUILD
- **V1 root:** $V1_ROOT
- **Started:** $START_TIME
- **Completed:** $end_time
- **Total wall time:** $total_wall_formatted

## Enabled Modes
${ENABLED_MODES[*]}

## Core Allocation
| Simulation | Cores |
|------------|-------|
EOF

    for sim in "${ENABLED_MODES[@]}"; do
        local cores_var
        case "$sim" in
            v1) cores_var="$V1_CORES" ;;
            static) cores_var="$STATIC_CORES" ;;
            adaptive) cores_var="$ADAPTIVE_CORES" ;;
        esac
        echo "| $sim | $cores_var |" >> "$OUTPUT_BASE/SUMMARY.md"
    done

    cat >> "$OUTPUT_BASE/SUMMARY.md" <<EOF

## Final Results

### Computational Performance
| Simulation | Final Iteration | Final Cells | Avg Iter/Sec | Status |
|------------|-----------------|-------------|--------------|--------|
EOF

    # Collect final metrics from simulation output files (performance_diagnostics.csv)
    declare -A final_ips
    for sim in "${ENABLED_MODES[@]}"; do
        local diag_file="$OUTPUT_BASE/sim_$sim/performance_diagnostics.csv"
        if [[ -f "$diag_file" ]] && [[ $(wc -l < "$diag_file") -gt 1 ]]; then
            local last_line
            last_line=$(tail -1 "$diag_file")
            local iter cells total_ms
            iter=$(echo "$last_line" | awk -F',' '{print $3}')
            cells=$(echo "$last_line" | awk -F',' '{print $4}')
            total_ms=$(echo "$last_line" | awk -F',' '{print $12}')

            # Calculate average iterations per second from total_iteration_ms
            local avg_ips="0"
            if [[ -n "$total_ms" ]] && [[ "$total_ms" != "0" ]]; then
                avg_ips=$(awk -F',' 'NR>1 && $12>0 {sum+=(1000/$12); n++} END {if(n>0) printf "%.2f", sum/n; else print "0"}' "$diag_file")
            fi
            final_ips[$sim]="$avg_ips"

            echo "| $sim | $iter | $cells | $avg_ips | ${SIM_STATUS[$sim]:-unknown} |" >> "$OUTPUT_BASE/SUMMARY.md"
        else
            final_ips[$sim]="0"
            echo "| $sim | - | - | - | ${SIM_STATUS[$sim]:-no_data} |" >> "$OUTPUT_BASE/SUMMARY.md"
        fi
    done

    cat >> "$OUTPUT_BASE/SUMMARY.md" <<EOF

### Speedup Comparison
| Comparison | Final Speedup |
|------------|---------------|
EOF

    # Calculate speedups from final iteration rates
    local v1_ips="${final_ips[v1]:-0}"
    local static_ips="${final_ips[static]:-0}"
    local adaptive_ips="${final_ips[adaptive]:-0}"

    if mode_is_enabled "v1" && mode_is_enabled "static" && [[ "$v1_ips" != "0" ]]; then
        local speedup
        speedup=$(awk "BEGIN {printf \"%.2f\", $static_ips / $v1_ips}")
        echo "| static vs v1.0 | ${speedup}x |" >> "$OUTPUT_BASE/SUMMARY.md"
    fi
    if mode_is_enabled "v1" && mode_is_enabled "adaptive" && [[ "$v1_ips" != "0" ]]; then
        local speedup
        speedup=$(awk "BEGIN {printf \"%.2f\", $adaptive_ips / $v1_ips}")
        echo "| adaptive vs v1.0 | ${speedup}x |" >> "$OUTPUT_BASE/SUMMARY.md"
    fi
    if mode_is_enabled "static" && mode_is_enabled "adaptive" && [[ "$static_ips" != "0" ]]; then
        local speedup
        speedup=$(awk "BEGIN {printf \"%.2f\", $adaptive_ips / $static_ips}")
        echo "| adaptive vs static | ${speedup}x |" >> "$OUTPUT_BASE/SUMMARY.md"
    fi

    cat >> "$OUTPUT_BASE/SUMMARY.md" <<EOF

## Health Events
EOF

    if [[ -f "$METRICS_DIR/health_log.csv" ]] && [[ $(wc -l < "$METRICS_DIR/health_log.csv") -gt 1 ]]; then
        echo '```' >> "$OUTPUT_BASE/SUMMARY.md"
        tail -n +2 "$METRICS_DIR/health_log.csv" | column -t -s',' >> "$OUTPUT_BASE/SUMMARY.md"
        echo '```' >> "$OUTPUT_BASE/SUMMARY.md"
    else
        echo "(No health events recorded)" >> "$OUTPUT_BASE/SUMMARY.md"
    fi

    cat >> "$OUTPUT_BASE/SUMMARY.md" <<EOF

## Files Generated

### Simulation Output (Primary Data)
- \`sim_*/performance_diagnostics.csv\` - Per-iteration timing and metrics
- \`sim_*/simulation_statistics.csv\` - Per-cell biological data

### Benchmark Metadata
- \`metrics/health_log.csv\` - Process health events
- \`checkpoint.json\` - Resume checkpoint
- \`metadata.json\` - Benchmark configuration

## Resume Command
\`\`\`bash
./scripts/run_parallel_benchmark_comparison.sh --resume=<this_directory>
\`\`\`

## Analysis Commands
\`\`\`bash
# View performance diagnostics
column -t -s',' sim_adaptive/performance_diagnostics.csv | less

# Generate visualizations
./scripts/generate_visualizations.sh <this_directory>

# Plot results
python scripts/plot_benchmark_results_updated.py <this_directory>
\`\`\`
EOF

    log "Summary saved to: $OUTPUT_BASE/SUMMARY.md"
}

#===============================================================================
# SECTION 11: SIGNAL HANDLERS AND CLEANUP
#===============================================================================

cleanup() {
    log ""
    log "Caught interrupt signal"
    save_current_checkpoint
    generate_summary
    log ""
    log "Simulations may still be running in background."
    log "PIDs saved in: $STATE_DIR/*.pid"
    log "Resume with: $0 --resume=$OUTPUT_BASE"
    exit 0
}

#===============================================================================
# SECTION 12: MAIN MONITORING LOOP
#===============================================================================

monitor_loop() {
    log "Starting simulation monitoring (every ${MONITOR_INTERVAL_SEC}s)..."
    log "Metrics are collected by simulations via --diagnostics-csv flag"

    while any_simulation_active; do
        check_all_health
        save_current_checkpoint
        print_status

        if ! check_and_cleanup_if_critical "monitoring"; then
            error "Critical disk space issue - stopping benchmark"
            break
        fi

        sleep "$MONITOR_INTERVAL_SEC"
    done

    log "All simulations finished"
}

#===============================================================================
# SECTION 13: MAIN ENTRY POINT
#===============================================================================

main() {
    trap cleanup SIGINT SIGTERM SIGHUP EXIT

    parse_arguments "$@"

    echo "==============================================================================="
    echo "  SimuCell3D Parallel Benchmark"
    echo "==============================================================================="
    echo ""

    # Initialize
    if [[ -n "$RESUME_PATH" ]]; then
        init_from_resume
    else
        check_prerequisites || exit 1
        init_directories
    fi

    START_TIME=$(date)
    START_EPOCH=$(date +%s)
    echo "$START_TIME" > "$OUTPUT_BASE/start_time"

    # Launch simulations for enabled modes only
    for mode in "${ENABLED_MODES[@]}"; do
        case "$mode" in
            v1)       launch_v1_simulation ;;
            static)   launch_static_simulation ;;
            adaptive) launch_adaptive_simulation ;;
        esac
    done

    if [[ "$DRY_RUN" == "true" ]]; then
        log ""
        log "[DRY-RUN] Complete. No simulations started."
        log "Would create output at: $OUTPUT_BASE"
        exit 0
    fi

    # Wait for processes to start
    sleep 5

    # Enter monitoring loop
    monitor_loop

    # Generate final summary
    generate_summary

    log ""
    log "Benchmark complete!"
    log "Results: $OUTPUT_BASE"
    log "Summary: $OUTPUT_BASE/SUMMARY.md"
}

main "$@"
