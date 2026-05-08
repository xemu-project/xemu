#!/usr/bin/env python3
"""
Generate changelog for xemu releases.

This script generates a markdown file that includes:
- Pull requests merged since the last release
- Issues closed since the last release
- Affected game titles (extracted from issue bodies)

Requires the GitHub CLI (gh) to be installed and authenticated.
"""

import argparse
import json
import re
import subprocess
import sys
from collections import defaultdict
from urllib.request import urlopen
from urllib.error import URLError


REPO_NAME = "xemu-project/xemu"
XEMU_TITLE_URL_BASE = "https://xemu.app/titles/"
XDB_RAW_URL_BASE = "https://raw.githubusercontent.com/xemu-project/xdb/main/titles"


def get_title_name(title_id: str) -> str | None:
    """
    Fetch title name from xemu-project/xdb for a specific title ID.
    Title IDs are 8 hex chars: first 2 bytes are publisher code (ASCII), last 2 bytes are title number.
    """
    title_id = title_id.lower()
    if len(title_id) != 8:
        return None

    try:
        pub_code = bytes.fromhex(title_id[:4]).decode("ascii")
        title_num = int(title_id[4:], 16)
    except (ValueError, UnicodeDecodeError):
        return None

    info_url = f"{XDB_RAW_URL_BASE}/{pub_code}/{title_num:03d}/info.json"
    try:
        with urlopen(info_url, timeout=5) as response:
            info = json.loads(response.read().decode("utf-8"))
            return info.get("name")
    except (URLError, json.JSONDecodeError, TimeoutError):
        return None


