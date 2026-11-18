(ug-input-meshes)=
# Input meshes

The {term}`input mesh` holds the initial geometry of a simulation: the surface of every cell
and the cell type of each one. Its path is given by `input_mesh_file_path` in the
{term}`parameter file` ({doc}`/user_guide/11_parameter_file`). This chapter describes the file
format, the meshes shipped in `data/input_meshes/`, and how to build a new mesh.

## File format

SimuCell3D reads the legacy VTK format in ASCII, as an unstructured grid in which every
biological cell is one polyhedron (VTK cell type 42). A minimal file with two cubic cells,
taken from `data/input_meshes/2_cubes.vtk` (metadata lines removed):

```text
# vtk DataFile Version 4.2
vtk output
ASCII
DATASET UNSTRUCTURED_GRID
POINTS 16 float
5e-07 0 0 7.5e-06 0 0 7.5e-06 0 7e-06
5e-07 0 7e-06 5e-07 7e-06 0 7.5e-06 7e-06 0
5e-07 7e-06 7e-06 7.5e-06 7e-06 7e-06 8e-06 0 0
1.5e-05 0 0 1.5e-05 0 7e-06 8e-06 0 7e-06
8e-06 7e-06 0 1.5e-05 7e-06 0 8e-06 7e-06 7e-06
1.5e-05 7e-06 7e-06

CELLS 2 100
49 12 3 0 1 3 3 2 3 1 3 0 4 1 3 5 1 4 3 0 3 4 3 6 4 3 3 1 5 2 3 7 2 5 3 5 4 7 3 6 7 4 3 3 2 6 3 7 6 2
49 12 3 8 9 11 3 10 11 9 3 8 12 9 3 13 9 12 3 8 11 12 3 14 12 11 3 9 13 10 3 15 10 13 3 13 12 15 3 14 15 12 3 11 10 14 3 15 14 10
CELL_TYPES 2
42
42

CELL_DATA 2
FIELD FieldData 1
cell_type_id 1 2 int
0 0
```

The sections are:

`# vtk DataFile Version 4.2`
: The version line is required. A version other than 4.2 is accepted with a warning.

`POINTS <n> float` (or `double`)
: The node coordinates, three numbers per node, in metres. Line breaks are free. The shipped
  meshes have cells about 10 to 20 micrometres across, so coordinates are of the order of
  $10^{-5}$.

`CELLS <number of cells> <size>`
: One line per cell. The first integer is the number of integers that follow on the line,
  the second is the number of faces of the cell; then each face is written as its number of
  nodes followed by the node indices (0-based, into `POINTS`). In the example, each cube has
  12 triangles: $1 + 12 \times 4 = 49$ integers. **Each cell must be on a single line**; the
  reader splits this section by line.

`CELL_TYPES <number of cells>`
: One `42` (polyhedron) per cell. Any other value stops the run.

