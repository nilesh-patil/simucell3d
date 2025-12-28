# 12-Hour Performance Comparison Scripts

This directory contains scripts for running a comprehensive 12-hour performance comparison between SimuCell3D v1.0 and the current version (version-cpp-next).

## Quick Start

```bash
# 1. Upscale your VM to 16+ CPU cores

# 2. Verify prerequisites
./preflight_check.sh

# 3. Launch the 12-hour comparison
./launch_12hour_comparison.sh

# 4. After 12 hours, analyze results
./analyze_results.sh <output_directory>
```

## Prerequisites

### System Requirements

- **CPU Cores**: Minimum 16 cores (8 for v1.0 + 8 for current version)
  - Recommended: 16 physical cores (avoid hyperthreading for fair comparison)
- **Memory**: 4-8 GB RAM
- **Disk Space**: At least 10 GB free
- **OS**: Linux (tested on Ubuntu/Debian)

### Software Requirements

- Both `simucell3d` binaries built in Release mode:
  - v1.0 at `/home/nilesh-patil/projects/version-cpp-v1.0/build/simucell3d`
  - Current at `/home/nilesh-patil/projects/version-cpp-next/build/simucell3d`
- Standard tools: `bash`, `taskset`, `timeout`, `grep`, `awk`

## Scripts Overview

### 1. `preflight_check.sh`

Pre-flight verification script that checks:
- CPU core count (≥16)
- Both binaries exist and are Release builds
- Parameter files and mesh files are accessible
- Sufficient disk space
- No conflicting simulations running

**Usage:**
```bash
./preflight_check.sh
```

### 2. `launch_12hour_comparison.sh`

Main launcher script that:
- Performs all pre-flight checks
- Creates output directory structure
- Prepares parameter files with absolute paths
- Launches both simulations with proper CPU affinity
- Starts monitoring script
- Generates status file

**Usage:**
```bash
./launch_12hour_comparison.sh
```

**What it does:**
1. Validates system configuration (16+ cores, binaries, etc.)
2. Creates timestamped output directory: `doc/working/12hour_comparison_YYYYMMDD_HHMMSS/`
3. Launches v1.0 on cores 0-7 with static scheduling
4. Launches current on cores 8-15 with adaptive scheduling
5. Both run for exactly 12 hours (43,200 seconds)
6. Starts background monitoring

**Output Structure:**
```
doc/working/12hour_comparison_YYYYMMDD_HHMMSS/
├── config.txt                    # Configuration details
├── STATUS.md                     # Real-time status (while running)
├── SUMMARY.md                    # Final results (after completion)
├── logs/
│   ├── v1.log                   # v1.0 simulation log
│   └── current.log              # Current version simulation log
├── sim_v1/                      # v1.0 output files
│   ├── simulation_statistics.csv
│   ├── cell_data/
│   └── face_data/
├── sim_current/                 # Current version output files
│   ├── simulation_statistics.csv
│   ├── performance_diagnostics.csv
│   ├── cell_data/
│   └── face_data/
└── monitor_output.log           # Monitoring script output
```

### 3. `monitor_12hour.sh`

Background monitoring script that:
- Checks simulation status every 60 seconds
- Displays detailed progress every 10 minutes
- Calculates current speedup and iteration rates
- Projects final iteration counts
- Generates `MONITOR_SUMMARY.txt` on completion

**Usage:**
```bash
# Automatically launched by launch_12hour_comparison.sh
# Or run manually:
./monitor_12hour.sh <output_directory>
```

**Monitor Output:**
```
===============================================================================
  12-Hour Comparison: v1.0 vs Current (Adaptive)
  Time: Sat Jan 18 10:30:00 UTC 2026
===============================================================================

Metric          v1.0       Current      Ratio
-------------------------------------------------------------------------------
Status          YES         YES          -
Iterations      45230       69845        1.54x
Rate (it/s)     1.05        1.62         -
Projected       90460       139690       -
-------------------------------------------------------------------------------

Time elapsed:   12h 00m 00s / 12h 00m 00s
Time remaining: 0h 00m 00s
```

### 4. `analyze_results.sh`

Post-run analysis script that:
- Extracts final iteration counts
- Calculates speedup metrics
- Extracts timing and memory usage from `/usr/bin/time`
- Generates comprehensive `SUMMARY.md` report
- Provides interpretation of results

**Usage:**
```bash
./analyze_results.sh /path/to/output_directory

# Example:
./analyze_results.sh ../doc/working/12hour_comparison_20260118_102030
```

## Monitoring During Execution

### Real-Time Progress

```bash
# Watch v1.0 log
tail -f doc/working/12hour_comparison_*/logs/v1.log

# Watch current version log
tail -f doc/working/12hour_comparison_*/logs/current.log

# Check monitor output
tail -f doc/working/12hour_comparison_*/monitor_output.log
```

### Quick Status Check

