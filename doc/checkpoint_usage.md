# Checkpoint/Restart System - User Guide

## Overview

The checkpoint/restart system allows you to:
- **Save** simulation state to disk periodically
- **Resume** simulations after crashes or interruptions
- **Branch** simulations from common checkpoints
- **Share** reproducible simulation states

---

## Quick Start

### Basic Usage

```bash
# Run with automatic checkpointing
./simucell3d params.xml --checkpoint-interval 100

# Checkpoints saved to: ./checkpoints/checkpoint_00000100.simchk, checkpoint_00000200.simchk, etc.
```

### Resume from Checkpoint

```bash
# Restart from iteration 500
./simucell3d params.xml --restart checkpoints/checkpoint_00000500.simchk
```

### Custom Checkpoint Directory

```bash
# Save to specific directory
./simucell3d params.xml --checkpoint-interval 100 --checkpoint-dir /path/to/checkpoints/
```

---

## Command-Line Options

### --checkpoint-interval \<N\>

Save checkpoint every N iterations.

**Example:**
```bash
./simucell3d params.xml --checkpoint-interval 50
# Saves at iterations: 0, 50, 100, 150, ...
```

**Notes:**
- Setting to 0 disables checkpointing (default)
- Recommend 50-500 depending on simulation length
- Higher intervals = less I/O overhead, less granular restarts

### --checkpoint-dir \<path\>

Directory where checkpoint files are saved.

**Example:**
```bash
./simucell3d params.xml --checkpoint-interval 100 --checkpoint-dir /tmp/my_checkpoints/
```

**Notes:**
- Directory created automatically if it doesn't exist
- Default: `./checkpoints/`
- Use absolute paths for clarity

### --restart \<file\>

Resume simulation from checkpoint file.

**Example:**
```bash
./simucell3d params.xml --restart checkpoints/checkpoint_00000500.simchk
```

**Notes:**
- Simulation resumes from the checkpointed iteration
- Parameter file still required (for solver configuration)
- Can combine with --checkpoint-interval to continue saving

---

## Common Workflows

### 1. Long Simulation with Fault Tolerance

**Scenario:** Running a 10,000-iteration simulation on a cluster with 24-hour job limits.

```bash
# Initial run
./simucell3d params.xml --checkpoint-interval 500 --checkpoint-dir /scratch/checkpoints/

# If job times out at iteration 4500, restart:
./simucell3d params.xml --restart /scratch/checkpoints/checkpoint_00004500.simchk --checkpoint-interval 500 --checkpoint-dir /scratch/checkpoints/

# Continues from 4500 → 10000
```

### 2. Parameter Exploration

**Scenario:** Test different adhesion strengths starting from common initial state.

```bash
# Step 1: Grow tissue to steady state
./simucell3d growth.xml --checkpoint-interval 1000
# Creates checkpoint_00001000.simchk

# Step 2: Branch into different parameter regimes
# (Create separate parameter files with different adhesion values)

./simucell3d high_adhesion.xml --restart checkpoints/checkpoint_00001000.simchk
./simucell3d low_adhesion.xml --restart checkpoints/checkpoint_00001000.simchk
./simucell3d no_adhesion.xml --restart checkpoints/checkpoint_00001000.simchk

# All start from same state, explore different parameters
```

### 3. Debugging Simulation Issues

**Scenario:** Simulation crashes at unknown time point.

```bash
# Run with frequent checkpointing
./simucell3d params.xml --checkpoint-interval 10

# Crashes at iteration 237...

# Restart from just before crash
./simucell3d params.xml --restart checkpoints/checkpoint_00000230.simchk

# Examine what happens at iteration 237
```

---

## Checkpoint File Format

### File Naming

```
checkpoint_NNNNNNNN.simchk
```

- **N** = 8-digit zero-padded iteration number
- **Extension:** `.simchk` (SimuCell3D Checkpoint)

**Examples:**
- `checkpoint_00000000.simchk` - Initial state (iteration 0)
- `checkpoint_00000100.simchk` - Iteration 100
- `checkpoint_00012345.simchk` - Iteration 12,345

### File Size

**Typical sizes:**
- Single cell: ~250-450 KB
- 10 cells: ~2-3 MB
- 100 cells: ~20-30 MB

**Factors affecting size:**
- Number of cells
- Mesh resolution (vertices per cell)
- Number of unused nodes/faces

---

## Best Practices

### Checkpoint Interval Selection

**Guidelines:**

| Simulation Length | Recommended Interval | Rationale |
|-------------------|----------------------|-----------|
| < 1,000 iterations | 100-200 | Minimal overhead |
| 1,000 - 10,000 iterations | 200-500 | Balance I/O and granularity |
| > 10,000 iterations | 500-1000 | Reduce I/O overhead |

**Consider:**
- Longer intervals = less I/O overhead
- Shorter intervals = finer restart granularity
- Critical simulations: use shorter intervals

### Disk Space Management

**Estimate required space:**
```
Space = (total_iterations / interval) × checkpoint_size
```

**Example:**
- 10,000 iterations, interval 100, 5 MB per checkpoint
- Space needed: (10,000 / 100) × 5 MB = 500 MB

**Tips:**
- Monitor disk usage for long simulations
- Consider checkpoint rotation (delete old checkpoints)
- Compress checkpoint directories if space-limited

### Reproducibility

**To ensure reproducible results:**

