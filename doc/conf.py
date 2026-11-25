"""Sphinx configuration for the SimuCell3D documentation.

Build locally from the ``doc/`` directory::

    pip install -r requirements.txt
    make html          # Sphinx only; the C++ API pages show a placeholder note
    make doxygen html  # with the C++ API reference (requires Doxygen)

The C++ API reference is rendered by Breathe from Doxygen XML. Breathe is
enabled only when that XML exists, either at ``doc/_build/doxygen/xml`` (the
location written by ``make doxygen`` and by the docs workflow) or at the
directory named by the ``SIMUCELL3D_DOXYGEN_XML`` environment variable.
Without it, the ``doxygen*`` directives are replaced by a short note so that
the rest of the site still builds without warnings.
"""

import os
import re
from pathlib import Path

from docutils import nodes
from docutils.parsers.rst import directives
from sphinx.util.docutils import SphinxDirective

DOC_DIR = Path(__file__).resolve().parent
REPO_ROOT = DOC_DIR.parent


# -- Project information ------------------------------------------------------


def _read_version() -> str:
    """Return the version from ``project(SimuCell3D VERSION X.Y.Z)``."""
    text = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\s*\(\s*SimuCell3D\s+VERSION\s+([0-9][0-9.]*)", text)
    return match.group(1) if match else "unknown"


project = "SimuCell3D"
# This documentation was written for the 2.x releases by their maintainer. SimuCell3D itself
# was created by Steve Runser in the Iber Lab; see doc/about.md and the NOTICE file.
author = "Nilesh Patil"
copyright = "2026, Nilesh Patil (changes since SimuCell3D 1.0; see NOTICE)"
release = _read_version()
version = ".".join(release.split(".")[:2])


# -- General configuration ----------------------------------------------------

extensions = [
    "myst_parser",
    "sphinx_design",
    "sphinx_copybutton",
    "sphinx.ext.mathjax",
]

source_suffix = {".md": "markdown"}
root_doc = "index"
templates_path = ["_templates"]

exclude_patterns = [
    "_build",
    # Sources of the archived 1.0 and 2.0 sites, built separately (legacy/).
    "legacy",
    "Thumbs.db",
    ".DS_Store",
]

myst_enable_extensions = [
    "amsmath",
    "colon_fence",
    "deflist",
    "dollarmath",
    "fieldlist",
]
myst_heading_anchors = 3
myst_dmath_double_inline = True

# Shell prompts and Python prompts are not copied by the copy button.
copybutton_prompt_text = r"\$ |>>> |\.\.\. "
copybutton_prompt_is_regexp = True

suppress_warnings = [
    # Heading levels that skip a level (for example H1 then H3) in short pages.
    "myst.header",
    # Doxygen runs without the preprocessor (see Doxyfile), so a member declared
    # identically in two compile-time branches (for example node::is_coupled for
    # CONTACT_MODEL_INDEX 1 and 2) reaches the C++ domain twice.
    "duplicate_declaration.cpp",
]


# -- HTML output --------------------------------------------------------------

# Public address of the site, without a trailing slash. Every absolute URL of
# the site (canonical links, the version switcher JSON, the workflow's
# switcher.json) is built from it. A fork can build against its own host with
# SIMUCELL3D_DOCS_BASE_URL=https://example.org/simucell3d.
DOCS_BASE_URL = os.environ.get(
    "SIMUCELL3D_DOCS_BASE_URL", "https://www.nilesh42.science/simucell3d"
).rstrip("/")
GITHUB_URL = "https://github.com/nilesh-patil/simucell3d"

# Name of the deployed version this build is for ("dev" or "X.Y"). The docs
# workflow sets it; local builds are labelled "dev".
docs_version = os.environ.get("SIMUCELL3D_DOCS_VERSION", "dev")

# Release number in CMakeLists.txt (the last release on main, between releases).
code_release = release
if docs_version == "dev":
    # The theme decides whether to show the "development version" banner from
    # the release string. "2.1.0" would compare equal to the stable "2.1" (and
    # "2.1.0+dev" too, build metadata is ignored), so no banner would appear on
    # /dev/. A string that contains "dev" and is not a version number does.
    release = "dev"

html_theme = "pydata_sphinx_theme"
html_title = f"SimuCell3D {release}"
html_short_title = "SimuCell3D"
html_static_path = ["_static"]
html_css_files = ["custom.css"]
# Break opportunities after "_", "/" and "." in code names inside tables.
html_js_files = [("table-wrap.js", {"defer": "defer"})]
html_favicon = "_static/favicon.svg"
# Canonical location of the pages. The newest release is deployed twice,
# to /X.Y/ and /stable/; the docs workflow sets SIMUCELL3D_DOCS_CANONICAL_VERSION
# to "stable" for that build so both copies point search engines at /stable/.
# Older releases and dev are canonical to themselves.
canonical_version = os.environ.get("SIMUCELL3D_DOCS_CANONICAL_VERSION", docs_version)
html_baseurl = f"{DOCS_BASE_URL}/{canonical_version}/"
html_show_sourcelink = False
html_last_updated_fmt = None
html_copy_source = False

html_theme_options = {
    "logo": {
        "text": "SimuCell3D",
        "image_light": "_static/logo-light.svg",
        "image_dark": "_static/logo-dark.svg",
        "alt_text": "SimuCell3D documentation home",
    },
    "github_url": GITHUB_URL,
    # Eight top-level sections fit on one row at 1280 px with the short titles
    # given in the root toctree (index.md); the rest go under "More".
    "header_links_before_dropdown": 8,
    "header_dropdown_text": "More",
    "navbar_align": "left",
    "navbar_end": ["version-switcher", "theme-switcher", "navbar-icon-links"],
    "show_toc_level": 2,
    "navigation_with_keys": False,
    # Source link in the right-hand sidebar: _templates/page-source-link.html.
    "secondary_sidebar_items": ["page-toc", "page-source-link"],
    "switcher": {
        # One switcher file for all deployed versions, kept at the site root by
        # the docs workflow (seeded from doc/_static/switcher.json). It must be
        # served from the same origin as the pages: the browser fetches it.
        "json_url": f"{DOCS_BASE_URL}/switcher.json",
        "version_match": docs_version,
    },
    # The switcher JSON is fetched by the browser; do not fetch it at build time.
    "check_switcher": False,
    # Banner on pages of versions other than the one marked "preferred" (stable).
    "show_version_warning_banner": True,
    "footer_start": ["copyright"],
    "footer_center": ["footer-links"],
    "footer_end": ["footer-version"],
}

html_context = {
    "default_mode": "auto",
    # Source link (_templates/page-source-link.html): the editor on main for dev
    # builds, the file at the release tag for release builds. When the pages of a
    # release are rebuilt from a later commit (a documentation-only update), set
    # SIMUCELL3D_DOCS_SOURCE_REF to that commit or branch, because pages added
    # since the tag do not exist at the tag.
    "source_ref": os.environ.get(
        "SIMUCELL3D_DOCS_SOURCE_REF",
        "main" if docs_version == "dev" else f"v{code_release}",
    ),
    "doc_path": "doc",
    "docs_version": docs_version,
    "canonical_version": canonical_version,
    "code_release": code_release,
    "docs_base_url": DOCS_BASE_URL,
    "repo_url": GITHUB_URL,
    # Preview image for link previews; the landing-page figure (index.md), as
    # copied by Sphinx into _images/. Change both together.
    "og_image": "_images/hero_tissues.png",
    "meta_description": (
        "Documentation of SimuCell3D, a C++ simulator of three-dimensional tissue "
        "mechanics at subcellular resolution: installation, user guide, examples, "
        "benchmarks and API reference."
    ),
}

# Pages without a left sidebar (single pages and landing pages).
html_sidebars = {
    "index": [],
    "install": [],
    "getting_started": [],
    "glossary": [],
    "faq": [],
    "citing": [],
    "about": [],
}


# -- C++ API reference (Breathe) ----------------------------------------------


def _doxygen_xml_dir():
    """Return the Doxygen XML directory if it exists, else ``None``."""
    candidates = []
    env_dir = os.environ.get("SIMUCELL3D_DOXYGEN_XML")
    if env_dir:
        candidates.append(Path(env_dir))
    candidates.append(DOC_DIR / "_build" / "doxygen" / "xml")
    for candidate in candidates:
        if (candidate / "index.xml").is_file():
            return candidate.resolve()
    return None


DOXYGEN_XML_DIR = _doxygen_xml_dir()
HAVE_DOXYGEN_XML = DOXYGEN_XML_DIR is not None

# The docs workflow sets SIMUCELL3D_REQUIRE_DOXYGEN=1 so that a missing XML
# directory fails the build instead of publishing placeholder API pages.
if os.environ.get("SIMUCELL3D_REQUIRE_DOXYGEN") == "1" and not HAVE_DOXYGEN_XML:
    raise RuntimeError(
        "SIMUCELL3D_REQUIRE_DOXYGEN=1 but no Doxygen XML was found; run 'make doxygen' first."
    )

BREATHE_DIRECTIVES = (
    "doxygenclass",
    "doxygenstruct",
    "doxygenfunction",
    "doxygengroup",
    "doxygenfile",
    "doxygendefine",
    "doxygenenum",
    "doxygentypedef",
    "doxygenvariable",
    "doxygenindex",
    "doxygennamespace",
)

if HAVE_DOXYGEN_XML:
    extensions.append("breathe")
    breathe_projects = {"simucell3d": str(DOXYGEN_XML_DIR)}
    breathe_default_project = "simucell3d"
    breathe_default_members = ("members",)
    breathe_domain_by_extension = {"hpp": "cpp", "h": "cpp", "dox": "cpp"}
    breathe_show_include = True
    breathe_show_define_initializer = True
    breathe_show_enumvalue_initializer = True


class _AnyOption(dict):
    """Option specification that accepts every option name as a string."""

    def __missing__(self, key):
        return directives.unchanged

    def __contains__(self, key):
        return True


class DoxygenPlaceholder(SphinxDirective):
    """Stand-in for the Breathe directives when no Doxygen XML is available."""

    has_content = True
    optional_arguments = 1
    final_argument_whitespace = True
    option_spec = _AnyOption()

    def run(self):
        # One note per page is enough; later directives on the page render nothing.
        if self.env.temp_data.get("simucell3d_doxygen_note"):
            return []
        self.env.temp_data["simucell3d_doxygen_note"] = True
        para = nodes.paragraph()
        para += nodes.Text(
            "The C++ reference on this page is built from the Doxygen comments in "
            "the header files and is included in the published documentation. To "
            "build it locally, install Doxygen and run "
        )
        para += nodes.literal(text="make doxygen html")
        para += nodes.Text(" in the doc/ directory.")
        note = nodes.note()
        note += para
        return [note]


def setup(app):
    if not HAVE_DOXYGEN_XML:
        for name in BREATHE_DIRECTIVES:
            app.add_directive(name, DoxygenPlaceholder)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
