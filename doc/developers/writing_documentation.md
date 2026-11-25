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
`CMakeLists.txt`; it is never typed into `conf.py`. Two environment variables describe the
deployment; the docs workflow sets both ({ref}`dev-ci`):

| Variable | Default | Effect |
|---|---|---|
| `SIMUCELL3D_DOCS_VERSION` | `dev` | The deployed version the build is for (`dev` or `X.Y`). It selects the entry of the version switcher, and a `dev` build shows the development-version banner. |
| `SIMUCELL3D_DOCS_BASE_URL` | `https://www.nilesh42.science/simucell3d` | The public address of the site. Canonical links and the address of `switcher.json` are built from it. |

When several builds run at once, give each its own output and doctree directories instead of
`make html`:

```console
$ cd doc
$ sphinx-build -b html -d /path/to/doctrees . /path/to/html
```

Before opening a pull request that touches `doc/` or the Doxygen comments, run
`make html-strict` and fix every warning. Broken cross-references, unknown glossary terms and
missing toctree entries are warnings.

## Versions and the published site

The site is published at <https://www.nilesh42.science/simucell3d/>. Each version has its own
directory:

| Path | Content | Built from |
|---|---|---|
| `/dev/` | the development version | every push to `main` |
| `/X.Y/` | release X.Y, for example `/2.1/` | the tag `vX.Y.Z` |
| `/stable/` | a copy of the newest release | the newest tag |
| `/2.0/`, `/1.0/` | archived documentation of 2.0 and 1.0 | the tags, with `doc/legacy/` (below), deployed by hand |

The site root redirects to `/stable/`. The version menu in the header offers dev, 2.1 (stable),
2.0 and 1.0. It reads `switcher.json` at the site root, which the docs workflow writes at every
deployment. The workflow starts from the seed `doc/_static/switcher.json`, which lists the
archived 2.0 and 1.0 sites, adds the entries already deployed, and keeps a release only if its
directory exists on the `gh-pages` branch or is being deployed, so the menu never links to a
missing site. The newest release is marked as preferred; pages of every other version show a
banner that links to it. `switcher.json` must be served from the same address as the pages,
because the browser fetches it.

### Archived documentation of 1.0 and 2.0

Releases 1.0 and 2.0 had no Sphinx site. Their Markdown documentation is rendered from the
release tags by `doc/legacy/build_legacy.py` and published at `/1.0/` and `/2.0/`. The text of
each page is kept as written at the tag; the tool changes only links and image paths that would
not work on the website, and lists every change on the landing page of the site. The landing
pages (`doc/legacy/v1.0/index.md.in`, `doc/legacy/v2.0/index.md.in`) and the licence page
(`doc/legacy/shared/licence.md.in`) are the only hand-written pages.

The sites take their theme, CSS, templates and version switcher from `doc/conf.py` at build time
(`doc/legacy/legacy_conf.py`), so rebuild them after a design change. They need the git tags
`v1.0` and `v2.0` in the clone (`git fetch --tags`). From the repository root:

```console
$ python doc/legacy/build_legacy.py 1.0 --out _site/1.0
$ python doc/legacy/build_legacy.py 2.0 --out _site/2.0
```

Both builds must finish without warnings; add `-- -W --keep-going` to make warnings errors.
Use `--keep-src DIR` to inspect the generated Markdown.

The docs workflow does not build these sites. To deploy, copy the contents of `_site/1.0/` and
`_site/2.0/` to the directories `1.0/` and `2.0/` of the `gh-pages` branch, replacing what is
there. The root `switcher.json` must contain entries with `"version": "1.0"` and
`"version": "2.0"`. The archived pages carry `<meta name="robots" content="noindex">`, so search
engines list only the current documentation.

## Where content goes

The site follows the structure of the scikit-learn documentation:

