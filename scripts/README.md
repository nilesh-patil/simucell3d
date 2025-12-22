# Performance Comparison Scripts

This directory contains automated scripts for comparing performance between SimuCell3D v1.0 and the current version (version-cpp-next).

## Quick Reference

| Comparison | Duration | CPU Cores | Status | Quick Start |
|------------|----------|-----------|--------|-------------|
| **1-Hour** | 1 hour | 8 (4+4) | ✅ Running | See `doc/working/1hour_comparison_*/` |
| **12-Hour** | 12 hours | 16 (8+8) | 📦 Ready | `cd 12hour_comparison && ./launch_12hour_comparison.sh` |
| **24-Hour** | 24 hours | 16 (8+8) | 📦 Ready | `cd 24hour_comparison && ./launch_24hour_comparison.sh` |

## Directory Structure

```
scripts/
├── README.md                    # This file
├── COMPARISON_GUIDE.md          # Detailed guide for choosing comparisons
├── 12hour_comparison/           # 12-hour comparison scripts
│   ├── README.md               # Complete documentation
│   ├── QUICKSTART.md           # One-page quick start
│   ├── launch_12hour_comparison.sh
│   ├── monitor_12hour.sh
│   ├── analyze_results.sh
│   └── preflight_check.sh
├── 24hour_comparison/           # 24-hour comparison scripts
│   ├── README.md               # Complete documentation
│   ├── QUICKSTART.md           # One-page quick start
│   ├── launch_24hour_comparison.sh
│   ├── monitor_24hour.sh
│   ├── analyze_results.sh
│   └── preflight_check.sh
└── benchmark_unified.sh         # Unified benchmark runner (separate tool)
```

## Getting Started

### 1. Read the Comparison Guide

```bash
cat COMPARISON_GUIDE.md
```

This helps you choose the right comparison for your needs.

### 2. For 12-Hour Comparison

```bash
cd 12hour_comparison
./preflight_check.sh       # Verify 16+ cores
./launch_12hour_comparison.sh
```

### 3. For 24-Hour Comparison (Recommended)

```bash
cd 24hour_comparison
./preflight_check.sh       # Verify 16+ cores
./launch_24hour_comparison.sh
```

## Choosing a Comparison

**Quick decision:**
- **Need fast results?** → 1-hour (8 cores)
- **Want sustained performance validation?** → 12-hour (16 cores)
- **Need publication-quality results?** → **24-hour (16 cores)** ⭐

See `COMPARISON_GUIDE.md` for detailed decision criteria.

## Prerequisites

### For 1-Hour Comparison
- 8 CPU cores total (4 for each simulation)
- 5 GB free disk space
- Currently running!

### For 12-Hour & 24-Hour Comparisons
- **16+ CPU cores** (8 for each simulation)
- 15-20 GB free disk space
- VM upscaling required

## Features

All comparison scripts include:

✅ **Automated Pre-flight Checks**
- CPU core validation
- Binary and build verification
- Disk space checks
- Dependency validation

✅ **One-Command Launch**
- Automatic setup and configuration
- Parallel simulation execution
- CPU affinity management
- Background monitoring

✅ **Real-Time Monitoring**
- Progress tracking every minute
- Detailed status every 10 minutes
- Speedup calculations
- ETA estimates (24-hour only)

✅ **Comprehensive Analysis**
- Automatic metric extraction
- Speedup calculations
- Time-to-completion (24-hour)
- Markdown summary reports

## Expected Results

| Comparison | v1.0 Result | Current Result | Speedup Metric |
|------------|-------------|----------------|----------------|
| 1-Hour | ~22k iterations | ~34k iterations | ~1.54x |
| 12-Hour | ~82k iterations | ~126k iterations | ~1.54x |
| 24-Hour | 128k in ~18-20h | 128k in ~12-14h | **~6-8 hours saved** |

**Key Difference:** The 24-hour comparison provides a **time-to-completion** measurement, while 1-hour and 12-hour provide **throughput** measurements.

## Documentation

### Main Guides
- `COMPARISON_GUIDE.md` - Choosing the right comparison
- `12hour_comparison/README.md` - Complete 12-hour documentation
- `24hour_comparison/README.md` - Complete 24-hour documentation

### Quick References
- `12hour_comparison/QUICKSTART.md` - One-page 12-hour guide
- `24hour_comparison/QUICKSTART.md` - One-page 24-hour guide

## Support

For help:
1. Check the appropriate README in the comparison directory
2. Run `./preflight_check.sh` to diagnose issues
3. Review log files in the output directory

## Current Status

✅ **1-Hour Comparison**: Running
- Started: ~30 minutes ago
- Location: `doc/working/1hour_comparison_20260117_213707/`
- Expected completion: ~30 minutes remaining

📦 **12-Hour Comparison**: Ready to launch (requires 16+ cores)
- Location: `scripts/12hour_comparison/`
- Pre-flight check available

📦 **24-Hour Comparison**: Ready to launch (requires 16+ cores)
- Location: `scripts/24hour_comparison/`
- Pre-flight check available

---

**Recommendation:** After upgrading your VM to 16+ cores, run the **24-hour comparison** for the most comprehensive and publication-ready results.
