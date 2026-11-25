"""Sphinx settings for an archived (legacy) documentation build.

``build_legacy.py`` writes a ``conf.py`` into the generated source tree that
runs this file with ``LEGACY_VERSION``, ``LEGACY_TAG`` and ``LEGACY_COMMIT``
set. The look of the site (theme, options, CSS, JavaScript, templates, logo,
extensions) is taken from ``doc/conf.py`` at build time, so these sites follow
the current design without a copy of it. Only what describes the release, and
what must differ for an archived page, is set here.
"""

import copy
import os
import runpy
import sys
from pathlib import Path

LEGACY_DIR = Path(__file__).resolve().parent
DOC_DIR = LEGACY_DIR.parent

# Names injected by the generated conf.py (defaults allow a direct import for testing).
LEGACY_VERSION = globals().get("LEGACY_VERSION", "1.0")
LEGACY_TAG = globals().get("LEGACY_TAG", "v" + LEGACY_VERSION)
LEGACY_COMMIT = globals().get("LEGACY_COMMIT", "")
# Pages that come from the tag (docnames); the landing, section and licence pages do not.
LEGACY_PAGES = list(globals().get("LEGACY_PAGES", []))

# -- Import the main configuration ---------------------------------------------------

os.environ["SIMUCELL3D_DOCS_VERSION"] = LEGACY_VERSION
os.environ.pop("SIMUCELL3D_REQUIRE_DOXYGEN", None)
if str(DOC_DIR) not in sys.path:
    sys.path.insert(0, str(DOC_DIR))
_main = runpy.run_path(str(DOC_DIR / "conf.py"))

SITE = str(_main.get("DOCS_BASE_URL", "https://www.nilesh42.science/simucell3d")).rstrip("/") + "/"
CURRENT_DOCS = SITE + "stable/"


def _abs(paths):
    """Make paths of the main configuration absolute (they are relative to doc/)."""
    out = []
    for p in paths or []:
        p = Path(p)
        out.append(str(p if p.is_absolute() else DOC_DIR / p))
    return out


def _abs_file(value):
    if not value or "://" in str(value) or Path(value).is_absolute():
        return value
    return str(DOC_DIR / value)


# Everything that decides how a page looks is inherited.
_INHERIT = (
    "html_theme",
    "html_theme_path",
    "html_css_files",
    "html_js_files",
    "html_context",
    "html_permalinks_icon",
    "pygments_style",
    "pygments_dark_style",
    "myst_enable_extensions",
    "myst_heading_anchors",
    "myst_dmath_double_inline",
    "copybutton_prompt_text",
    "copybutton_prompt_is_regexp",
    "suppress_warnings",
    "source_suffix",
    "html_copy_source",
)
for _name in _INHERIT:
    if _name in _main:
        globals()[_name] = copy.deepcopy(_main[_name])

# Extensions: everything except the C++ API reference, which these sites do not have.
extensions = [e for e in _main.get("extensions", []) if e != "breathe"]

html_static_path = _abs(_main.get("html_static_path"))
# Legacy templates first (footer), then the main templates (theme overrides).
templates_path = [str(LEGACY_DIR / "_templates")] + _abs(_main.get("templates_path"))
html_logo = _abs_file(_main.get("html_logo"))
html_favicon = _abs_file(_main.get("html_favicon"))

# -- Release description -------------------------------------------------------------

project = "SimuCell3D"
release = LEGACY_VERSION
version = LEGACY_VERSION
html_title = f"SimuCell3D {LEGACY_VERSION} documentation (archived)"
html_short_title = f"SimuCell3D {LEGACY_VERSION}"
if LEGACY_VERSION == "1.0":
    author = "Steve Runser, Roman Vetter, Dagmar Iber"
    copyright = "Steve Runser, Roman Vetter, Dagmar Iber (Iber Lab, ETH Zurich)"
else:
    author = "Nilesh Patil"
    copyright = "2026, Nilesh Patil (changes since SimuCell3D 1.0; see NOTICE)"

root_doc = "index"
exclude_patterns = ["_build", "_legacy_extra", "Thumbs.db", ".DS_Store"]
html_extra_path = ["_legacy_extra"]

html_baseurl = f"{SITE}{LEGACY_VERSION}/"
html_show_sourcelink = False
html_last_updated_fmt = None
html_sidebars = {"index": [], "licence": []}

html_theme_options = copy.deepcopy(_main.get("html_theme_options", {}))
_switcher = dict(html_theme_options.get("switcher") or {})
_switcher["version_match"] = LEGACY_VERSION
html_theme_options["switcher"] = _switcher
html_theme_options["check_switcher"] = False
html_theme_options["show_version_warning_banner"] = True
html_theme_options["footer_start"] = ["legacy-footer"]
# The main footer links (citing, about) point to pages these sites do not have.
html_theme_options["footer_center"] = []
# Link to the old page at the tag, instead of the main site's source link.
html_theme_options["secondary_sidebar_items"] = ["page-toc", "legacy-source-link"]
# A short bar of the site's own sections, ending with the link to the current
# documentation; the rest of the 2.0 sections go under "More". Keep equal to
# NAV_VISIBLE in build_legacy.py.
html_theme_options["header_links_before_dropdown"] = 7

html_context = dict(globals().get("html_context", {}))
html_context.update(
    {
        "legacy_version": LEGACY_VERSION,
        "legacy_tag": LEGACY_TAG,
        "legacy_commit": LEGACY_COMMIT,
        "current_docs": CURRENT_DOCS,
        "legacy_pages": LEGACY_PAGES,
        # Keys read by the main templates (doc/_templates).
        "docs_version": LEGACY_VERSION,
        "canonical_version": LEGACY_VERSION,
        "code_release": LEGACY_VERSION,
        "source_ref": LEGACY_TAG,
        "docs_base_url": SITE.rstrip("/"),
        "og_image": {
            "1.0": "doc/img/simucell_topolgy_example.png",
            "2.0": "doc/assets/img/simucell_topolgy_example.png",
        }.get(LEGACY_VERSION, ""),
        "meta_description": (
            f"Archived documentation of SimuCell3D {LEGACY_VERSION} (tag {LEGACY_TAG}), "
            "re-rendered unchanged. See the current documentation for the latest release."
        ),
    }
)

# One code block of the 2.0 pages (doc/user-guide/visualization.md, a notebook
# cell marked "python" that starts a line with "!") cannot be lexed as Python.
# Sphinx then shows it with relaxed highlighting; the warning says only that.
suppress_warnings = list(globals().get("suppress_warnings", [])) + ["misc.highlighting_failure"]


# -- Keep search engines on the current documentation --------------------------------

ROBOTS_META = '<meta name="robots" content="noindex, follow" />\n'


def _noindex(app, pagename, templatename, context, doctree):
    context["metatags"] = context.get("metatags", "") + ROBOTS_META


def setup(app):
    main_setup = _main.get("setup")
    result = main_setup(app) if callable(main_setup) else None
    app.connect("html-page-context", _noindex)
    # Code blocks of the old pages marked "csv" (no Pygments lexer of that
    # name in the pinned version): show them as plain text, as a code host does.
    from pygments.lexers.special import TextLexer

    app.add_lexer("csv", TextLexer)
    return result or {"parallel_read_safe": True, "parallel_write_safe": True}