| Directory or page | Content | Style |
|---|---|---|
| `install.md`, `getting_started.md` | Building and a first run. | tutorial |
| `user_guide/` | One numbered chapter per topic: the model, its parameters, inputs, outputs, tools. | explanation and reference |
| `examples/` | One page per shipped parameter file: command, expected output, what to look at. | how-to |
| `benchmarks/` | The November 2026 cross-version benchmark: method, completion, performance, time step, validity, reproducibility. The data behind each figure is in `benchmarks/data/`. | reference |
| `api/` | C++ classes by Doxygen group; the Python module. | reference |
| `developers/` | This guide. | how-to and explanation |
| `whats_new/` | Release notes in prose, one page per release. | |
| `glossary.md` | Every term used with `` {term} ``. | reference |

`CHANGELOG.md` at the repository root is the canonical list of changes; the `whats_new/` pages
summarise it for users. Images go in `doc/images/` with lowercase, underscore-separated names;
benchmark figures go in `doc/benchmarks/figures/`.

Benchmark numbers live only in `doc/benchmarks/` ({ref}`dev-writing-docs-house-style`).

## Writing conventions

- Neutral, present tense; third person for descriptions ("The solver removes ...") and the
  imperative for instructions ("Run `ctest`."). No marketing words, no emojis, no exclamation
  marks, no plans or promises; planned work belongs in GitHub issues.
- Every flag, XML tag, path, class, CSV column and number must exist in the code of the release
  the page documents. Check it against the source before writing it down.
- State SI units for every physical quantity. The parameter files write units as dimensions
  (M, L, T); the parameter-file chapter of the user guide gives the SI unit for each tag.
- Performance numbers appear only with their conditions (machine, cores, cell count,
  configuration, and which growth configuration: the lab's benchmark file or the reconstructed
  `parameters/longrun_fast_growth.xml`), and only if they are recorded on a benchmarks page, in
  `CHANGELOG.md` or in the paper. A measurement is not a guarantee; say "measured".
- Attribute results of the paper to the paper (Runser, Vetter & Iber, 2024) and link to the
  {doc}`/citing` page; do not present them as properties of the current release.
- Use the canonical spelling of each term (`simucell3d` for the executable, "parameter file",
  "face type", "contact model"), as defined in the {doc}`/glossary`.

### MyST syntax

Every page starts with a label above its title. Top-level pages and section landing pages use a
single name (`index`, `install`, `gs`, `glossary`, `faq`, `citing`, `about`, `examples`,
`benchmarks`, `whatsnew`); the pages inside a section use the prefixes `ug-`, `ex-`,
`benchmarks-`, `api-`, `dev-` and `whatsnew-`. Labels of sections inside a page start with the
page's label (for example `install-source`):

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

Figures use the `figure` directive with alt text and a caption. The caption says what the
figure shows and what to take from it:

````markdown
:::{figure} /images/cell_mesh.png
:alt: A cell mesh coloured by cell id.

Caption sentence.
:::
````

The site has a light and a dark theme, and every figure must be readable in both. Two ways
work:

- **Two variants.** Draw the figure twice, with the colours of each theme, and give each
  variant the theme's class; the page shows the one that matches. The benchmark charts are made
  this way by `doc/benchmarks/make_figures.py`, which writes `<name>-light.svg` and
  `<name>-dark.svg` from the tables in `doc/benchmarks/data/`:

  ````markdown
  ```{figure} figures/removals_dt-light.svg
  :figclass: only-light
  :class: only-light
  :alt: ...

  Caption.
  ```

  ```{figure} figures/removals_dt-dark.svg
  :figclass: only-dark
  :class: only-dark
  :alt: ...

  Caption.
  ```
  ````

- **One SVG that follows the text colour.** Draw lines and text with `currentColor` (and fills
  that read on both backgrounds), and embed the SVG inline so that it inherits the page colour.

Put the class on the image (`:class:`) as well as on the figure (`:figclass:`). In dark mode
the theme puts a light plate and a brightness filter on every image that lacks `only-dark` or
`dark-light` on the image itself. A single transparent render that suits both themes gets
`:class: dark-light`; an opaque screenshot gets no class and keeps the theme's plate. The colour
and figure rules are under {ref}`dev-writing-docs-house-style`.

(dev-writing-docs-house-style)=
## House style

