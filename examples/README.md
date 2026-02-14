# SimuCell3D Examples

This directory contains example simulations demonstrating SimuCell3D capabilities.

## Available Examples

| Example | Description | Runtime |
|---------|-------------|---------|
| [single_vesicle](single_vesicle/) | Basic vesicle mechanical equilibration | ~30s |

## Quick Start

1. **Build SimuCell3D** (from repository root):
   ```bash
   mkdir -p build && cd build
   cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)
   ```

2. **Run an example** (from build directory):
   ```bash
   ./simucell3d ../examples/single_vesicle/parameters.xml
   ```

3. **View results** in ParaView or your preferred VTK viewer.

## Creating Your Own Simulations

1. Copy an example directory as a starting point
2. Modify the `parameters.xml` file
3. Use a different input mesh from `data/input_meshes/`
4. Adjust simulation duration and output frequency as needed

See each example's README for detailed parameter descriptions.

## Parameter File Structure

All SimuCell3D simulations are configured via XML parameter files with two main sections:

- **`<numerical_parameters>`**: Simulation settings (time step, duration, mesh paths)
- **`<cell_types>`**: Cell mechanical properties and face type definitions

Full parameter documentation: [Parameter Reference](../doc/user-guide/parameter-reference.md)
