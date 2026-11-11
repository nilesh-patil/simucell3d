(dev-writing-docs)=
# Writing documentation

The documentation is a Sphinx site in `doc/`, written in Markdown (MyST) with the PyData theme.
The C++ API reference is generated from Doxygen comments in `include/` and rendered by Breathe.
This page explains how to build the site, where each kind of content goes, and the writing
conventions.

## Building the documentation

The Python packages are pinned in `doc/requirements.txt` (Sphinx, pydata-sphinx-theme,
myst-parser, Breathe, sphinx-design, sphinx-copybutton). Install them in a virtual environment
and build from `doc/`:

```console
$ python3 -m venv .venv-doc
$ . .venv-doc/bin/activate
$ python -m pip install -r doc/requirements.txt
$ cd doc
$ make html
```

The site is written to `doc/_build/html/`; open `doc/_build/html/index.html` in a browser.

Targets of `doc/Makefile`:

| Target | Effect |
|---|---|
| `make html` | Build the site. |
| `make html-strict` | Build with warnings treated as errors (`-W --keep-going`), as CI does. |
| `make doxygen` | Run Doxygen with `doc/Doxyfile`, writing XML to `doc/_build/doxygen/xml/`. Requires the `doxygen` program. |
| `make doxygen html` | Both, in that order: the site with the C++ API reference. |
| `make clean` | Remove `doc/_build/`. |

Without Doxygen XML, the C++ API pages still build: `doc/conf.py` replaces the Breathe
directives by a note (once per page) saying that the reference is built in the published
documentation. With `SIMUCELL3D_REQUIRE_DOXYGEN=1`, as set in CI, a missing XML directory is an
error instead, so the published site never shows the placeholder. To use
Doxygen XML from another location, set `SIMUCELL3D_DOXYGEN_XML` to the directory that contains
`index.xml`.

The version shown on the site is read from `project(SimuCell3D VERSION X.Y.Z)` in the top-level
`CMakeLists.txt`; it is never typed into `conf.py`. The environment variable
`SIMUCELL3D_DOCS_VERSION` (`dev` by default) tells the version switcher which deployed version
the build is for; the docs workflow sets it ({ref}`dev-ci`).

Before opening a pull request that touches `doc/` or the Doxygen comments, run
`make html-strict` and fix every warning. Broken cross-references, unknown glossary terms and
missing toctree entries are warnings.

## Where content goes

The site follows the structure of the scikit-learn documentation:

| Directory or page | Content | Style |
|---|---|---|
| `install.md`, `getting_started.md` | Building and a first run. | tutorial |
| `user_guide/` | One numbered chapter per topic: the model, its parameters, inputs, outputs, tools. | explanation and reference |
| `examples/` | One page per shipped parameter file: command, expected output, what to look at. | how-to |
| `api/` | C++ classes by Doxygen group; the Python module. | reference |
| `developers/` | This guide. | how-to and explanation |
| `whats_new/` | Release notes in prose, one page per release. | |
| `glossary.md` | Every term used with `` {term} ``. | reference |

`CHANGELOG.md` at the repository root is the canonical list of changes; the `whats_new/` pages
summarise it for users. Images go in `doc/images/` with lowercase, underscore-separated names.

## Writing conventions

- Neutral, present tense; third person for descriptions ("The solver removes ...") and the
  imperative for instructions ("Run `ctest`."). No marketing words, no emojis, no exclamation
  marks, no plans or promises; planned work belongs in GitHub issues.
- Every flag, XML tag, path, class, CSV column and number must exist in the code of the release
  the page documents. Check it against the source before writing it down.
- State SI units for every physical quantity. The parameter files write units as dimensions
  (M, L, T); the parameter-file chapter of the user guide gives the SI unit for each tag.
- Performance numbers appear only with their conditions (machine, cores, cell count,
  configuration) and only if they are recorded in `CHANGELOG.md` or the paper.
- Attribute results of the paper to the paper (Runser, Vetter & Iber, 2024) and link to the
  {doc}`/citing` page; do not present them as properties of the current release.
- Use the canonical spelling of each term (`simucell3d` for the executable, "parameter file",
  "face type", "contact model"), as defined in the {doc}`/glossary`.

### MyST syntax

Every page starts with a label above its title; prefixes are `install-`, `gs-`, `ug-`, `ex-`,
`api-`, `dev-`, `whatsnew-` and `glossary-`:

````markdown
(dev-testing)=
# Testing

Link to a section with {ref}`dev-testing`, to a page with {doc}`/user_guide/03_contact`,
to a glossary entry with {term}`USPG`.

Inline math $V_0$; display math with a label:

$$
p = -K \ln(V/V_0)
$$ (eq-pressure)

:::{note}
Admonitions use colon fences: note, important, warning, tip, seealso.
:::

```console
$ ./simucell3d ../parameters/core/parameters_vesicle_quick.xml --output-dir=dev_run
```
````

Code blocks carry a language: `console` for shell sessions with a `$` prompt (the copy button
strips the prompt), `bash` for scripts, `xml`, `cpp`, `python`, `text` for program output. State
the directory a command runs from. XML snippets are copied from a real file in `parameters/` and
name it in a caption.

Figures use the `figure` directive with alt text and a caption:

````markdown
:::{figure} /images/cell_mesh.png
:alt: A cell mesh coloured by cell id.

Caption sentence.
:::
````

## Documenting the C++ API

The API pages in `doc/api/` are maintained by hand: one page per Doxygen group, listing its
classes explicitly with Breathe directives (`doxygenclass` and `doxygenstruct` with
`:members:`, and `doxygendefine`, `doxygenenum`, `doxygenfile` or `doxygennamespace` where
needed). The groups are defined once in `doc/api/groups.dox`, and each header joins one with
`@ingroup`. Doxygen runs with preprocessing disabled, so members in every compile-time branch
(`#if CONTACT_MODEL_INDEX ...`) are documented. Document public classes and member functions in
the header, before the declaration:

```cpp
/**
 * @file local_mesh_refiner.hpp
 * @ingroup triangulation
 */

/**
 * Keeps every edge length of a cell mesh between a minimum and a maximum.
 */
class local_mesh_refiner { ... };

/**
 * Refine the meshes of all cells, in parallel.
 *
 * A cell whose refinement throws a std::exception is left unchanged and reported.
 *
 * @param cell_lst the cells of the simulation
 * @return the cells whose refinement failed, with the reason
 * @throws unstable_simulation_exception rethrown after the parallel loop
 * @note Call outside any parallel region; the method parallelises over cells itself.
 */
```

The first sentence is the brief description. Give units in `@param` for physical quantities.
Comment-only changes to headers do not change behaviour, but they trigger a full rebuild; keep
them in separate commits from code changes.

To add a class to the reference, tag its header with `@file` and `@ingroup <group>` and add a
`doxygenclass` block to the matching `doc/api/<group>.md`. To add a group, define it in
`groups.dox`, add a page under `doc/api/` and list it in `doc/api/index.md`.

## Adding a page

1. Create the `.md` file in the right directory, with a label and one H1.
2. Add it to the `toctree` of that directory's `index.md` (or of `doc/index.md` for a top-level
   page).
3. Build with `make html-strict`.

Pages for the user guide follow the template: summary, concept, model, parameters,
compile-time options, practical guidance, see also. Example pages follow: goal, files,
compile-time configuration, run command, what to look at, variations.
