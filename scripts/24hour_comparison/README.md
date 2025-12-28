# 24-Hour Performance Comparison Scripts

This directory contains scripts for running a comprehensive 24-hour performance comparison between SimuCell3D v1.0 and the current version (version-cpp-next).

**Key Feature:** Unlike the 1-hour and 12-hour comparisons, the 24-hour window provides sufficient time for **both versions to complete the full 128,000 iteration benchmark**, enabling a direct time-to-completion comparison.

## Quick Start

```bash
# 1. Upscale your VM to 16+ CPU cores

# 2. Verify prerequisites
./preflight_check.sh

# 3. Launch the 24-hour comparison
./launch_24hour_comparison.sh

# 4. After both simulations complete, analyze results
./analyze_results.sh <output_directory>
```

## Prerequisites

### System Requirements

- **CPU Cores**: Minimum 16 cores (8 for v1.0 + 8 for current version)
  - Recommended: 16 physical cores (avoid hyperthreading for fair comparison)
- **Memory**: 4-8 GB RAM
- **Disk Space**: At least 15 GB free (more than 12-hour due to potential longer runtime)
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
- Sufficient disk space (≥15GB)
- No conflicting simulations running

**Usage:**
```bash
./preflight_check.sh
```

### 2. `launch_24hour_comparison.sh`

Main launcher script that:
- Performs all pre-flight checks
- Creates output directory structure
- Prepares parameter files with absolute paths
- Launches both simulations with proper CPU affinity
- Starts monitoring script
- Generates status file

**Usage:**
```bash
./launch_24hour_comparison.sh
```

**What it does:**
1. Validates system configuration (16+ cores, binaries, etc.)
2. Creates timestamped output directory: `doc/working/24hour_comparison_YYYYMMDD_HHMMSS/`
3. Launches v1.0 on cores 0-7 with static scheduling
4. Launches current on cores 8-15 with adaptive scheduling
5. Both run for up to 24 hours (86,400 seconds) OR until they complete 128k iterations
6. Starts background monitoring

**Output Structure:**
```
doc/working/24hour_comparison_YYYYMMDD_HHMMSS/
├── config.txt                    # Configuration details
├── STATUS.md                     # Real-time status (while running)
├── SUMMARY.md                    # Final results (after completion)
├── MONITOR_SUMMARY.txt           # Monitor script summary
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

### 3. `monitor_24hour.sh`

Background monitoring script that:
- Checks simulation status every 60 seconds
- Displays detailed progress every 10 minutes
- Calculates current speedup and iteration rates
- Shows progress toward 128k iteration goal
- Estimates time to completion
- Generates `MONITOR_SUMMARY.txt` on completion

**Usage:**
```bash
# Automatically launched by launch_24hour_comparison.sh
# Or run manually:
./monitor_24hour.sh <output_directory>
```

**Monitor Output:**
```
===============================================================================
  24-Hour Comparison: v1.0 vs Current (Adaptive)
  Time: Sat Jan 18 18:00:00 UTC 2026
===============================================================================

Metric              v1.0        Current      Ratio
-------------------------------------------------------------------------------
Status              YES         YES          -
Iterations          95230       127845       1.34x
Progress            74.4%       99.9%        -
Rate (it/s)         1.47        2.22         -
ETA to 128k         6h 12m      0h 05m       -
-------------------------------------------------------------------------------

Time elapsed:   18h 00m 00s / 24h 00m 00s
Time remaining: 6h 00m 00s

Progress: 75.0% of 24-hour window

✓ Current has completed all 128,000 iterations!
```

### 4. `analyze_results.sh`

Post-run analysis script that:
- Extracts final iteration counts
- Determines if each version completed 128k iterations
- Calculates speedup metrics and time savings
- Extracts timing and memory usage from `/usr/bin/time`
- Generates comprehensive `SUMMARY.md` report
- Provides interpretation of results

**Usage:**
```bash
./analyze_results.sh /path/to/output_directory

# Example:
./analyze_results.sh ../doc/working/24hour_comparison_20260118_102030
```

## Expected Behavior

### Timeline Estimates

Based on ~1.54x speedup from benchmarks:

| Event | v1.0 Time | Current Time |
|-------|-----------|--------------|
| **Start** | 0:00 | 0:00 |
| **50% complete** | ~9-10h | ~6-7h |
| **100% complete** | ~18-20h | ~12-14h |
| **Hard timeout** | 24h | 24h |

**Key Point:** Both simulations should finish **before** the 24-hour timeout by completing all 128,000 iterations!

### Completion Scenarios

#### Scenario 1: Both Complete (Expected)
- **v1.0**: Completes 128k iterations in ~18-20 hours
- **Current**: Completes 128k iterations in ~12-14 hours
- **Result**: Direct time-to-completion comparison shows ~1.5x speedup

#### Scenario 2: Only Current Completes (Possible)
- **v1.0**: Reaches ~110-120k iterations in 24 hours
- **Current**: Completes 128k iterations in ~12-14 hours
- **Result**: Shows that current version completes while v1.0 would need extra time

#### Scenario 3: Neither Completes (Unlikely)
- Both reach high iteration counts but not 128k
- Still provides iteration-count comparison at 24-hour mark

## Monitoring During Execution

### Real-Time Progress

```bash
# Watch v1.0 log
tail -f doc/working/24hour_comparison_*/logs/v1.log

# Watch current version log
tail -f doc/working/24hour_comparison_*/logs/current.log

