# Node-node coupling contact model tests

Tests of `contact_node_node_via_coupling` (contact model 1, `CONTACT_MODEL_INDEX 1`), which
links nodes of adjacent cell surfaces mechanically.

## C++ tests (CTest)

Three subdirectories are built and registered with CTest by `CMakeLists.txt`:

- `test_cell_separation/`
- `test_nucleus_in_cell_contact_via_coupling/`
- `test_cell_in_ecm_contact_via_coupling/`

They run with the rest of the suite (`ctest`); see the testing page of the developer guide
(`doc/developers/testing.md`).

## Python simulation scripts

The other subdirectories hold Python scripts that run longer simulations through the Python
bindings and compare the result with an analytical expectation (for example, the contact angle
of a cell doublet, arccos(lateral tension / apical tension)):

- `test_cell_doublet_angle/`
- `test_cell_doublet_angle_with_varying_lat_tension/`
- `test_cell_triplet_angle/`
- `test_cell_internalization/`

They are not part of CTest or CI. They load the module from `build/bin/python_bindings`, so the
build directory must be `build/` at the repository root and the bindings must be enabled. The
module runs the contact model selected at compile time, so set `CONTACT_MODEL_INDEX 1` in
`include/global_configuration.hpp` before building to test this model. From the repository
root:

```console
cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_PYTHON_BINDINGS=TRUE -DPYTHON_EXECUTABLE=$(which python3)
cmake --build build -j
cd test/test_contact_models/test_contact_node_node_via_coupling
python3 -m venv venv && . venv/bin/activate
pip install -r requirements.txt
python3 test_cell_doublet_angle/test_cell_doublet_angle.py
```

To debug the C++ code called from these scripts, see the debugging page of the developer guide
(`doc/developers/debugging.md`).