def get_previous_tag(current_ref: str) -> str | None:
    """Get the previous release tag before the current ref."""
    try:
        result = subprocess.run(
            [
                "git",
                "describe",
                "--tags",
                "--match",
                "v*",
                "--abbrev=0",
                f"{current_ref}^",
            ],
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        return None


def get_commits_between(from_ref: str, to_ref: str) -> list[str]:
    """Get list of commit SHAs between two refs."""
    try:
        result = subprocess.run(
            [
                "git",
                "log",
                "--pretty=format:%H",
                "--first-parent",
                f"{from_ref}..{to_ref}",
            ],
            capture_output=True,
            text=True,
            check=True,
        )
        if result.stdout.strip():
            return result.stdout.strip().split("\n")
        return []
    except subprocess.CalledProcessError:
        return []


def extract_title_ids(text: str) -> set[str]:
    """Extract Xbox title IDs from issue body text."""
    if not text:
        return set()
    titles_re = re.compile(r"Titles?[:/]\s*([a-fA-F0-9,\s]+)", re.IGNORECASE)
    title_id_re = re.compile(r"([a-fA-F0-9]{8})")
    references = " ".join(titles_re.findall(text))
    return set(title_id_re.findall(references.lower()))


def format_title_reference(title_id: str, title_cache: dict[str, str | None]) -> str:
    """Format a title ID as a markdown link with the game name."""
    title_id = title_id.lower()
    if title_id not in title_cache:
        title_cache[title_id] = get_title_name(title_id)
    name = title_cache[title_id]
    if name:
        return f"[{name}]({XEMU_TITLE_URL_BASE}{title_id})"
    return f"`{title_id}`"


def log(msg: str) -> None:
    """Print a log message to stderr."""
    print(f"[changelog] {msg}", file=sys.stderr)


def gh_api(endpoint: str) -> dict | list | None:
    """Call GitHub REST API using gh CLI."""
    try:
        result = subprocess.run(
            ["gh", "api", f"repos/{REPO_NAME}/{endpoint}"],
            capture_output=True,
            text=True,
            check=True,
        )
        return json.loads(result.stdout)
    except (subprocess.CalledProcessError, json.JSONDecodeError):
        return None


def gh_graphql(query: str) -> dict | None:
    """Call GitHub GraphQL API using gh CLI."""
    try:
        result = subprocess.run(
            ["gh", "api", "graphql", "-f", f"query={query}"],
            capture_output=True,
            text=True,
            check=True,
        )
        return json.loads(result.stdout)
    except (subprocess.CalledProcessError, json.JSONDecodeError):
        return None


def generate_changelog(
    current_ref: str,
    previous_tag: str | None = None,
) -> str:
    """Generate changelog markdown content."""
    title_cache: dict[str, str | None] = {}

    if previous_tag is None:
        log(f"Finding previous tag before {current_ref}...")
        previous_tag = get_previous_tag(current_ref)

    if previous_tag is None:
        return "No previous release found.\n"

    log(f"Generating changelog from {previous_tag} to {current_ref}")

    commits = get_commits_between(previous_tag, current_ref)
    commit_shas = set(commits)
    log(f"Found {len(commit_shas)} commits in range")
    if not commit_shas:
        return "No changes since last release.\n"

    log("Fetching pull requests for commits...")
    merged_prs: list[dict] = []
    pr_numbers_seen: set[int] = set()

    for sha in commit_shas:
        pulls = gh_api(f"commits/{sha}/pulls")
        if not pulls:
            continue
        for pr in pulls:
            pr_num = pr["number"]
            if pr_num in pr_numbers_seen:
                continue
            pr_numbers_seen.add(pr_num)

            # Skip dependabot PRs that only touch CI/workflow files
            if pr["user"]["login"] == "dependabot[bot]":
                files = gh_api(f"pulls/{pr_num}/files") or []
                if all(f["filename"].startswith(".github/") for f in files):
                    log(f"  Skipping CI-only dependabot PR #{pr_num}: {pr['title']}")
                    continue

            merged_prs.append(pr)
            log(f"  Found PR #{pr_num}: {pr['title']}")

    log(f"Found {len(merged_prs)} merged PRs")
    merged_prs.sort(key=lambda pr: pr.get("merged_at") or "", reverse=True)

    log("Fetching closed issues via GraphQL...")
    closed_issues: list[dict] = []
    issue_numbers_seen: set[int] = set()
    affected_titles: dict[str, list[dict]] = defaultdict(list)

    if merged_prs:
        owner, name = REPO_NAME.split("/")
        pr_numbers = [pr["number"] for pr in merged_prs]
        pr_queries = " ".join(
            f"pr{pr_num}: pullRequest(number: {pr_num}) {{ closingIssuesReferences(first: 10) {{ nodes {{ number title body state stateReason }} }} }}"
            for pr_num in pr_numbers
        )
        query = f'{{ repository(owner: "{owner}", name: "{name}") {{ {pr_queries} }} }}'

        result = gh_graphql(query)
        if result:
            repo_data = result.get("data", {}).get("repository", {})
            for pr_num in pr_numbers:
                pr_data = repo_data.get(f"pr{pr_num}", {})
                issues = pr_data.get("closingIssuesReferences", {}).get("nodes", [])
                for issue in issues:
                    if issue["number"] in issue_numbers_seen:
                        continue
                    if (
                        issue["state"] == "CLOSED"
                        and issue["stateReason"] == "COMPLETED"
                    ):
                        issue_numbers_seen.add(issue["number"])
                        closed_issues.append(issue)
                        log(f"  Found issue #{issue['number']}: {issue['title']}")
                        title_ids = extract_title_ids(issue.get("body") or "")
                        for tid in title_ids:
                            affected_titles[tid].append(issue)

    log(f"Found {len(closed_issues)} closed issues")
    if affected_titles:
        log(f"Fetching names for {len(affected_titles)} affected titles...")
        for tid in affected_titles.keys():
            if tid not in title_cache:
                title_cache[tid] = get_title_name(tid)
                name = title_cache[tid]
                if name:
                    log(f"  {tid} -> {name}")
                else:
                    log(f"  {tid} -> (not found)")

    log("Building changelog...")
    lines = []

    if merged_prs:
        lines.append("## Pull Requests")
        lines.append("")
        for pr in merged_prs:
            lines.append(f"* #{pr['number']} - {pr['title']} (@{pr['user']['login']})")
        lines.append("")

    if closed_issues:
        lines.append("## Issues Fixed")
        lines.append("")
        for issue in closed_issues:
            lines.append(f"* #{issue['number']} - {issue['title']}")
        lines.append("")

    if affected_titles:
        lines.append("## Affected Titles")
        lines.append("")
        lines.append("The following titles had issues fixed in this release:")
        lines.append("")
        for tid in sorted(affected_titles.keys()):
            issue_nums = ", ".join(f"#{i['number']}" for i in affected_titles[tid])
            title_ref = format_title_reference(tid, title_cache)
            lines.append(f"* {title_ref} ({issue_nums})")
        lines.append("")

    if not merged_prs and not closed_issues:
        log("No PRs or issues found, falling back to git log")
        lines.append("## Changes")
        lines.append("")
        try:
            result = subprocess.run(
                [
                    "git",
                    "log",
                    "--pretty=format:* %h %s",
                    "--first-parent",
                    f"{previous_tag}..{current_ref}",
                ],
                capture_output=True,
                text=True,
                check=True,
            )
            lines.append(result.stdout)
        except subprocess.CalledProcessError:
            lines.append("See commit history for changes.")
        lines.append("")

    compare_url = (
        f"https://github.com/{REPO_NAME}/compare/{previous_tag}...{current_ref}"
    )
    lines.append(f"**Full Changelog**: {compare_url}")

    log("Done")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate changelog for xemu releases")
    parser.add_argument(
        "--ref",
        default="HEAD",
        help="Git ref to generate changelog for (default: HEAD)",
    )
    parser.add_argument(
        "--previous-tag",
        help="Previous release tag to compare against (auto-detected if not specified)",
    )
    parser.add_argument(
        "--output",
        "-o",
        help="Output file (default: stdout)",
    )

    args = parser.parse_args()
    changelog = generate_changelog(
        current_ref=args.ref,
        previous_tag=args.previous_tag,
    )
    if args.output:
        with open(args.output, "w") as f:
            f.write(changelog)
        print(f"Changelog written to {args.output}", file=sys.stderr)
    else:
        print(changelog)


if __name__ == "__main__":
    main()