# Check monitor output (includes ETA and completion status)
tail -f doc/working/24hour_comparison_*/monitor_output.log
```

### Quick Status Check

```bash
OUTPUT_DIR="doc/working/24hour_comparison_YYYYMMDD_HHMMSS"  # Use actual directory

# Get current iterations and progress
V1=$(grep -oP 'iteration: \K[0-9]+' $OUTPUT_DIR/logs/v1.log | tail -1)
CURR=$(grep -oP 'Iteration \K[0-9]+' $OUTPUT_DIR/logs/current.log | tail -1)
echo "v1.0: $V1 / 128000 ($(awk "BEGIN {printf \"%.1f\", ($V1/128000)*100}")%)"
echo "Current: $CURR / 128000 ($(awk "BEGIN {printf \"%.1f\", ($CURR/128000)*100}")%)"

# Check if completed
[ "$V1" -ge 128000 ] && echo "✓ v1.0 COMPLETED!" || echo "v1.0 still running..."
[ "$CURR" -ge 128000 ] && echo "✓ Current COMPLETED!" || echo "Current still running..."
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

To modify core allocation, edit `launch_24hour_comparison.sh`:
```bash
V1_CORES="0-7"
CURRENT_CORES="8-15"
V1_NUM_THREADS=8
CURRENT_NUM_THREADS=8
```

### Duration

Default: 24 hours (86,400 seconds)

**Note:** This is a maximum timeout. Simulations will stop early when they reach 128,000 iterations.

To modify, edit `launch_24hour_comparison.sh`:
```bash
DURATION_SECONDS=86400  # Change this value
```

### Parameter File

Default: `parameters/progressive_scaling/parameters_vesicle_progbench_128k.xml`

This file configures 128,000 iterations maximum. With the 24-hour window:
- v1.0 expected to complete ~128k iterations in ~18-20 hours
- Current expected to complete ~128k iterations in ~12-14 hours
- Both should finish before the 24-hour timeout

## Troubleshooting

### Issue: Script reports insufficient CPU cores

**Solution**: Upscale your VM to at least 16 CPU cores before running.

```bash
# Check current core count
nproc

# Should show ≥16
```

### Issue: Simulation exits with "Iteration 128000, simulation complete"

**This is expected!** The simulation completed successfully.

**Action**: Wait for the other simulation to finish (or timeout), then run analysis.

### Issue: One simulation finished, other still running

**This is the expected outcome!** The current version should finish first.

**Action**: You can:
1. Wait for both to finish (recommended for complete comparison)
2. Manually stop the slower one and analyze partial results

```bash
# To stop simulations manually (if needed)
kill $(cat $OUTPUT_DIR/v1.pid)
kill $(cat $OUTPUT_DIR/current.pid)
```

### Issue: Both simulations finished in < 24 hours

**This is great!** You now have a time-to-completion comparison.

**Action**: Run the analysis script:
```bash
./analyze_results.sh $OUTPUT_DIR
```

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
| **Iterations Completed** | 128,000 | 128,000 | Both complete |
| **Time to Complete** | ~18-20 hours | ~12-14 hours | 1.4-1.6x |
| **Time Saved** | - | ~6-8 hours | - |
| **CPU Utilization** | ~790% | ~790% | Similar |
| **Memory Usage** | ~350 MB | ~350 MB | Similar |

**Key Result:** Current version finishes the full benchmark **6-8 hours faster** than v1.0!

## Post-Run Analysis

After completion (when both finish or hit 24-hour timeout):

```bash
# 1. Check if both completed
OUTPUT_DIR=$(ls -td ../doc/working/24hour_comparison_* | head -1)
tail -100 $OUTPUT_DIR/logs/v1.log      # Look for "simulation complete"
tail -100 $OUTPUT_DIR/logs/current.log # Look for "simulation complete"

# 2. Generate comprehensive summary
./analyze_results.sh $OUTPUT_DIR

# 3. View the summary
cat $OUTPUT_DIR/SUMMARY.md

# 4. Check performance diagnostics (current version only)
head -50 $OUTPUT_DIR/sim_current/performance_diagnostics.csv

# 5. Review monitor summary
cat $OUTPUT_DIR/MONITOR_SUMMARY.txt
```

## Integration with Other Comparisons

### Comparison Matrix

| Duration | CPU Each | Purpose | Expected Outcome |
|----------|----------|---------|------------------|
| **1 Hour** | 4 cores | Quick validation | ~30-50k iterations |
| **12 Hour** | 8 cores | Sustained performance | ~80-120k iterations |
| **24 Hour** | 8 cores | **Complete benchmark** | **Full 128k completion** |

### Recommended Workflow

1. **1-hour**: Quick proof-of-concept (~1 hour)
2. **12-hour**: Sustained performance validation (~12 hours)
3. **24-hour**: Final production benchmark (~12-20 hours actual)

All three use the same methodology, providing consistent results across time scales.

## Files in This Directory

```
scripts/24hour_comparison/
├── README.md                       # This file
├── QUICKSTART.md                   # One-page quick reference
├── launch_24hour_comparison.sh     # Main launcher script
├── monitor_24hour.sh               # Monitoring script
├── analyze_results.sh              # Post-run analysis
└── preflight_check.sh              # Pre-launch verification
```

## Support

For issues or questions:
1. Check log files in output directory
2. Verify prerequisites with `preflight_check.sh`
3. Review this README
4. Check main project documentation in `doc/`