```bash
# Get current iteration counts
OUTPUT_DIR="doc/working/12hour_comparison_YYYYMMDD_HHMMSS"  # Use actual directory

# v1.0 iterations
grep -oP 'iteration: \K[0-9]+' $OUTPUT_DIR/logs/v1.log | tail -1

# Current iterations
grep -oP 'Iteration \K[0-9]+' $OUTPUT_DIR/logs/current.log | tail -1

# Calculate current speedup
V1=$(grep -oP 'iteration: \K[0-9]+' $OUTPUT_DIR/logs/v1.log | tail -1)
CURR=$(grep -oP 'Iteration \K[0-9]+' $OUTPUT_DIR/logs/current.log | tail -1)
echo "scale=2; $CURR / $V1" | bc
```

### Process Status

```bash
# Check if simulations are running
ps aux | grep simucell3d | grep -v grep

# Check CPU usage
top -b -n 1 | grep simucell3d

# Check CPU affinity
pgrep -f "simucell3d.*v1" | xargs taskset -cp
pgrep -f "simucell3d.*adaptive" | xargs taskset -cp
```

## Configuration

### CPU Core Allocation

Default configuration (16 cores):
- **v1.0**: Cores 0-7 (8 threads, static scheduling)
- **Current**: Cores 8-15 (8 threads, adaptive scheduling)

To modify core allocation, edit `launch_12hour_comparison.sh`:
```bash
V1_CORES="0-7"
CURRENT_CORES="8-15"
V1_NUM_THREADS=8
CURRENT_NUM_THREADS=8
```

### Duration

Default: 12 hours (43,200 seconds)

To modify, edit `launch_12hour_comparison.sh`:
```bash
DURATION_SECONDS=43200  # Change this value
```

### Parameter File

Default: `parameters/progressive_scaling/parameters_vesicle_progbench_128k.xml`

This file configures 128,000 iterations maximum. For 12 hours:
- v1.0 will likely complete ~80-90k iterations
- Current version will likely complete all 128k iterations (if speedup ≥1.54x)

## Troubleshooting

### Issue: Script reports insufficient CPU cores

**Solution**: Upscale your VM to at least 16 CPU cores before running.

```bash
# Check current core count
nproc

# Should show ≥16
```

### Issue: Simulation exits early with SIGSEGV

**Possible causes**:
1. Physics parameter issues (not a code bug)
2. Insufficient memory

**Solutions**:
- Check log files for error messages
- Verify Release build: `grep CMAKE_BUILD_TYPE build/CMakeCache.txt`
- Ensure sufficient RAM (4-8 GB)

### Issue: CPU affinity not applied correctly

**Check**:
```bash
# Get PID of simucell3d process
pgrep -f simucell3d

# Check affinity (should show restricted core list)
taskset -cp <PID>
```

**Note**: The wrapper processes (timeout, time) may show full core access, but the actual `simucell3d` process should be restricted.

### Issue: Low CPU utilization

**Possible causes**:
1. I/O bottleneck (disk writing)
2. Memory bandwidth limitation
3. Other processes consuming CPU

**Check**:
```bash
# Monitor CPU usage
htop

# Check I/O wait
iostat -x 5

# Verify OpenMP threads
ps -eLf | grep simucell3d | wc -l  # Should show 8 threads per simulation
```

## Expected Results

Based on previous benchmarks showing ~1.54x average speedup for adaptive mode:

| Metric | v1.0 (Static) | Current (Adaptive) | Speedup |
|--------|---------------|-------------------|---------|
| **Estimated Iterations (12h)** | ~82,000 | ~126,000 | 1.54x |
| **Iteration Rate** | ~1.9 it/s | ~2.9 it/s | 1.54x |
| **CPU Utilization** | ~790% | ~790% | Similar |
| **Memory Usage** | ~350 MB | ~350 MB | Similar |

**Key Differences:**
- Adaptive scheduling provides 10-15% base improvement
- Per-loop optimization adds another 3-5%
- Bug fixes (BUG-001, BUG-003) improve correctness
- 24 commits of improvements since v1.0

## Post-Run Analysis

After completion (12 hours or when both reach 128k iterations):

```bash
# 1. Check if both completed
tail -100 $OUTPUT_DIR/logs/v1.log
tail -100 $OUTPUT_DIR/logs/current.log

# 2. Generate comprehensive summary
./analyze_results.sh $OUTPUT_DIR

# 3. View the summary
cat $OUTPUT_DIR/SUMMARY.md

# 4. Check performance diagnostics (current version only)
head -50 $OUTPUT_DIR/sim_current/performance_diagnostics.csv
```

## Integration with 1-Hour Comparison

This 12-hour comparison complements the 1-hour comparison by:
- Using more CPU resources (8 cores each vs 4 cores each)
- Running longer to observe sustained performance
- Potentially completing the full 128k iterations
- Providing more statistical significance

Compare results:
- **1-hour**: Quick validation of speedup claims
- **12-hour**: Comprehensive performance characterization
- **Both**: Verify consistent speedup across time scales

## Files in This Directory

```
scripts/12hour_comparison/
├── README.md                       # This file
├── launch_12hour_comparison.sh     # Main launcher script
├── monitor_12hour.sh               # Monitoring script
├── analyze_results.sh              # Post-run analysis
└── preflight_check.sh              # Pre-launch verification
```

## Support

For issues or questions:
1. Check log files in output directory
2. Verify prerequisites with `preflight_check.sh`
3. Review this README
4. Check main project documentation in `doc/`
