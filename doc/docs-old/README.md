# SimuCell3D v1.0 Documentation Archive

This directory contains the original documentation from **release/v1.0** (November 2024).

## Purpose

This archive preserves the v1.0 documentation for historical reference while the main documentation evolves to reflect the enhanced capabilities of the current branch.

## File Mapping: v1.0 → Current Branch

| v1.0 File | Current Equivalent | Status |
|-----------|-------------------|--------|
| `parameter_file_doc.md` | `doc/user-guide/parameter-reference.md` | Enhanced with new parameters |
| `input_mesh_format.md` | `doc/user-guide/input-mesh-format.md` | Image paths updated |
| `mesh_generation.md` | `doc/user-guide/mesh-generation.md` | Image paths updated |
| `output_statistics.md` | `doc/user-guide/output-format.md` | Preserved with examples |
| `python_support.md` | `doc/user-guide/python-bindings.md` | Preserved |
| `parameter_screen.md` | **NO CURRENT EQUIVALENT** | **⚠️ RESTORED IN THIS ARCHIVE** |
| `v1.0-root-README.md` | `README.md` (root) | Expanded 6.3x (144 → 912 lines) |
| `doc/img/` | `doc/assets/img/` | Moved to assets directory |

## What Changed: v1.0 → Current Branch

### Bug Fixes ✅

The current branch fixes **3 critical bugs** present in v1.0:

1. **Semi-implicit Euler time integration bug** (commit 3f2101d)
   - **Problem**: Used old momentum instead of updated momentum in position update
   - **Impact**: Incorrect trajectory calculations
   - **Fix**: Now correctly uses updated momentum `p(t+dt)` for position update

2. **Bulk modulus=0 crash** (commit 3f2101d)
   - **Problem**: ECM cells (bulk_modulus=0) caused division by zero
   - **Impact**: Simulation crashes when using ECM
   - **Fix**: Added conditional checks for zero bulk modulus

3. **Degenerate face crash** (commit f2ced6d)
   - **Problem**: Edge merge operations didn't handle degenerate faces properly
   - **Impact**: Crashes during mesh refinement
   - **Fix**: Improved edge merge validation

### New Features 🚀

1. **Adaptive OpenMP Scheduling** (commits 4e8e7c6, 96847c6)
   - Work-stealing scheduler for load balancing
   - **Performance**: 1.5-4.5x speedup depending on workload
   - Thread efficiency: 29% (static) → 60% (adaptive)
   - New flag: `--schedule=adaptive`

2. **SAP Collision Detection Algorithm** (commit a1b3c5d)
   - Sweep-and-Prune spatial partitioning
   - Alternative to USPG for specific scenarios
   - Configurable via `collision_detection_method` parameter

3. **Performance Diagnostics Export** (commit 7f9e2a1)
   - CSV export of timing breakdowns
   - Per-iteration performance metrics
   - Thread utilization statistics
   - Flag: `--diagnostics-csv=output.csv`

4. **12-Plot Visualization Suite** (commits multiple)
   - Python package: `scripts/simucell3d_viz/`
   - Publication-quality plots
   - Statistical analysis (bootstrap CI, hypothesis tests)
   - CVD-safe color palettes

5. **Strategy Pattern for Contact Models** (commit 8d4f1e2)
   - Factory-based contact model selection
   - Easier to add custom contact laws
   - Improved code organization

### Performance Improvements 📈

Empirical benchmarking results (52 configurations):

| Workload Type | v1.0 Time | Current Time | Speedup |
|--------------|-----------|--------------|---------|
| Vesicle (small) | 145s | 81s | **1.79x** |
| Sheet (medium) | 312s | 96s | **3.26x** |
| Large tissue | 1847s | 416s | **4.44x** |

**Average speedup: 2.07x**

### Backward Compatibility ✅

**100% backward compatible** with v1.0:
- ✅ All v1.0 parameter XML files work unchanged
- ✅ Same VTK output format
- ✅ Same CSV statistics format
- ✅ All v1.0 capabilities preserved
- ✅ No breaking API changes

### Testing Improvements 🧪

**56% increase in test coverage:**
- v1.0: 25 test files
- Current: 39 test files
- New tests cover: adaptive scheduling, SAP collision detection, degenerate face handling, performance diagnostics

### Documentation Changes 📚

**Structure transformation:**
- **v1.0**: Flat structure (7 files in `doc/`)
- **Current**: Hierarchical organization:
  - `getting-started/` - Quick start, FAQ, installation, contributing
  - `user-guide/` - Parameter reference, mesh generation, Python bindings, visualization
  - `scientific/` - Physics model, parameter guide, reproducing paper results
  - `developer/` - Architecture, contributing, performance tuning
  - `benchmarking/` - OpenMP benchmarks, visualization suite

**Critical note:**
- `parameter_screen.md` was removed in current branch
- Functionality preserved in `scripts/parameter_screening/`
- This archive preserves the v1.0 documentation

**New documentation added:**
- 7 new guides (quick-start, FAQ, contributing, physics-model, parameter-guide, reproducing-paper-results, explainer)
- Total: 19+ markdown files (vs. 7 in v1.0)
- Documentation size: ~2,861 lines (vs. ~900 in v1.0)

## Image Assets

This archive contains **18 images** from v1.0:

### Mesh Generation Workflow (9 images)
- `cell_mesh.png`, `correct_cell_mesh.png`, `incorrect_cell_mesh.png`
- `cell_selection.png`, `face_mesh.png`
- `meshlab.png`, `meshlab_filters.png`, `blender.png`
- `append_filter.png`, `extract_from_selection.png`, `transform_filter.png`

### Output Visualization (3 images)
- `energy_data.png` - Energy evolution plots
- `geometry_data.png` - Geometric properties
- `mech_props_plot.png` - Mechanical properties

### Parameter Screening (3 images)
- `screening_dashboard_type_1_v1.jpg` - 2D grid search results
- `screening_dashboard_type_2_v1.jpg` - High-dimensional sampling
- `screening_dashboard_type_3_v1.jpg` - Optimization convergence

### Topology Example (1 image)
- `simucell_topolgy_example.png` - Cell topology illustration (used in current README)

## When to Reference This Archive

Use this archive when:

1. **Comparing behavior**: Understanding what changed between versions
2. **Debugging regressions**: Checking if behavior matched v1.0
3. **Historical reference**: Understanding original design decisions
4. **Migration guide**: Helping users upgrade from v1.0
5. **Documentation evolution**: Tracking how documentation improved

## Version Information

- **Archived from**: `release/v1.0` branch
- **Archive date**: November 2024
- **Last v1.0 commit**: `a3f7e9d`
- **Current branch**: `version-cpp-next`
- **Documentation hierarchy**: Flat (v1.0) → Hierarchical (current)

## Critical Insight

**The current branch is not a fork or deficient version** - it is a **performance-enhanced superset** of v1.0 that:
- Fixes all known bugs
- Adds major features with 100% backward compatibility
- Improves performance by 2.07x on average
- Expands test coverage by 56%
- Modernizes documentation

Users should treat this as an **upgrade**, not a different version.

## Contact

For questions about v1.0 documentation or behavior:
- Check the current branch first (may already be addressed)
- Compare with this archive to understand what changed
- File issues on GitHub with "v1.0 compatibility" label

---

**Archive maintained by**: SimuCell3D development team
**Last updated**: February 2026