1. **Save parameter file with checkpoints**
   - Checkpoint stores parameter file reference
   - Keep parameter files organized

2. **Document checkpoint provenance**
   - Note which checkpoint was used for which analysis
   - Include checkpoint files in supplementary data

3. **Version compatibility**
   - Checkpoints include format version
   - Future versions will maintain backward compatibility

---

## Troubleshooting

### Checkpoint Not Saved

**Problem:** No checkpoint files created

**Check:**
1. Checkpoint interval set? `--checkpoint-interval > 0`
2. Directory writable? Check permissions
3. Disk space available?
4. Simulation ran long enough? (Need to reach checkpoint iteration)

**Solution:**
```bash
# Verify arguments
./simucell3d params.xml --checkpoint-interval 10 --checkpoint-dir ./test_chk/

# Check directory created
ls -la ./test_chk/
```

### Checkpoint Load Fails

**Problem:** Error loading checkpoint

**Check:**
1. File exists? `ls checkpoints/checkpoint_*.simchk`
2. File corrupt? Check file size (should be >100 KB)
3. Version compatible? Check error message

**Solution:**
- Use `--restart` with absolute path
- Verify parameter file is correct
- Check checkpoint file version

### Simulation Behaves Differently After Restart

**Expected:** Small numerical differences due to floating-point rounding

**Acceptable:** Differences < 1e-8 in positions/forces

**Not Expected:** Large differences or crashes

**If issues persist:**
- Report as bug with checkpoint file and parameters
- Use earlier checkpoint
- Re-run from beginning as fallback

---

## Examples

### Example 1: Daily Checkpointing

```bash
#!/bin/bash
# Save checkpoint every 1000 iterations (~1 hour of compute)

./simucell3d tissue_growth.xml \
    --checkpoint-interval 1000 \
    --checkpoint-dir /data/simulations/tissue_growth/checkpoints/
```

### Example 2: Restart After Crash

```bash
# Find latest checkpoint
LATEST=$(ls -t checkpoints/*.simchk | head -1)

# Restart from latest
./simucell3d params.xml --restart $LATEST --checkpoint-interval 1000
```

### Example 3: Parameter Sweep Script

```python
import subprocess
import os

# Grow to steady state first
subprocess.run([
    "./simucell3d", "growth.xml",
    "--checkpoint-interval", "5000"
])

# Explore different parameters
adhesion_values = [50, 100, 200, 500, 1000]

for adhesion in adhesion_values:
    # Create parameter file with this adhesion
    create_param_file(f"params_adh{adhesion}.xml", adhesion)

    # Run from common checkpoint
    subprocess.run([
        "./simucell3d", f"params_adh{adhesion}.xml",
        "--restart", "checkpoints/checkpoint_00005000.simchk"
    ])
```

---

## Technical Details

### File Format Specification

**Binary Format (.simchk):**

```
Header:
  uint32_t magic_number     (0x53494D43 = "SIMC")
  uint16_t version_major    (1)
  uint16_t version_minor    (0)
  time_t timestamp          (creation time)

Metadata:
  uint32_t iteration        (current iteration)
  double simulation_time    (current simulation time)
  string parameter_file     (length-prefixed)

Cells:
  uint32_t num_cells        (number of cells)
  for each cell:
    uint32_t cell_id
    uint32_t cell_type_id
    bool is_static
    double volume, area, target_volume, growth_rate, division_volume
    vec3 centroid

    uint32_t num_nodes
    for each node:
      bool is_used
      vec3 position
      vec3 force
      [vec3 momentum if DYNAMIC_MODEL]

    uint32_t num_faces
    for each face:
      uint32_t face_id
      uint32_t n1, n2, n3  (node indices)
      uint16_t type_id
```

**Portability:** Binary format is platform-dependent (endianness). Use same architecture for save/load.

---

## Performance Impact

**Checkpoint Write Time:**
- Single cell: <1 ms
- 10 cells: ~5-10 ms
- 100 cells: ~50-100 ms

**Overhead:**
- Negligible for intervals >50 iterations
- <0.1% total runtime impact for typical usage

**Storage I/O:**
- Write bandwidth: ~50-100 MB/s (SSD)
- Read bandwidth: ~100-200 MB/s (SSD)

---

## Future Enhancements

**Planned:**
- Checkpoint compression (reduce file sizes)
- HDF5 format option (better tooling)
- Checkpoint rotation (auto-delete old files)
- RNG state saving (bit-exact determinism)
- Parallel checkpoint writing (MPI simulations)

**Request features:** Submit issues to the repository

---

## FAQs

**Q: How often should I checkpoint?**
A: For long simulations, every 100-500 iterations is good. For critical simulations, checkpoint more frequently (every 10-50 iterations).

**Q: Can I use checkpoints from different versions?**
A: Currently, checkpoints include version numbers. Future versions will aim for backward compatibility.

**Q: Do checkpoints include RNG state?**
A: Not yet. Restarted simulations may have slightly different random numbers. Future enhancement planned.

**Q: Can I inspect checkpoint files?**
A: Currently no dedicated tool. Future: checkpoint inspector utility. For now, checkpoints are binary blobs.

**Q: What if my checkpoint file is corrupt?**
A: Use an earlier checkpoint. The system doesn't yet validate checksums (future enhancement).

---

**Documentation Version:** 1.0
**Last Updated:** 2025-10-31
**Feedback:** Submit issues to repository
