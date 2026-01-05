# Quick Start: 12-Hour Performance Comparison

## One-Command Launch (After VM Upscaling)

```bash
cd /home/nilesh-patil/projects/version-cpp-next/scripts/12hour_comparison
./launch_12hour_comparison.sh
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
cd /home/nilesh-patil/projects/version-cpp-next/scripts/12hour_comparison
./preflight_check.sh
```

This validates:
- ✓ 16+ CPU cores
- ✓ Both binaries exist and are Release builds
- ✓ Parameter files accessible
- ✓ Sufficient disk space (10GB+)
- ✓ Required commands available

### Step 2: Launch 12-Hour Comparison

```bash
./launch_12hour_comparison.sh
```

This automatically:
1. Creates timestamped output directory
2. Prepares parameter files
3. Launches v1.0 (cores 0-7, static scheduling)
4. Launches current (cores 8-15, adaptive scheduling)
5. Starts background monitoring
6. Runs for exactly 12 hours

### Step 3: Monitor Progress (Optional)

```bash
# Quick status - get current iteration counts
OUTPUT_DIR=$(ls -td ../doc/working/12hour_comparison_* | head -1)

# v1.0 iterations
grep -oP 'iteration: \K[0-9]+' $OUTPUT_DIR/logs/v1.log | tail -1

# Current iterations
grep -oP 'Iteration \K[0-9]+' $OUTPUT_DIR/logs/current.log | tail -1

# Watch logs in real-time
tail -f $OUTPUT_DIR/logs/v1.log        # v1.0
tail -f $OUTPUT_DIR/logs/current.log   # Current
```

### Step 4: After 12 Hours - Analyze Results

```bash
# Find the output directory
OUTPUT_DIR=$(ls -td ../doc/working/12hour_comparison_* | head -1)

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
| 6:00 | Halfway point (~60k iterations) |
| 12:00 | Automatic termination, both simulations stop |
| 12:01 | Run analyze_results.sh to generate summary |

---

## Expected Results

Based on ~1.54x speedup from benchmarks:

| Version | Estimated Iterations (12h) | Rate |
|---------|---------------------------|------|
| v1.0 (static) | ~82,000 | ~1.9 it/s |
| Current (adaptive) | ~126,000 | ~2.9 it/s |
| **Speedup** | **1.54x** | **1.54x** |

---

## Output Location

All results in: `doc/working/12hour_comparison_YYYYMMDD_HHMMSS/`

Key files:
- `SUMMARY.md` - Comprehensive comparison report (after analysis)
- `logs/v1.log` - v1.0 simulation log
- `logs/current.log` - Current version log
- `sim_current/performance_diagnostics.csv` - Detailed metrics

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

### Simulation crashed early

```bash
# Check logs for errors
OUTPUT_DIR=$(ls -td ../doc/working/12hour_comparison_* | head -1)
tail -50 $OUTPUT_DIR/logs/v1.log
tail -50 $OUTPUT_DIR/logs/current.log
```

---

## What Makes This Comparison Fair?

✅ **Equal CPU Resources**: 8 cores each
✅ **Same Parameter File**: Identical physics configuration
✅ **Parallel Execution**: Same system load, no time-of-day effects
✅ **Hard Time Limit**: Exactly 12 hours for both
✅ **Only Difference**: Adaptive scheduling + bug fixes in current version

---

## Need Help?

1. Run `./preflight_check.sh` to diagnose issues
2. Check `README.md` for detailed documentation
3. Review log files in output directory

---

**Ready to go? Just run:**

```bash
cd /home/nilesh-patil/projects/version-cpp-next/scripts/12hour_comparison
./launch_12hour_comparison.sh
```
