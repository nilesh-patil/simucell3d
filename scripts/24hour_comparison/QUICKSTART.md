# Quick Start: 24-Hour Performance Comparison

## One-Command Launch (After VM Upscaling)

```bash
cd /home/nilesh-patil/projects/version-cpp-next/scripts/24hour_comparison
./launch_24hour_comparison.sh
```

That's it! The script handles everything automatically.

---

## Step-by-Step Guide

### Before You Start

**Important**: Upscale your VM to **16+ CPU cores** first!

```bash
# Check current cores (should show ≥16)
nproc
```

### Step 1: Pre-Flight Check (Optional but Recommended)

```bash
cd /home/nilesh-patil/projects/version-cpp-next/scripts/24hour_comparison
./preflight_check.sh
```

This validates:
- ✓ 16+ CPU cores
- ✓ Both binaries exist and are Release builds
- ✓ Parameter files accessible
- ✓ Sufficient disk space (15GB+)
- ✓ Required commands available

### Step 2: Launch 24-Hour Comparison

```bash
./launch_24hour_comparison.sh
```

This automatically:
1. Creates timestamped output directory
2. Prepares parameter files
3. Launches v1.0 (cores 0-7, static scheduling)
4. Launches current (cores 8-15, adaptive scheduling)
5. Starts background monitoring
6. Runs for up to 24 hours (or until 128k iterations)

### Step 3: Monitor Progress (Optional)

```bash
# Quick status - get current iteration counts
OUTPUT_DIR=$(ls -td ../doc/working/24hour_comparison_* | head -1)

# v1.0 iterations
grep -oP 'iteration: \K[0-9]+' $OUTPUT_DIR/logs/v1.log | tail -1

# Current iterations
grep -oP 'Iteration \K[0-9]+' $OUTPUT_DIR/logs/current.log | tail -1

# Watch logs in real-time
tail -f $OUTPUT_DIR/logs/v1.log        # v1.0
tail -f $OUTPUT_DIR/logs/current.log   # Current

# Check monitor output
tail -f $OUTPUT_DIR/monitor_output.log
```

### Step 4: After Completion - Analyze Results

```bash
# Find the output directory
OUTPUT_DIR=$(ls -td ../doc/working/24hour_comparison_* | head -1)

# Generate comprehensive summary
./analyze_results.sh $OUTPUT_DIR

# View results
cat $OUTPUT_DIR/SUMMARY.md
```

---

## Expected Timeline

| Time | What's Happening |
|------|------------------|
| 0:00 | Launch script starts both simulations |
| 0:01 | Simulations running, monitor active |
| 12:00 | Current version likely completes 128k iterations |
| 18:00 | v1.0 likely completes 128k iterations |
| 24:00 | Hard timeout (if not already finished) |

**Note:** Both simulations will likely finish **before** 24 hours when they reach 128,000 iterations!

---

## Expected Results

Based on ~1.54x speedup from benchmarks:

| Version | Expected Outcome |
|---------|------------------|
| v1.0 (static) | Complete 128k iterations in ~18-20 hours |
| Current (adaptive) | Complete 128k iterations in ~12-14 hours |
| **Time Savings** | **~6-8 hours faster** |

**Both should complete the full benchmark**, providing a time-to-completion comparison!

---

## Output Location

All results in: `doc/working/24hour_comparison_YYYYMMDD_HHMMSS/`

Key files:
- `SUMMARY.md` - Comprehensive comparison report (after analysis)
- `logs/v1.log` - v1.0 simulation log
- `logs/current.log` - Current version log
- `sim_current/performance_diagnostics.csv` - Detailed metrics
- `MONITOR_SUMMARY.txt` - Monitor script summary

---

## Troubleshooting

### "Insufficient CPU cores" error

**Solution**: Upscale VM to 16+ cores, then relaunch

```bash
# Verify cores
nproc  # Should show ≥16
```

### Check if simulations are running

```bash
# Should show 2 simucell3d processes
ps aux | grep simucell3d | grep -v grep
```

### Simulation completed early

This is **expected**! Both simulations will likely finish before 24 hours.

```bash
# Check final iterations (should be ~128,000)
OUTPUT_DIR=$(ls -td ../doc/working/24hour_comparison_* | head -1)
grep -oP 'iteration: \K[0-9]+' $OUTPUT_DIR/logs/v1.log | tail -1
grep -oP 'Iteration \K[0-9]+' $OUTPUT_DIR/logs/current.log | tail -1
```

---

## Why 24 Hours vs 12 Hours vs 1 Hour?

### Comparison Matrix

| Duration | CPU Each | Expected Outcome | Use Case |
|----------|----------|------------------|----------|
| **1 Hour** | 4 cores | ~30-50k iterations | Quick validation |
| **12 Hour** | 8 cores | ~80-120k iterations | Sustained performance |
| **24 Hour** | 8 cores | **Full 128k benchmark** | **Complete comparison** |

### Key Advantage of 24-Hour Run

✅ **Both versions complete the full 128,000 iteration benchmark**
✅ **Time-to-completion comparison** (not just iteration count at timeout)
✅ **Validates sustained performance** over extended periods
✅ **Production-representative** workload

**Recommended:** Use 24-hour for final validation and publication-ready results.

---

## What Makes This Comparison Fair?

✅ **Equal CPU Resources**: 8 cores each
✅ **Same Parameter File**: Identical physics configuration
✅ **Parallel Execution**: Same system load, no time-of-day effects
✅ **Completion-Based**: Measures time to finish 128k iterations
✅ **Only Difference**: Adaptive scheduling + bug fixes in current version

---

## Need Help?

1. Run `./preflight_check.sh` to diagnose issues
2. Check `README.md` for detailed documentation
3. Review log files in output directory

---

**Ready to go? Just run:**

```bash
cd /home/nilesh-patil/projects/version-cpp-next/scripts/24hour_comparison
./launch_24hour_comparison.sh
```

**Tip:** The 24-hour window is conservative. Both simulations will likely finish in 12-20 hours!
