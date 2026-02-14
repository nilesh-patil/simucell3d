# Nature Paper Golden Masters

This directory stores golden master VTK files for validating Nature paper figure reproduction.

## Directory Structure

```
nature_paper/
  fig3e/           # Figure 3e: Monolayer/Multilayer transition
  fig4cd/          # Figure 4c-d: Pseudostratified epithelia
  fig1e/           # Figure 1e: 125K cell scaling
```

## Generating Golden Masters

1. Build SimuCell3D in Release mode:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
   ```

2. Run the reproduction script:
   ```bash
   scripts/benchmarking/scientific/reproduce_fig3e.sh
   ```

3. Copy the final VTK output to this directory:
   ```bash
   cp simulation_results/nature_fig3e/monolayer/final.vtk baselines/nature_paper/fig3e/monolayer.vtk
   cp simulation_results/nature_fig3e/multilayer/final.vtk baselines/nature_paper/fig3e/multilayer.vtk
   ```

4. Commit with Git LFS (VTK files are tracked automatically):
   ```bash
   git add baselines/nature_paper/
   git commit -m "chore: update Nature paper golden masters"
   ```

## VTK File Storage

VTK files are stored via Git LFS (configured in `.gitattributes`).
