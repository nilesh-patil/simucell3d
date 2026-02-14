# Getting Started with SimuCell3D

Run your first 3D tissue simulation in under 5 minutes.

## Prerequisites

- **Linux, macOS, or Windows with WSL** (Docker recommended for easiest setup)
- For Docker: [Docker Desktop](https://www.docker.com/products/docker-desktop/)
- For manual build: GCC 9+, CMake 3.0+, OpenMP

## Quick Start (Docker - Recommended)

This is the fastest way to get SimuCell3D running.

```bash
# Clone the repository
git clone https://github.com/nilesh-patil/simucell3d.git
cd SimuCell3D

# Build the Docker image (~5 minutes first time)
docker build -t simucell3d .

# Create output directory
mkdir -p simulation_results

# Run your first simulation (vesicle formation, ~20-30 seconds)
docker run -v "$(pwd)/simulation_results:/app/simulation_results" \
    simucell3d ../parameters/core/parameters_vesicle.xml --schedule=adaptive
```

## Quick Start (Manual Build)

If you prefer to build from source:

```bash
# Clone and build
git clone https://github.com/nilesh-patil/simucell3d.git
cd SimuCell3D
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)

# Run your first simulation
./simucell3d ../parameters/core/parameters_vesicle.xml --schedule=adaptive
```

## What Just Happened?

You simulated a **vesicle formation** with ~13 cells over 1,000 time steps. The simulation:

1. **Loaded** a VTK mesh defining initial cell geometry
2. **Computed** cell mechanics (pressure, surface tension, contact forces)
3. **Integrated** equations of motion using semi-implicit Euler
4. **Wrote** results to `simulation_results/` every 50 iterations

### Output Files

```
simulation_results/
├── mesh_*.vtk           # Cell geometry snapshots (view in ParaView)
├── face_data_*.vtk      # Surface face data
└── simulation_statistics.csv  # Time series of simulation metrics
```

## View Results in ParaView

1. Download [ParaView](https://www.paraview.org/download/) (free, open-source)
2. Open ParaView → File → Open → Select `mesh_*.vtk` files
3. Click **Apply** in the Properties panel
4. Use the timeline slider to animate the simulation

## Next Steps

### Try Different Simulations

```bash
# Tube morphogenesis (~5 minutes)
./simucell3d ../parameters/core/parameters_tube.xml --schedule=adaptive

# Epithelial sheet (~3 minutes)
./simucell3d ../parameters/core/parameters_sheet.xml --schedule=adaptive

# Larger scale (256 cells, ~10-20 minutes)
./simucell3d ../parameters/scalability/parameters_growth_256cells.xml --schedule=adaptive
```

### Command-Line Options

| Option | Description |
|--------|-------------|
| `--schedule=MODE` | Scheduling mode: `adaptive` (default), `static`, `dynamic`, `guided` |
| `--output-dir=PATH` | Override output directory |
| `--diagnostics-csv=FILE` | Export performance metrics to CSV |

### Performance Tips

For multi-hour production simulations:

```bash
# Set thread count to physical cores (not hyperthreads)
export OMP_NUM_THREADS=$(nproc)
export OMP_PROC_BIND=close

# Run with auto-scheduler
./simucell3d params.xml --schedule=adaptive
```

## Learn More

| Guide | Description |
|-------|-------------|
| [faq.md](./faq.md) | Common issues and troubleshooting |
| [Parameter Configuration](../user-guide/parameter-reference.md) | Understanding XML parameter files |
| [SimuCell3D Paper](../SimuCell3D.pdf) | Scientific background and validation |

## Example: Customize Cell Growth Rate

Edit the parameter file to change cell behavior:

```xml
<!-- In parameters_vesicle.xml -->
<cell_type_parameters>
    <!-- Increase growth rate for faster cell division -->
    <avg_growth_rate>2e-11</avg_growth_rate>  <!-- Default: 1e-11 -->

    <!-- Reduce division volume for more frequent divisions -->
    <avg_division_volume>1.2e-15</avg_division_volume>  <!-- Default: 1.4e-15 -->
</cell_type_parameters>
```

## Getting Help

- **FAQ**: [faq.md](./faq.md) - Common issues and solutions
- **Issues**: [GitHub Issues](https://github.com/nilesh-patil/simucell3d/issues)
- **Paper**: [SimuCell3D Paper](../SimuCell3D.pdf) - Scientific details
