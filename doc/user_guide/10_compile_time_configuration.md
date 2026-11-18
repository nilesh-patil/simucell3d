(ug-compile-time)=
# Compile-time configuration

Three model choices, the {term}`contact model`, the {term}`dynamic model` and the
{term}`polarization mode`, are fixed when SimuCell3D is compiled, together with two switches for
the output. They are preprocessor macros in one header file. This chapter lists them, explains
which shipped parameter files assume which values, and describes how to change them.

## The configuration header

The macros are defined in `include/global_configuration.hpp`. With its comments shortened, the
2.1 file reads:

```cpp
#define FACE_STORE_CONTACT_ENERGY false
#define WRITE_NORMALS_IN_OUTPUT_MESH_FILE true

//  - 0 : No polarization
//  - 1 : Polarization based on the contacts of the cell
//  - 2 : Polarization based on the discretization of the simulation space
#define POLARIZATION_MODE_INDEX 1

//  - 0 : springs between faces and vertices
//  - 1 : mechanically linking pairs of adjacent nodes
//  - 2 : mechanically linking pairs of adjacent faces
#define CONTACT_MODEL_INDEX 2

//  - 0 : Full equations of motions solved with semi-implicit euler
//  - 1 : Overdamped equations of motions solved with forward euler
#define DYNAMIC_MODEL_INDEX 0
```

| Macro | Values | Default | Effect |
|---|---|---|---|
| `CONTACT_MODEL_INDEX` | 0, 1, 2 | 2 | 0 node-face springs, 1 node-node coupling, 2 face-face coupling ({doc}`03_contact`) |
| `DYNAMIC_MODEL_INDEX` | 0, 1 | 0 | 0 full equations of motion with semi-implicit Euler, 1 overdamped with forward Euler ({doc}`04_time_integration`) |
| `POLARIZATION_MODE_INDEX` | 0, 1, 2 | 1 | 0 none, 1 from contacts, 2 from a voxel map ({doc}`07_polarization`) |
| `FACE_STORE_CONTACT_ENERGY` | `true`, `false` | `false` | Faces accumulate adhesion and repulsion energies; adds the columns `adhesion_energy` and `repulsion_energy` to the statistics CSV and the arrays `face_adhesion_energy` and `face_repulsion_energy` to the face meshes |
| `WRITE_NORMALS_IN_OUTPUT_MESH_FILE` | `true`, `false` | `true` | Writes face normals to the output meshes (and node normals with contact model 1); makes the files larger |

Note these restrictions and interactions:

- `DYNAMIC_MODEL_INDEX` 2 appears in source comments as an "improved Euler" scheme but is not
  implemented. Do not use it ({doc}`04_time_integration`).
- The polarization rules depend on the contact model ({doc}`07_polarization`): with contact
  models 1 and 2, polarization mode 1 never labels faces as basal.
- `FACE_STORE_CONTACT_ENERGY` records adhesion energies only with contact model 0, because models
  1 and 2 have no adhesion force; repulsion energies are recorded with all three.
- The column `cell_contact_area_fraction` of the statistics CSV exists only with polarization
  mode 1.

Other outputs depend on the configuration as well: polarization mode 2 creates the folder
`region_data/` ({doc}`14_output_files`).

## Configuration assumed by the parameter files

A parameter file does not record the configuration it needs, and SimuCell3D does not check it:
a file written for one contact model runs with another and gives a different simulation. The
shipped files state their configuration in their header comment:

| Parameter file | Polarization | Contact | Dynamic |
|---|---|---|---|
| `parameters/core/parameters_vesicle.xml`, `parameters_vesicle_quick.xml`, `parameters_vesicle_realistic_biology.xml`, `parameters_sheet.xml`, `parameters_tube.xml` | 1 | 2 | 0 |
| `parameters/longrun_fast_growth.xml`, `parameters/parameters_paper_exact_fast_growth.xml` | 1 | 2 | 0 |
| `parameters/core/parameters_default_dynamic.xml` | 1 | 1 | 0 |
| `parameters/parameters_paper_exact.xml` | 1 | 1 | 0 |
| `parameters/core/parameters_default_overdamped.xml` | 1 | 0 | 1 |
| `parameters/benchmarks/fig3e_monolayer.xml`, `fig3e_multilayer.xml` | 1 | 2 | 0 |

The default build (1, 2, 0) matches the files of the first two rows and the `fig3e_*` files.

:::{warning}
In the default build, the basal face type and `adherence_strength` have no effect: with contact
models 1 and 2, polarization mode 1 never labels a face basal, so faces facing the ECM or the
outside use the apical parameters ({doc}`07_polarization`), and adhesion is a rigid tie between
nodes rather than a force of strength $\omega$ ({doc}`03_contact`). Basal faces and $\omega$ act
only with `CONTACT_MODEL_INDEX 0` (and, for basal faces, `POLARIZATION_MODE_INDEX 1`). The shipped
files that define basal face types or a non-zero `adherence_strength` but are written for the
default build (the vesicle, sheet and tube files, `fig3e_*`, `longrun_fast_growth.xml` and
`parameters_paper_exact_fast_growth.xml`) have not been tested with contact model 0.
:::

:::{important}
The paper's Methods describe contact models 0 and 1; the simulations of its figures were solved
with the dynamic model. Contact model 2 is the default of 2.x and of the tag `v1.0` (commit
`38af451`), but the Zenodo archive cited by the paper (commit `5342bf8`) has contact model 1 as
its default ({doc}`/whats_new/v1.0`). When comparing with a figure of the paper, check which
contact model it used.
:::

## Changing the configuration

1. Edit the value in `include/global_configuration.hpp`.
2. Rebuild. From the `build/` directory:

   ```console
   $ make -j4
   ```

   Every source file that includes the header is recompiled, because CMake tracks header
   dependencies. A fresh build directory is not needed.
3. Run the parameter file written for that configuration.

The executable does not report its configuration (`simucell3d --version` prints only the version
and the git commit), so keep track of which build is which. Because the configuration is part of
the source tree, all build directories of one checkout share it. To keep two configurations
side by side, use a second checkout, for example with `git worktree add`, and build each one in
its own build directory.

The test suite is written for the default configuration; some tests are compiled only for a
particular contact model. Run `ctest` with the default configuration
({doc}`/developers/testing`).

:::{seealso}
- {doc}`/install` for the first build.
- {doc}`/examples/single_cell_dynamic_overdamped` for an example that needs two different builds.
- {doc}`/api/solver` for `global_configuration.hpp` in the API reference.
:::
