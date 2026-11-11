# Parameter screening scripts

Scripts to run a SimuCell3D parameter screen on a cluster with the Slurm batch scheduler: one
simulation per row of a parameter table, submitted as a Slurm job array.

The full description is in the user guide, chapter "Parameter screening"
([`doc/user_guide/17_parameter_screening.md`](../../doc/user_guide/17_parameter_screening.md),
published at <https://nilesh-patil.github.io/simucell3d/stable/user_guide/17_parameter_screening.html>).

## Files

| File | Purpose |
|---|---|
| `parameter_table.csv` | Example parameter table (five rows). Each column is one parameter of the template. |
| `parameter_file_model.py` | XML template of the parameter file, with one placeholder per table column. |
| `launch_screening.py` | Writes one parameter file per table row, builds SimuCell3D in `build_<screen name>/` in the repository root, and submits the Slurm job array. |
| `collect_screening_data.py` | Collects the last meshes and the statistics of every run into `<screen folder>/summary_folder/` and marks failed runs. |

## Usage

Requirements: Python 3 with `pandas` and `numpy`, CMake and a C++ compiler on the submission
node, and Slurm (`sbatch`).

Run both scripts from this directory and give absolute paths:

```console
$ cd scripts/parameter_screening
$ python3 launch_screening.py /abs/path/my_table.csv my_screen /abs/path/to/scratch
$ # ... after the jobs have finished:
$ python3 collect_screening_data.py /abs/path/to/scratch/my_screen
```

Before the first use on a cluster, edit the `sbatch` command in `launch_screening.py`: it
requests 4 CPUs, 48 hours and 2048 MB per CPU per run and sends e-mail to
`<login>@bsse.ethz.ch`. `OMP_NUM_THREADS` in that command has no effect in SimuCell3D 2.1; add
`--threads=N` to the `./simucell3d` call to fix the thread count.

Each Slurm task appends `simucell3d exit status: <code>` to its `std_out_err/stdout_<i>.txt`.
`collect_screening_data.py` marks a run as crashed (`has_crashed`) if that line is missing or the
code is not 0, or if the run's output folder is missing. It rewrites the parameter table in the
screen folder in place and stops if `summary_folder/` already exists. See the user guide for
details.
