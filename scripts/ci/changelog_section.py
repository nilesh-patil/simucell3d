#!/usr/bin/env python3
"""Print the CHANGELOG.md section of one version, for use as GitHub release notes.

Release notes are taken only from the curated CHANGELOG.md, never from ``git log``.
The section runs from its ``## [X.Y.Z]`` heading (exclusive) to the next ``## [`` heading
or the link references at the end of the file. A trailing link to the full CHANGELOG is
appended.

Usage::

    python3 scripts/ci/changelog_section.py 2.1.0 > notes.md
    python3 scripts/ci/changelog_section.py v2.1.0 > notes.md   # a leading 'v' is ignored

Exit status 1 if the version has no section or the section is empty.
"""

import argparse
import re
import sys
from pathlib import Path


def extract(text, version):
    """Return the body of the '## [version]' section, stripped, or None if absent."""
    lines = text.splitlines()
    heading = re.compile(r"^## \[" + re.escape(version) + r"\](\s|$)")
    start = next((i for i, line in enumerate(lines) if heading.match(line)), None)
    if start is None:
        return None
    body = []
    for line in lines[start + 1:]:
        if line.startswith("## [") or re.match(r"^\[[^\]]+\]:\s*\S+", line):
            break
        body.append(line)
    return "\n".join(body).strip()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("version", help="X.Y.Z or vX.Y.Z (an -rcN suffix is ignored)")
    parser.add_argument("--changelog", type=Path,
                        default=Path(__file__).resolve().parents[2] / "CHANGELOG.md")
    parser.add_argument("--repo-url", default="https://github.com/nilesh-patil/simucell3d",
                        help="used for the link to the full CHANGELOG")
    args = parser.parse_args(argv)

    version = re.sub(r"-rc\d+$", "", args.version.lstrip("v"))
    body = extract(args.changelog.read_text(encoding="utf-8"), version)
    if not body:
        print("error: no non-empty section '## [{}]' in {}".format(version, args.changelog), file=sys.stderr)
        return 1
    tag = args.version if args.version.startswith("v") else "v" + args.version
    print(body)
    print()
    print("The full list of changes is in [CHANGELOG.md]({}/blob/{}/CHANGELOG.md).".format(args.repo_url, tag))
    return 0


if __name__ == "__main__":
    sys.exit(main())
