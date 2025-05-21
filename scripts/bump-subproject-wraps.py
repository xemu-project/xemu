#!/usr/bin/env python
# /// script
# dependencies = ["requests"]
# ///
"""
Update Meson wrap file `revision` fields to point to latest release.
"""
from __future__ import annotations
import argparse
import configparser
import json
import logging
import os
import re
import sys
from pathlib import Path
from dataclasses import dataclass, asdict

import requests


log = logging.getLogger(__name__)


SEMVER_RE = re.compile(
    r"""
    ^v?
    (?P<major>0|[1-9]\d*)\.
    (?P<minor>0|[1-9]\d*)\.
    (?P<patch>0|[1-9]\d*)
    $""",
    re.VERBOSE,
)

ROOT = Path(__file__).resolve().parents[1]
WRAP_DIR = ROOT / "subprojects"
SESSION = requests.Session()
GH_TOKEN = os.getenv("GH_TOKEN", "")
if GH_TOKEN:
    SESSION.headers["Authorization"] = f"Bearer {GH_TOKEN}"
SESSION.headers["Accept"] = "application/vnd.github+json"


def gh_sha_for_tag(owner: str, repo: str, tag: str) -> str:
    data = SESSION.get(
        f"https://api.github.com/repos/{owner}/{repo}/git/ref/tags/{tag}", timeout=30
    ).json()

    # First level: get the object it points to
    obj_type = data["object"]["type"]
    obj_sha = data["object"]["sha"]

    if obj_type == "commit":
        # Lightweight tag
        return obj_sha
    elif obj_type == "tag":
        # Annotated tag: need to dereference
        tag_obj_url = data["object"]["url"]
        tag_data = requests.get(tag_obj_url).json()
        return tag_data["object"]["sha"]
    else:
        raise Exception(f"Unknown object type: {obj_type}")


def gh_latest_release(
    owner: str, repo: str, pattern: re.Pattern
) -> None | tuple[str, str]:
    """
    Return (tag_name, commit_sha) for the most recent matching release.
    """
    releases = SESSION.get(
        f"https://api.github.com/repos/{owner}/{repo}/releases", timeout=30
    ).json()
    viable = [t for t in releases if pattern.match(t["tag_name"])]

    if not viable:
        return None

    tag_name = viable[0]["tag_name"]
    sha = gh_sha_for_tag(owner, repo, tag_name)

    return tag_name, sha


def gh_latest_tag(owner: str, repo: str, pattern: re.Pattern) -> tuple[str, str]:
    """
    Return (tag_name, commit_sha) for the most recent matching tag.
    """
    tags = SESSION.get(
        f"https://api.github.com/repos/{owner}/{repo}/tags", timeout=30
    ).json()
    viable = [t for t in tags if pattern.match(t["name"])]

    if not viable:
        return None

    return viable[0]["name"], viable[0]["commit"]["sha"]


@dataclass
class UpdatedWrap:
    path: str
    owner: str
    repo: str
    old_rev: str
    new_rev: str
    new_tag: str


def update_wrap(path: Path) -> None | UpdatedWrap:
    """
    Return (tag_name, commit_sha) if updated, otherwise None.
    """
    cp = configparser.ConfigParser(interpolation=None)
    cp.read(path, encoding="utf-8")

    if "wrap-git" not in cp:
        # FIXME: Support wrap-file from wrapdb
        return None

    w = cp["wrap-git"]
    url = w.get("url", "")
    rev = w.get("revision", "").strip()
    m = re.match(r".*github\.com[:/](?P<owner>[^/]+)/(?P<repo>[^/.]+)(?:\.git)?", url)
    if not (m and rev):
        return None

    owner, repo = m.group("owner"), m.group("repo")
    try:
        pattern = cp.get("update", "tag_regex", fallback=None)
        pattern = re.compile(pattern) if pattern else SEMVER_RE

        latest = gh_latest_release(owner, repo, pattern)
        if latest is None:
            log.info("Couldn't find latest release for %s/%s", owner, repo)
            log.info("Searching for tags directly...")
            latest = gh_latest_tag(owner, repo, pattern)
            if latest is None:
                log.info("Couldn't find latest tag for %s/%s", owner, repo)
                return None
        tag, sha = latest
    except Exception as e:
        log.exception(e)
        return None

    if sha.startswith(rev):
        log.info("%s already at %s (%s)", path.name, tag, sha)
        return None

    log.info("%s updated to %s (%s)", path.name, tag, sha)

    w["revision"] = sha

    with open(path, "w", encoding="utf-8") as file:
        cp.write(file)

        # XXX: ConfigParser writes two extra newlines. Trim the last one.
        file.seek(file.tell() - 1, 0)
        file.truncate()

    return UpdatedWrap(str(path), owner, repo, rev, sha, tag)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--manifest",
        "-m",
        action="store_true",
        default=False,
        help="Print JSON-formatted updated manifest",
    )
    ap.add_argument(
        "wraps", nargs="*", help="Which wraps to update, or all if unspecified"
    )
    args = ap.parse_args()

    wraps = args.wraps
    if wraps:
        wraps = [Path(p) for p in wraps]
    else:
        wraps = WRAP_DIR.glob("*.wrap")

    logging.basicConfig(level=logging.INFO)

    updated = []
    for wrap in wraps:
        info = update_wrap(wrap)
        if info:
            updated.append(asdict(info))

    if args.manifest:
        json.dump(updated, sys.stdout, indent=2)


if __name__ == "__main__":
    main()
