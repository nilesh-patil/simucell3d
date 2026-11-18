#!/usr/bin/env python3
"""Check that the version number is consistent across the repository.

The single source of truth is ``project(SimuCell3D VERSION X.Y.Z)`` in the top-level
``CMakeLists.txt``. This script checks that the other places that state a version agree
with it:

* ``CHANGELOG.md``: the top ``## [X.Y.Z] - <date>`` heading names the CMake version, its
  date is ``unreleased`` or ``YYYY-MM-DD``, headings are in descending order, and every
  heading has a link reference (``[X.Y.Z]: https://...``) at the bottom of the file.
* ``CITATION.cff`` (if present): ``version`` is the CMake version or the newest released
  version, and ``date-released`` (if present) matches the CHANGELOG date of that version.
* ``include/version.hpp.in`` takes the version from CMake (no hard-coded number).

With ``--tag vX.Y.Z`` (release mode) it also checks that the tag names the CMake version,
that the top CHANGELOG heading is dated (a ``-rcN`` tag may still be ``unreleased``), that
its compare link ends at the tag rather than at ``HEAD``, and that ``CITATION.cff`` states
the released version and its date.

Exit status 0 when all checks pass, 1 otherwise. Standard library only.

Usage::

    python3 scripts/ci/check_version.py                  # branch mode (every PR)
    python3 scripts/ci/check_version.py --tag v2.1.0     # release mode (tag push)
    python3 scripts/ci/check_version.py --print-version  # print X.Y.Z and exit
"""

import argparse
import re
import sys
from pathlib import Path

SEMVER = r"(\d+)\.(\d+)\.(\d+)"
CMAKE_PROJECT_RE = re.compile(r"^\s*project\s*\(\s*SimuCell3D\s+VERSION\s+" + SEMVER + r"\s*\)",
                              re.MULTILINE | re.IGNORECASE)
CHANGELOG_HEADING_RE = re.compile(r"^## \[([^\]]+)\](?:\s+-\s+(.*?))?\s*$", re.MULTILINE)
CHANGELOG_LINK_RE = re.compile(r"^\[([^\]]+)\]:\s*(\S+)\s*$", re.MULTILINE)
DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
TAG_RE = re.compile(r"^v" + SEMVER + r"(-rc\d+)?$")


def version_key(version):
    return tuple(int(part) for part in version.split("."))


def read_cmake_version(root):
    """Return the X.Y.Z string from project(SimuCell3D VERSION X.Y.Z)."""
    text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    matches = CMAKE_PROJECT_RE.findall(text)
    if len(matches) != 1:
        raise ValueError("CMakeLists.txt: expected exactly one 'project(SimuCell3D VERSION X.Y.Z)', "
                         "found {}".format(len(matches)))
    return ".".join(matches[0])


def read_changelog(root):
    """Return ([(version, date), ...] in file order, {version: link})."""
    text = (root / "CHANGELOG.md").read_text(encoding="utf-8")
    headings = [(m.group(1), (m.group(2) or "").strip()) for m in CHANGELOG_HEADING_RE.finditer(text)]
    links = {m.group(1): m.group(2) for m in CHANGELOG_LINK_RE.finditer(text)}
    return headings, links


def read_citation(root):
    """Return {'version': ..., 'date-released': ...} from CITATION.cff, or None if absent.

    Only the two top-level scalar keys are needed, so a line-based parse is enough and
    avoids a YAML dependency.
    """
    path = root / "CITATION.cff"
    if not path.exists():
        return None
    fields = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^(version|date-released):\s*['\"]?([^'\"#\s]+)['\"]?\s*(#.*)?$", line)
        if m:
            fields[m.group(1)] = m.group(2)
    return fields


