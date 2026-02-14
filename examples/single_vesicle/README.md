# Single Vesicle Example

This example demonstrates a basic SimuCell3D simulation of a single epithelial vesicle.

## What This Simulates

A vesicle is a closed epithelial cell layer surrounding a fluid-filled lumen. This example:
- Simulates mechanical equilibration of a vesicle
- Demonstrates cell-cell adhesion and repulsion
- Shows how surface tension affects cell shape

## Running the Example

From the build directory:

```bash
./simucell3d ../examples/single_vesicle/parameters.xml
```

Output will be written to `simulation_results/vesicle_example/`.

## Expected Runtime

- ~30 seconds on a modern multi-core CPU
- The simulation runs 10,000 iterations (1e-2 simulation time)

## Output Files

- `mesh_*.vtk`: Cell geometry at each sampling interval
- `simulation_statistics.csv`: Summary statistics per iteration

## Parameter Highlights

| Parameter | Value | Description |
|-----------|-------|-------------|
| `time_step` | 1e-7 | Numerical integration time step |
| `simulation_duration` | 1e-2 | Total simulation time |
| `cell_bulk_modulus` | 2500 | Cell incompressibility [Pa] |
| `surface_tension` | 1e-3 | Membrane surface tension [N/m] |

## Visualizing Results

Load the VTK files in [ParaView](https://www.paraview.org/):

1. Open ParaView
2. File -> Open -> Select all `mesh_*.vtk` files
3. Click "Apply" in the Properties panel
4. Use the play button to animate

## Modifying the Example

Try changing these parameters to see their effects:

- **Increase `cell_bulk_modulus`**: Makes cells more resistant to volume changes
- **Decrease `surface_tension`**: Allows more cell deformation
- **Change `sampling_period`**: Adjusts output frequency

## Configuration Requirements

This example requires these compile-time settings in `include/global_configuration.hpp`:

```cpp
#define CONTACT_MODEL_INDEX 2       // Face-face contact via coupling
#define DYNAMIC_MODEL_INDEX 0       // Semi-implicit Euler integration
#define POLARIZATION_MODE_INDEX 1   // Contact-based polarization
```

These are the default settings.
