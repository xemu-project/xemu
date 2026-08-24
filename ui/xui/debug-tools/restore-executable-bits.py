#!/usr/bin/env python3
"""Restore executable bits for tracked script files whose shebang survived checkout.

This is primarily for source trees committed from Windows, where Git may record
script files as 100644. With --update-git-index, tracked shebang files are also
marked executable in the real Git index so git archive preserves the mode.
--temporary-git-index exercises the same chmod path on a disposable index copy.
"""

from __future__ import annotations

import argparse
import os
import stat
import subprocess
import tempfile
from pathlib import Path


SKIP_DIRS = {".git", "build", "dist", "pyvenv"}


def is_script(path: Path) -> bool:
    try:
        if path.is_symlink() or not path.is_file():
            return False
        with path.open("rb") as f:
            return f.read(2) == b"#!"
    except OSError:
        return False


def iter_files(root: Path):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        base = Path(dirpath)
        for name in filenames:
            yield base / name


def tracked_files(root: Path) -> list[Path] | None:
    try:
        result = subprocess.run(
            ["git", "-C", str(root), "ls-files", "-z"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
    except (OSError, subprocess.CalledProcessError):
        return None

    files = []
    for raw in result.stdout.split(b"\0"):
        if not raw:
            continue
        files.append(root / os.fsdecode(raw))
    return files


def set_executable(path: Path) -> bool:
    try:
        mode = path.stat().st_mode
        wanted = mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH
        if wanted == mode:
            return False
        path.chmod(wanted)
        return True
    except OSError:
        return False


def update_git_index(root: Path, rels: list[str], env=None) -> None:
    for i in range(0, len(rels), 100):
        batch = rels[i : i + 100]
        if batch:
            subprocess.run(
                ["git", "-C", str(root), "update-index", "--chmod=+x", "--", *batch],
                check=True, env=env,
            )


def temporary_index_environment(root: Path):
    result = subprocess.run(
        ["git", "-C", str(root), "rev-parse", "--git-path", "index"],
        check=True, stdout=subprocess.PIPE, text=True,
    )
    index_path = Path(result.stdout.strip())
    if not index_path.is_absolute():
        index_path = root / index_path
    temp = tempfile.NamedTemporaryFile(prefix="xemu-debug-tools-index-", delete=False)
    temp_path = Path(temp.name)
    temp.close()
    if index_path.is_file():
        temp_path.write_bytes(index_path.read_bytes())
    env = os.environ.copy()
    env["GIT_INDEX_FILE"] = str(temp_path)
    return temp_path, env


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=None, help="source tree root")
    parser.add_argument(
        "--update-git-index",
        action="store_true",
        help="also mark tracked scripts executable in the real Git index",
    )
    parser.add_argument(
        "--temporary-git-index",
        action="store_true",
        help="exercise Git index chmod updates against a temporary index copy",
    )
    args = parser.parse_args()
    if args.update_git_index and args.temporary_git_index:
        parser.error("choose either --update-git-index or --temporary-git-index")

    root = Path(args.root).resolve() if args.root else Path(__file__).resolve().parents[3]
    tracked = tracked_files(root)
    candidates = tracked if tracked is not None else list(iter_files(root))

    scripts: list[Path] = []
    changed = 0
    for path in candidates:
        if is_script(path):
            scripts.append(path)
            changed += int(set_executable(path))

    if tracked is not None and (args.update_git_index or args.temporary_git_index):
        rels = [str(p.relative_to(root)) for p in scripts]
        if args.update_git_index:
            update_git_index(root, rels)
        else:
            temp_index, env = temporary_index_environment(root)
            try:
                update_git_index(root, rels, env=env)
            finally:
                temp_index.unlink(missing_ok=True)

    print(f"Executable-bit repair: {len(scripts)} script(s) checked, {changed} file mode(s) changed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