def check(root, tag=None):
    """Run all checks; return a list of error strings (empty when consistent)."""
    errors = []

    try:
        cmake_version = read_cmake_version(root)
    except (OSError, ValueError) as exc:
        return [str(exc)]

    # include/version.hpp.in must take the number from CMake.
    header = root / "include" / "version.hpp.in"
    if header.exists():
        if "@PROJECT_VERSION@" not in header.read_text(encoding="utf-8"):
            errors.append("include/version.hpp.in: SIMUCELL3D_VERSION must be '@PROJECT_VERSION@'")
    else:
        errors.append("include/version.hpp.in: file not found")

    # CHANGELOG.md
    try:
        headings, links = read_changelog(root)
    except OSError as exc:
        return errors + [str(exc)]
    released = [(v, d) for v, d in headings if re.fullmatch(SEMVER, v)]
    if not released:
        return errors + ["CHANGELOG.md: no '## [X.Y.Z] - <date>' heading found"]

    top_version, top_date = released[0]
    if headings[0][0] != top_version:
        errors.append("CHANGELOG.md: the first heading '## [{}]' is not a version; use "
                      "'## [{}] - unreleased' for unreleased changes".format(headings[0][0], cmake_version))
    if top_version != cmake_version:
        errors.append("CHANGELOG.md: top heading is [{}] but CMakeLists.txt says {}".format(
            top_version, cmake_version))

    for version, date in released:
        if date.lower() != "unreleased" and not DATE_RE.match(date):
            errors.append("CHANGELOG.md: heading [{}] has date '{}'; expected YYYY-MM-DD or "
                          "'unreleased'".format(version, date))
        if version not in links:
            errors.append("CHANGELOG.md: no link reference '[{0}]: <url>' for heading [{0}]".format(version))
    for version, date in released[1:]:
        if date.lower() == "unreleased":
            errors.append("CHANGELOG.md: only the top heading may be 'unreleased', not [{}]".format(version))
    keys = [version_key(v) for v, _ in released]
    if keys != sorted(keys, reverse=True) or len(set(keys)) != len(keys):
        errors.append("CHANGELOG.md: version headings are not unique and in descending order")

    dated = [(v, d) for v, d in released if DATE_RE.match(d)]
    newest_dated = dated[0] if dated else (None, None)
    changelog_dates = dict(released)

    # CITATION.cff
    citation = read_citation(root)
    if citation is not None:
        cff_version = citation.get("version")
        cff_date = citation.get("date-released")
        allowed = {cmake_version, newest_dated[0]} - {None}
        if cff_version is None:
            errors.append("CITATION.cff: no 'version:' field")
        elif cff_version not in allowed:
            errors.append("CITATION.cff: version {} is neither the CMake version {} nor the newest "
                          "released version {}".format(cff_version, cmake_version, newest_dated[0]))
        if cff_date is not None and cff_version in changelog_dates:
            expected = changelog_dates[cff_version]
            if DATE_RE.match(expected) and cff_date != expected:
                errors.append("CITATION.cff: date-released {} differs from the CHANGELOG date {} of "
                              "[{}]".format(cff_date, expected, cff_version))
            if not DATE_RE.match(expected):
                errors.append("CITATION.cff: date-released is set but [{}] is still '{}' in "
                              "CHANGELOG.md".format(cff_version, expected))

    # Release mode
    if tag is not None:
        m = TAG_RE.match(tag)
        if not m:
            errors.append("tag '{}' does not match vX.Y.Z or vX.Y.Z-rcN".format(tag))
        else:
            tag_version = ".".join(m.group(1, 2, 3))
            is_rc = m.group(4) is not None
            if tag_version != cmake_version:
                errors.append("tag {} does not match CMakeLists.txt version {}".format(tag, cmake_version))
            if not is_rc:
                if not DATE_RE.match(top_date):
                    errors.append("CHANGELOG.md: [{}] is '{}'; a release tag needs a YYYY-MM-DD "
                                  "date".format(top_version, top_date))
                link = links.get(top_version, "")
                if link.endswith("HEAD"):
                    errors.append("CHANGELOG.md: link for [{}] still ends at HEAD: {}".format(top_version, link))
                elif link and not link.endswith("v" + top_version):
                    errors.append("CHANGELOG.md: link for [{}] should end at v{}: {}".format(
                        top_version, top_version, link))
                if citation is None:
                    errors.append("CITATION.cff: file not found")
                else:
                    if citation.get("version") != tag_version:
                        errors.append("CITATION.cff: version {} differs from tag {}".format(
                            citation.get("version"), tag))
                    if citation.get("date-released") != top_date:
                        errors.append("CITATION.cff: date-released {} differs from the CHANGELOG date {}".format(
                            citation.get("date-released"), top_date))

    return errors


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2],
                        help="repository root (default: two levels above this script)")
    parser.add_argument("--tag", help="release mode: the tag being released, e.g. v2.1.0")
    parser.add_argument("--print-version", action="store_true",
                        help="print the CMake version and exit")
    args = parser.parse_args(argv)

    if args.print_version:
        try:
            print(read_cmake_version(args.root))
        except (OSError, ValueError) as exc:
            print("error: {}".format(exc), file=sys.stderr)
            return 1
        return 0

    errors = check(args.root, args.tag)
    if errors:
        for error in errors:
            print("error: {}".format(error), file=sys.stderr)
        return 1
    print("Version {} is consistent{}.".format(read_cmake_version(args.root),
                                               " with tag " + args.tag if args.tag else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