These defaults apply to every page and figure. They keep the site consistent, so that a version
always has the same colour and a number always has one home.

### Wording

- Versions are written "1.0", "1.0 with two crash fixes", "2.0", "2.1.0" (the release) or "2.1"
  (the documentation series). Git tags such as `v2.1.0` appear only in code font; build labels
  from benchmark scripts (B0 to B7) never appear.
- Time steps are written in code font with their unit: `2.5e-8` s. Cell and iteration counts
  use thousands separators: 1,736 iterations, 144,919 cells.
- Dates are written as month and year: November 2026.
- A measured value is stated as measured, with its conditions, not as a guarantee. For example,
  the recommended time step for the growth benchmark file is `2.5e-8` s because it was clean in
  three of three 48-hour runs with 2.1.0 ({ref}`benchmarks-time-step`).

### Where numbers live

Every benchmark number has one canonical place: a page in `doc/benchmarks/`, built from the
tables in `doc/benchmarks/data/`. Other pages give the practical consequence in one line and
link to that page with its label; they do not repeat its tables.

### Colour

The site chrome uses two colours, set through the PyData variables `--pst-color-primary` and
`--pst-color-secondary` (and their `-bg` and `-highlight` companions) for each theme in
`doc/_static/custom.css`. Admonitions keep the theme's colours.

| Role | Light | Dark |
|---|---|---|
| Primary: links, active navigation, focus ring | `#0b6b7c` | `#62c7d8` |
| Secondary: link hover, active underline, card hover border | `#a8432b` | `#f09a7a` |

Charts use one palette, so that a version has the same colour in every figure:

| Series | Light | Dark |
|---|---|---|
| 2.1.0 | `#0b6b7c` | `#62c7d8` |
| 2.0 | `#6f4bb8` | `#b79cf0` |
| 1.0 with two crash fixes | `#9a6700` | `#e3b341` |
| 1.0 | `#6b7280` | `#9aa3ad` |
| Intermediate builds | light grey, unlabelled unless the text discusses them | light grey |
| Emphasis, failure marker | `#a8432b` | `#f09a7a` |

Axes, ticks and text use the theme's text colour (`#222832` light, `#ced6dd` dark), and figure
backgrounds are transparent. Renders of tissues keep their face-type colours (apical orange,
lateral blue, ECM green, lumen ochre); do not recolour them to the site palette.

### Figures

- Every figure has alt text and a caption that states its takeaway in one sentence.
- Charts are SVG, in a light and a dark variant shown with the classes `only-light` and
  `only-dark`, set on both the figure and the image (see the example above). A transparent render
  that suits both themes gets `:class: dark-light`; an opaque screenshot gets no class. Label series directly at the line end or bar; use a legend box only for
  more than four series. Never encode by colour alone: 1.0 and 1.0 with crash fixes also differ
  in marker or dash. No top or right spines, and no gridlines except faint horizontal ones on
  log axes.
- Raster renders are saved at twice their display width, on a transparent or neutral
  background, with a scale bar where size matters and no title baked into the image.

### Navigation

The top bar shows, in this order: Install, Get started, User guide, Examples, Benchmarks, API,
Developers, Releases; Glossary, FAQ, Citing and About are under "More". The labels are set in the
root toctree of `doc/index.md`; the pages themselves are not renamed, so their addresses do not
change. To the right of the bar there are only the search, the version switcher, the theme
toggle and the GitHub icon. Section pages have the section navigation on the left; single pages
have no left sidebar. The right sidebar shows "On this page" with two levels.

## Documenting the C++ API

The API pages in `doc/api/` are maintained by hand: one page per Doxygen group (the group
`core` is `solver.md`), plus `python.md` for the Python module, whose two C++ wrapper classes
are in `core`. Each page lists its entities explicitly with Breathe directives: `doxygenclass`
and `doxygenstruct` with `:members:`, and `doxygenenum`, `doxygenvariable`, `doxygenfile` or
`doxygennamespace` where needed. The compile-time macros are a hand-written table in
`solver.md`. The groups are defined once in `doc/api/groups.dox`, and each header joins one with
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
