# SimuCell3D FAQ

Frequently asked questions and troubleshooting guide.

## Table of Contents

- [Installation Issues](#installation-issues)
- [Runtime Errors](#runtime-errors)
- [Performance Issues](#performance-issues)
- [Parameter Configuration](#parameter-configuration)
- [Results & Visualization](#results--visualization)
- [Development](#development)

---

## Installation Issues

### CMake can't find OpenMP

**Error:**
```
Could NOT find OpenMP (missing: OpenMP_C_FOUND OpenMP_CXX_FOUND)
```

**Solution:**

**Ubuntu/Debian:**
```bash
sudo apt install libomp-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install libgomp-devel
```

**macOS:**
```bash
brew install libomp
# Then rebuild with special CMake flags (see INSTALL.md)
```

---

### Build fails with "out of memory"

**Problem:** Compilation crashes during parallel build.

**Solution:** Reduce parallel jobs:
```bash
make -j2  # Instead of make -j$(nproc)
```

Or add swap space temporarily:
```bash
sudo fallocate -l 4G /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
make -j$(nproc)
```

---

### Permission denied when running simucell3d

**Solution:**
```bash
chmod +x ./simucell3d
```

---

### Docker build fails

**Error:** "Cannot connect to Docker daemon"

**Solution:**
1. Ensure Docker is running: `systemctl start docker` (Linux) or start Docker Desktop (macOS/Windows)
2. Add user to docker group: `sudo usermod -aG docker $USER && newgrp docker`

---

## Runtime Errors

### SIGSEGV (Segmentation fault / Exit code 139)

**This is almost always a physics parameter issue, not a code bug.**

**Common causes and solutions:**

1. **Time step too large**
   ```xml
   <!-- Reduce time step -->
   <time_step>5e-8</time_step>  <!-- Try halving from 1e-7 -->
   ```

2. **Mesh file not found**
   - Check that `<input_mesh_file_path>` in XML points to an existing file
   - Use absolute paths to avoid issues

3. **Cells exploding (numerical instability)**
   - Reduce growth rate: `<avg_growth_rate>5e-12</avg_growth_rate>`
   - Increase damping: `<damping>3e-10</damping>`
   - Reduce surface tension: `<surface_tension>0.0005</surface_tension>`

4. **Initial mesh too coarse**
   - Ensure `<min_edge_len>` is appropriate for cell size
   - Re-generate mesh with finer triangulation

---

### "File not found" errors

**Error:** `Error: Cannot open file: ../parameters/core/parameters_vesicle.xml`

**Solution:**
- Run from the correct directory (usually `build/`)
- Use absolute paths: `./simucell3d /full/path/to/params.xml`

---

### Simulation hangs (no progress)

**Possible causes:**

1. **Extremely small time step**: Check `<time_step>` isn't too small (< 1e-9)
2. **Stuck in local minimum**: Try different initial conditions
3. **Infinite loop in mesh refinement**: Check `<min_edge_len>` is reasonable

---

## Performance Issues

### Very slow simulation

**Checklist:**

1. **Wrong build type** - Must be Release, not Debug
   ```bash
   cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)
   ```

2. **Wrong scheduling mode** for your cell count:
   | Cell Count | Best Mode |
   |------------|-----------|
   | 64-256 | `dynamic` or `static` |
   | 256-2048 | `guided` or `auto` |
   | 2048+ | `static` or `auto` |

3. **Not using all cores**:
   ```bash
   export OMP_NUM_THREADS=$(nproc)
   ./simucell3d params.xml --schedule=adaptive
   ```

---

### Low CPU utilization (threads not being used)

**Expected CPU utilization:**
- 4 threads: 240-280% (60-70% efficiency)
- 8 threads: 300-500% (37-62% efficiency)
- 16 threads: ~1000-1200% (62-75% efficiency)

**If significantly below expected:**

1. **Verify thread count:**
   ```bash
   echo $OMP_NUM_THREADS
   # Should match your desired thread count
   ```

2. **Check OpenMP is linked:**
   ```bash
   ldd build/simucell3d | grep gomp
   # Should show libgomp.so.1
   ```

3. **Use thread binding:**
   ```bash
   export OMP_PROC_BIND=close
   export OMP_PLACES=cores
   ./simucell3d params.xml
   ```

4. **Small workloads**: Low efficiency is normal for <512 cells with 8+ threads. The workload is too small to parallelize effectively.

---

### CPU utilization is high but simulation is still slow

**Possible causes:**

1. **Lots of contact calculations**: Dense cell packing increases O(n²) contact checks
2. **Frequent mesh refinement**: Many edge splits/collapses per iteration
3. **Small time step**: Physics requires many iterations

**Solutions:**
- Use `--diagnostics-csv=perf.csv` to identify bottlenecks
- Consider reducing `<writing_frequency>` to minimize I/O
- For large simulations, use `--schedule=adaptive`

---

### Simulation runs faster initially then slows down

**This is normal** - as cells grow and divide, the workload increases:
- More cells = more contact checks (O(n²))
- More triangles = more force calculations

**Tips:**
- Use `--schedule=adaptive` to adapt to changing workload
- Monitor with `--diagnostics-csv=perf.csv`

---

## Parameter Configuration

### How do I increase cell count?

**Option 1:** Use scalability parameter files
```bash
./simucell3d ../parameters/scalability/parameters_growth_256cells.xml
```

**Option 2:** Modify growth parameters
```xml
<avg_growth_rate>2e-11</avg_growth_rate>    <!-- Faster growth -->
<avg_division_volume>1.2e-15</avg_division_volume>  <!-- Earlier division -->
<max_simulation_time>1e-3</max_simulation_time>  <!-- Run longer -->
```

---

### What do the parameters mean biologically?

| Parameter | Biological Meaning | Typical Range |
|-----------|-------------------|---------------|
| `bulk_modulus` | Cell stiffness/compressibility | 2000-3000 Pa |
| `surface_tension` | Cortical actin contractility | 0.0005-0.002 N/m |
| `bending_modulus` | Membrane rigidity | 1-3 × 10⁻¹⁸ J |
| `adherence_strength` | Cadherin-mediated adhesion | 10⁸-10⁹ Pa/m |
| `growth_rate` | Volume increase rate | 10⁻¹¹-10⁻²⁰ m³/s |
| `division_volume` | Volume at which cells divide | 1.4 × 10⁻¹⁵ m³ |

See [Parameter Reference](../user-guide/parameter-reference.md) for complete reference.

---

### How do I change the physics model?

SimuCell3D has three compile-time selectable physics options in `include/global_configuration.hpp`:

```cpp
#define CONTACT_MODEL_INDEX 2      // 0=springs, 1=node-coupling, 2=face-coupling
#define DYNAMIC_MODEL_INDEX 0      // 0=semi-implicit, 1=overdamped
#define POLARIZATION_MODE_INDEX 1  // 0=none, 1=contact-based, 2=spatial
```

**After changing, full rebuild required:**
```bash
cd build && rm -rf * && cmake .. && make -j$(nproc)
```

---

### Which scheduling mode should I use?

**Quick answer:** Use `--schedule=adaptive` for all cases.

**Detailed guidance:**
| Cell Count | Best Mode | Why |
|------------|-----------|-----|
| < 128 | `dynamic` | High workload heterogeneity |
| 128-512 | `guided` | Balance load distribution + cache |
| 512-2048 | `auto` | Let auto-selector decide |
| > 2048 | `static` or `auto` | Uniform workload, cache matters |

---

## Results & Visualization

### How do I view VTK output files?

1. Download [ParaView](https://www.paraview.org/download/) (free, open-source)
2. File → Open → Select `mesh_*.vtk` files (select all)
3. Click **Apply** in Properties panel
4. Use timeline slider to animate

**Useful ParaView tips:**
- Color by "cell_type" to distinguish cell types
- Use "Slice" filter to see internal structure
- Export animation via File → Save Animation

---

### What's in simulation_statistics.csv?

Each row is one iteration with columns:
- `iteration`: Current time step
- `time`: Simulation time in seconds
- `num_cells`: Current cell count
- `total_volume`: Sum of all cell volumes
- `avg_pressure`: Average internal cell pressure
- Various timing columns if `--diagnostics-csv` used

---

### Simulation finished but no output files

Check:
1. Output directory exists and is writable
2. `<writing_frequency>` in XML isn't zero or very large
3. Simulation ran long enough (`<max_simulation_time>` or `<max_iteration>`)

---

## Development

### How do I run the test suite?

```bash
cd build
ctest              # Run all tests
ctest -V           # Verbose output
ctest -R mesh      # Run only mesh tests
ctest -j$(nproc)   # Parallel test execution
```

---

### How do I add a new feature?

See [contributing.md](./contributing.md) for:
- Code style guidelines
- Pull request process
- Testing requirements

---

### Debug build has different behavior

**This is expected.** Debug builds disable OpenMP parallelization for easier debugging. Always use Release builds for performance testing:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

---

## Still Need Help?

- **Paper:** [SimuCell3D.pdf](../SimuCell3D.pdf) - Scientific background
- **Issues:** Report bugs on GitHub
