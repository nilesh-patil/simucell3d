(api-io)=
# Input and output

These classes read the parameter file and the input mesh, create the cells, and write the
output meshes and statistics. {cpp:class}`parameter_reader` parses the XML parameter file
({doc}`/user_guide/11_parameter_file`); {cpp:class}`mesh_reader` reads the legacy VTK input mesh
({doc}`/user_guide/12_input_meshes`); {cpp:class}`simulation_initializer` combines both into the list of
cells passed to the {cpp:class}`solver`. During the run, {cpp:class}`mesh_writer` writes the
cell and face meshes, and a statistics writer writes one row per cell per sample, either to
`simulation_statistics.csv` ({cpp:class}`csv_file_statistics_writer`) or to a string
({cpp:class}`string_statistics_writer`, used by the Python module). The arrays written to the
output meshes are defined by the data mappers in `mesh_data.hpp`; adding a mapper there adds
an array to every output file ({doc}`/user_guide/14_output_files`).

Reading happens once, before the solver is constructed. During the run, the solver calls
{cpp:func}`mesh_writer::write` when a sampling period has elapsed (first step of an
iteration) and `abstract_statistics_writer::write_data()` every 50 iterations and once at the
end.

| Class or type | Header | Role |
|---|---|---|
| {cpp:class}`parameter_reader` | `io/parameter_reader.hpp` | XML parameter file to parameter structures |
| {cpp:class}`mesh_reader` | `io/mesh_reader.hpp` | Legacy VTK input mesh to one {cpp:struct}`mesh` per cell |
| {cpp:class}`simulation_initializer` | `io/simulation_initializer.hpp` | Builds the cell objects and the parameters passed to the solver |
| {cpp:class}`mesh_writer` | `io/mesh_writer.hpp` | Writes `cell_data/` and `face_data/` |
| {cpp:class}`abstract_statistics_writer` | `io/statistics_writer.hpp` | Interface of the statistics writers |
| {cpp:class}`csv_file_statistics_writer` | `io/statistics_writer.hpp` | Writes `simulation_statistics.csv` (command-line runs) |
| {cpp:class}`string_statistics_writer` | `io/statistics_writer.hpp` | Collects the statistics in memory (Python module) |
| {cpp:struct}`cell_data_mapper`, {cpp:struct}`face_data_mapper`, {cpp:struct}`node_data_mapper` | `io/mesh_data_mappers.hpp` | One output array or statistics column each |
| mapper lists | `io/mesh_data.hpp` | The arrays and columns actually written |

## Reading input

```{doxygenclass} parameter_reader
:project: simucell3d
:members:
```

```{doxygenclass} mesh_reader
:project: simucell3d
:members:
```

```{doxygenclass} simulation_initializer
:project: simucell3d
:members:
```

## Writing output

```{doxygenclass} mesh_writer
:project: simucell3d
:members:
```

```{doxygenclass} abstract_statistics_writer
:project: simucell3d
:members:
```

```{doxygenclass} csv_file_statistics_writer
:project: simucell3d
:members:
```

```{doxygenclass} string_statistics_writer
:project: simucell3d
:members:
```

## Output arrays

```{doxygenstruct} cell_data_mapper
:project: simucell3d
:members:
```

```{doxygenstruct} face_data_mapper
:project: simucell3d
:members:
```

```{doxygenstruct} node_data_mapper
:project: simucell3d
:members:
```

```{doxygenfile} mesh_data.hpp
:project: simucell3d
```
