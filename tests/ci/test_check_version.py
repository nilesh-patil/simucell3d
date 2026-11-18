"""Tests for scripts/ci/check_version.py and scripts/ci/changelog_section.py."""
import importlib.util
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent


def _load(name):
    # Load by path: tests/ci is itself a package named "ci", so "import ci.x" is ambiguous.
    spec = importlib.util.spec_from_file_location(name, REPO / "scripts" / "ci" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


check_version = _load("check_version")
changelog_section = _load("changelog_section")

CHANGELOG = """# Changelog

## [{top}] - {date}

### Fixed
- Something.

## [2.0.0] - 2026-03-02

- Older.

[{top}]: https://github.com/o/r/compare/v2.0...{end}
[2.0.0]: https://github.com/o/r/releases/tag/v2.0
"""


def make_repo(tmp_path, cmake="2.1.0", top="2.1.0", date="unreleased", end="HEAD",
              cff_version="2.1.0", cff_date=None):
    (tmp_path / "include").mkdir()
    (tmp_path / "include" / "version.hpp.in").write_text('#define SIMUCELL3D_VERSION "@PROJECT_VERSION@"\n')
    (tmp_path / "CMakeLists.txt").write_text("project(SimuCell3D VERSION {})\n".format(cmake))
    (tmp_path / "CHANGELOG.md").write_text(CHANGELOG.format(top=top, date=date, end=end))
    if cff_version is not None:
        cff = "cff-version: 1.2.0\nversion: {}\n".format(cff_version)
        if cff_date:
            cff += "date-released: '{}'\n".format(cff_date)
        (tmp_path / "CITATION.cff").write_text(cff)
    return tmp_path


def test_branch_mode_consistent(tmp_path):
    assert check_version.check(make_repo(tmp_path)) == []


def test_cmake_changelog_mismatch(tmp_path):
    errors = check_version.check(make_repo(tmp_path, cmake="2.2.0"))
    assert any("top heading is [2.1.0]" in e for e in errors)


def test_citation_may_lag_at_previous_release(tmp_path):
    assert check_version.check(make_repo(tmp_path, cff_version="2.0.0")) == []


def test_citation_wrong_version(tmp_path):
    errors = check_version.check(make_repo(tmp_path, cff_version="1.9.0"))
    assert any("CITATION.cff: version 1.9.0" in e for e in errors)


def test_tag_mode_rejects_unreleased(tmp_path):
    errors = check_version.check(make_repo(tmp_path), tag="v2.1.0")
    assert any("needs a YYYY-MM-DD date" in e for e in errors)
    assert any("ends at HEAD" in e for e in errors)


def test_tag_mode_release_candidate_allows_unreleased(tmp_path):
    assert check_version.check(make_repo(tmp_path), tag="v2.1.0-rc1") == []


def test_tag_mode_release_ok(tmp_path):
    repo = make_repo(tmp_path, date="2026-12-01", end="v2.1.0", cff_date="2026-12-01")
    assert check_version.check(repo, tag="v2.1.0") == []


def test_tag_mode_wrong_tag(tmp_path):
    repo = make_repo(tmp_path, date="2026-12-01", end="v2.1.0", cff_date="2026-12-01")
    errors = check_version.check(repo, tag="v2.1.1")
    assert any("does not match CMakeLists.txt" in e for e in errors)


def test_bad_date_and_missing_link(tmp_path):
    repo = make_repo(tmp_path, date="soon")
    text = (repo / "CHANGELOG.md").read_text().replace("[2.0.0]: https://github.com/o/r/releases/tag/v2.0\n", "")
    (repo / "CHANGELOG.md").write_text(text)
    errors = check_version.check(repo)
    assert any("has date 'soon'" in e for e in errors)
    assert any("no link reference '[2.0.0]" in e for e in errors)


def test_changelog_section_extract():
    text = CHANGELOG.format(top="2.1.0", date="unreleased", end="HEAD")
    assert changelog_section.extract(text, "2.1.0") == "### Fixed\n- Something."
    assert changelog_section.extract(text, "2.0.0") == "- Older."
    assert changelog_section.extract(text, "9.9.9") is None


def test_real_repository_is_consistent():
    assert check_version.check(REPO) == []
