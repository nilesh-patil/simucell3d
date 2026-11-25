(ug-index)=
# User guide

The user guide explains the model that SimuCell3D simulates and how to run and analyse
simulations. Chapters 1 to 10 describe the model: the mechanics of a cell, contact between
cells, time integration, mesh maintenance, growth and division, polarization, the cell classes,
the safeguards added in 2.1 and the compile-time options. Chapters 11 to 18 describe the
parameter file, input meshes, running simulations, output files, visualisation, the Python
interface, parameter screening and parallel performance.

The equations follow the Methods of Runser, Vetter & Iber (2024) and its Supplementary
Information. Where the 2.1 code differs from the paper, the chapters say so in a note. Where a
chapter quotes a measured limit of the model (a stable time step, a share of refused divisions
or unlogged deaths), the measurement comes from the November 2026 benchmark and is stated in
full in {ref}`benchmarks`.

**Model.** Read in order for an understanding of what a simulation computes:

1. {doc}`01_model_overview`: cells as closed triangulated surfaces, cell types, cell classes and
   face types, the solver loop, compile-time and run-time choices.
2. {doc}`02_cell_mechanics`: the cell energy, pressure, surface tension, area elasticity, bending
   and their nodal forces.
3. {doc}`03_contact`: the three contact models, contact detection and the coupling of nodes.
4. {doc}`04_time_integration`: the dynamic and overdamped equations of motion, choosing and
   checking the time step, output intervals.
5. {doc}`05_mesh_refinement`: edge-length bounds, split, collapse and swap, the initial
   triangulation.
6. {doc}`06_growth_division_removal`: growth of the target volume, division along the longest
   axis, refused divisions, removal.
7. {doc}`07_polarization`: face types and how the polarization modes assign them.
8. {doc}`08_cell_classes`: epithelial, ECM, lumen, nucleus and static cells.
9. {doc}`09_safeguards`: what 2.1 does with damaged or diverged cells, and how to count it.
10. {doc}`10_compile_time_configuration`: the configuration header and how to rebuild.

**Running and analysing simulations.** Read as needed:

11. {doc}`11_parameter_file`: every tag of the XML parameter file.
12. {doc}`12_input_meshes`: the input mesh format and how to build meshes.
13. {doc}`13_running_simulations`: command-line options, paths and long runs.
14. {doc}`14_output_files`: mesh files, the statistics CSV and the diagnostics CSV.
15. {doc}`15_visualization`: viewing results in ParaView and plotting statistics.
16. {doc}`16_python_interface`: running simulations from Python.
17. {doc}`17_parameter_screening`: running parameter screens on a cluster.
18. {doc}`18_parallel_performance`: threads, scheduling and the cost of large tissues.

Readers who want to run a first simulation before reading the model chapters can start with
{doc}`/getting_started` and the {doc}`/examples/index`. Terms are defined in the
{doc}`/glossary`.

```{toctree}
:hidden:
:maxdepth: 1

01_model_overview
02_cell_mechanics
03_contact
04_time_integration
05_mesh_refinement
06_growth_division_removal
07_polarization
08_cell_classes
09_safeguards
10_compile_time_configuration
11_parameter_file
12_input_meshes
13_running_simulations
14_output_files
15_visualization
16_python_interface
17_parameter_screening
18_parallel_performance
```