`cell_type_id`
: An integer array with one value per cell, in a `CELL_DATA` field. It selects the
  `<cell_type>` block of the parameter file by position: 0 is the first block in the file, 1
  the second, and so on (see
  [Cell types and the input mesh](11_parameter_file.md#cell-types-and-the-input-mesh)). It is not the
  {term}`cell class`; the class comes from that block's `global_cell_id`. The array is
  required. It may be accompanied by other arrays in the same `FIELD`.

### Requirements on the cell surfaces

- Each cell must be a closed surface without boundary in which every edge is shared by
  exactly two faces (a manifold surface). A cell that fails this check stops the run with
  "The surface of cell N is not properly triangulated."
- Faces may have any number of nodes if `perform_initial_triangulation` is 1: the surfaces
  are then re-meshed at the start with edge lengths between `min_edge_length` and three times
  that value. If `perform_initial_triangulation` is 0, every face must be a triangle, and the
  triangles should already have edge lengths close to that range.
- The orientation of the faces does not matter: when a cell is created, its faces are
  re-oriented so that their normals point outward.
- Cells may share points in the file. Each cell receives its own copy of the nodes it uses,
  so cells that touch in the file are separate surfaces in the simulation.
- Cells should not overlap. SimuCell3D does not check this.

## Shipped meshes

All meshes are in `data/input_meshes/`. Counts are from the files; "polygons" means that the
faces are quadrilaterals and pentagons, so the mesh needs `perform_initial_triangulation` 1.

| File | Cells | `cell_type_id` values (count) | Faces | Used by |
|---|---|---|---|---|
| `fig_3_vesicle.vtk` | 434 | 0 (432), 1 (1), 2 (1) | triangles | `core/parameters_vesicle*.xml`, `benchmarks/fig3e_*.xml` |
| `fig_1_vesicle.vtk` | 50 | 0 (48), 1 (1), 2 (1) | triangles | |
| `fig_1_sheet_geometry.vtk` | 118 | 0 | triangles | `core/parameters_sheet.xml` |
| `fig_1_tube.vtk` | 192 | 0 | triangles | `core/parameters_tube.xml` |
| `big_sphere.vtk` | 1 | 0 | triangles | `core/parameters_default_*.xml`, `parameters_paper_exact*.xml` |
| `paper_sphere.vtk` | 1 | 0 | triangles | `longrun_fast_growth.xml` |
| `sphere.vtk` | 1 | 0 | triangles | |
| `cube.vtk`, `big_cube.vtk` | 1 | 0 | triangles | |
| `2_cubes.vtk`, `2_big_cubes.vtk` | 2 | 0 | triangles | |
| `4_cubes.vtk` | 4 | 0 | triangles | |
| `cell_triplet.vtk` | 3 | 0 | triangles | |
| `young_laplace_test_geom.vtk` | 2 | 0 (1), 1 (1) | triangles | |
| `lumen_initial_mesh.vtk` | 13 | 0 (12), 2 (1) | polygons | |
| `lumen_initial_mesh_triangulated.vtk` | 13 | 0 (12), 2 (1) | triangles | |
| `embryo_1.vtk`, `embryo_5.vtk` | 16 | 0 to 15, one cell each | triangles | |

The `embryo_*.vtk` meshes give every cell its own `cell_type_id`, so a parameter file for them
needs 16 `<cell_type>` blocks. No shipped parameter file uses them.

In `fig_3_vesicle.vtk` and `fig_1_vesicle.vtk`, `cell_type_id` 1 and 2 select the second and
third `<cell_type>` blocks, which in the shipped files are the ECM cell and the lumen cell.

## Building a mesh

A new mesh is usually assembled from existing cells or drawn in a 3D modelling program, then
converted to the format above.

### ParaView

[ParaView](https://www.paraview.org/download/) opens the legacy VTK files directly and can
assemble a tissue from cells of existing meshes:

1. Open a mesh (for example a shipped one, or the `cell_data/result_<n>.vtk` output of a
   previous run; see [Reusing simulation output](#reusing-simulation-output)).
2. Select a cell with the *Select Cells On* tool and click on it.

   :::{figure} /images/cell_selection.png
   :alt: ParaView window with one cell of a tissue selected and highlighted.

   Selecting one cell in ParaView.
   :::

3. Extract it with the *Extract Selection* filter.

   :::{figure} /images/extract_from_selection.png
   :alt: ParaView with the Extract Selection filter applied to the selected cell.

   The selected cell extracted into its own dataset.
   :::

4. Move, rotate or scale it with the *Transform* filter.

   :::{figure} /images/transform_filter.png
   :alt: ParaView Transform filter panel with translation, rotation and scale fields.

   Placing the extracted cell with the Transform filter.
   :::

5. Combine several datasets into one with *Append Datasets*, then save the result with
   *File > Save Data* as a legacy VTK file in ASCII.

   :::{figure} /images/append_filter.png
   :alt: ParaView Append Datasets filter combining several cell datasets.

   Combining cells into one mesh with Append Datasets.
   :::

Check that the saved file still contains the `cell_type_id` array and that each cell is one
polyhedron (see [Checking a mesh](#checking-a-mesh)).

### MeshLab and Blender

[MeshLab](https://www.meshlab.net/) has filters to smooth, simplify and re-mesh surfaces,
which helps to prepare cell shapes that come from imaging data.

:::{figure} /images/meshlab_filters.png
:alt: MeshLab filter menu with smoothing, cleaning and remeshing filters.

MeshLab filters for cleaning and re-meshing surfaces.
:::

[Blender](https://www.blender.org/download/) is suited to drawing cells from scratch:
modelling, sculpting and positioning closed surfaces.

:::{figure} /images/blender.png
:alt: Blender viewport with several closed cell surfaces being modelled.

Modelling cell surfaces in Blender.
:::

Both programs export STL, PLY or OBJ files. These formats store all triangles of a scene as
one list, without grouping them by cell, so they have to be converted.

### Converting STL, PLY and OBJ files

`scripts/mesh_conversion/convert_to_cell_mesh.py` groups the triangles of a mesh into cells
and writes a legacy VTK file with one polyhedron per cell. It reads `.stl`, `.ply`, `.obj`,
`.vtp`, `.vtu` and legacy `.vtk` files and needs the Python packages `vtk` and `numpy`:

```console
$ pip install vtk numpy
$ python3 scripts/mesh_conversion/convert_to_cell_mesh.py tissue.stl tissue.vtk
```

The script assigns faces to cells by connectivity: every connected set of triangles becomes
one cell. Cells that share a node or an edge in the input are therefore merged into one
cell; keep a gap between cell surfaces before converting.

The converted file has no `cell_type_id` array, which SimuCell3D requires. Append one; for
example, to give every cell `cell_type_id` 0:

```console
$ n=$(grep -m1 '^CELLS' tissue.vtk | cut -d' ' -f2)
$ { printf '\nCELL_DATA %s\nFIELD FieldData 1\ncell_type_id 1 %s int\n' "$n" "$n"; yes 0 | head -n "$n"; } >> tissue.vtk
```

For several cell types, write the values in the order of the cells in the `CELLS` section.

### Reusing simulation output

The files `cell_data/result_<n>.vtk` written by a run ({doc}`/user_guide/14_output_files`)
have the input format: one polyhedron per cell on one line, triangulated faces, and a
`cell_type_id` array. They can be used as the input mesh of a new run, for example to start
from a relaxed tissue. Two points need care:

- The output `cell_type_id` is the `global_cell_id` of each cell's type, while the input
  `cell_type_id` is a position in the parameter file. The two agree only if the `<cell_type>`
  blocks of the new parameter file are in the order of their `global_cell_id` (0, 1, 2, ...),
  as in all shipped files.
- The output does not store growth rates or target volumes in a form that the reader uses.
  Each cell starts with its current volume as target volume and draws a new growth rate and
  division volume from the distributions of its cell type.

## Checking a mesh

Open the file in ParaView, choose the cell-selection tool and click on a cell. If the file is
correct, the whole cell is highlighted:

:::{figure} /images/correct_cell_mesh.png
:alt: ParaView showing one complete cell highlighted after a click.

Correct mesh: the selection picks a whole cell.
:::

If only one triangle is highlighted, each face is a separate VTK cell and the file has to be
converted:

:::{figure} /images/incorrect_cell_mesh.png
:alt: ParaView showing a single triangle highlighted after a click.

Incorrect mesh: the selection picks a single triangle.
:::

Colour the mesh by `cell_type_id` to check the cell types. The quickest test of a new mesh is
a short run: set `simulation_duration` to a few time steps and check that the run starts and
writes `cell_data/result_1.vtk`.

## Error messages

| Message | Cause |
|---|---|
| `ERROR: input mesh file <path> not found` | Neither the path as given nor the path relative to the source directory exists (see [Paths](11_parameter_file.md#paths)). |
| `ERROR: header with .vtk file version not found` | Not a legacy VTK file. |
| `ERROR: input mesh file, nodes have incorrect format: <type>` | `POINTS` type other than `float` or `double`. |
| `ERROR: cell is not a polyhedron (type 42) but has type: <n>` | A `CELL_TYPES` entry is not 42; each face may have been written as a separate cell. |
| `ERROR mesh_reader: could not locate the cell_type_id array in the input VTK file` | The `cell_type_id` array is missing. |
| `The cell type id of cell N (X) is not valid` | A `cell_type_id` value is not smaller than the number of `<cell_type>` blocks. |
| `If you disable the initial triangulation, the input cell meshes must already be triangulated` | `perform_initial_triangulation` is 0 and a face is not a triangle. |
| `The cell N could not be triangulated after 10 attempts` | Initial triangulation or cell creation failed ten times; the preceding lines give the reason for each attempt. |

:::{seealso}
- {doc}`/user_guide/05_mesh_refinement`: initial triangulation and the mesh refiner.
- {doc}`/user_guide/08_cell_classes`: the cell classes selected through the cell types.
- {doc}`/user_guide/15_visualization`: viewing meshes in ParaView.
:::
