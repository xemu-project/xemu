#!/usr/bin/env python3
import os
import subprocess
import sys
from datetime import datetime


def main():
    if len(sys.argv) < 2:
        sys.exit(1)
    os.chdir(sys.argv[1])

    def cmd(c):
        return subprocess.run(
            c, shell=True, capture_output=True, text=True
        ).stdout.strip()

    def read(f):
        return open(f).read().strip() if os.path.exists(f) else ""

    commit = (
        cmd("git rev-parse HEAD") if os.path.exists(".git") else read("XEMU_COMMIT")
    )
    branch = (
        cmd("git symbolic-ref --short HEAD")
        if os.path.exists(".git")
        else read("XEMU_BRANCH")
    )
    version = (
        cmd("git describe --tags --match 'v*'")
        if os.path.exists(".git")
        else read("XEMU_VERSION")
    )

    version = version[1:]
    if not version:
        version = "0.0.0"

    # Parse version
    parts = f"{version}-0".split("-")
    dot_parts = parts[0].split(".")

    # Output
    print(f'#define XEMU_VERSION       "{version}"')
    print(f"#define XEMU_VERSION_MAJOR {dot_parts[0]}")
    print(f"#define XEMU_VERSION_MINOR {dot_parts[1]}")
    print(f"#define XEMU_VERSION_PATCH {dot_parts[2]}")
    print(f"#define XEMU_VERSION_COMMIT {parts[1]}")
    print(f'#define XEMU_BRANCH        "{branch}"')
    print(f'#define XEMU_COMMIT        "{commit}"')
    print(
        f'#define XEMU_DATE          "{datetime.now().strftime("%Y-%m-%d %H:%M:%S")}"'
    )


if __name__ == "__main__":
    main()
