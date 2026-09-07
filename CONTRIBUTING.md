# Contributing to xemu

Thank you for your interest in contributing to xemu! xemu is an open-source, cross-platform original Xbox emulator built on top of QEMU. Contributions from the community help make xemu more accurate, performant, and accessible.

Please take a moment to review this guide before submitting issues or pull requests.

---

## Code of Conduct & Civility

We are committed to providing a friendly, safe, and welcoming environment for everyone, regardless of experience level, background, or identity.

- **Be respectful and civil**: Treat other contributors, maintainers, and users with kindness and patience. Technical disagreements are normal and expected, but discussions must remain constructive and focused on the code and design.
- **Unacceptable behavior**: Personal attacks, trolling, insults, harassment, exclusionary comments, and disrespectful conduct will not be tolerated.
- **Reference**: For more details, see [docs/devel/code-of-conduct.rst](docs/devel/code-of-conduct.rst).

---

## Getting Started & Communicating

### Reporting Bugs & Requesting Features
- Search existing [GitHub Issues](https://github.com/xemu-project/xemu/issues) to verify that your problem or proposal has not already been reported.
- When opening a new issue, choose the appropriate issue template (Bug Report, Title Compatibility, or Feature Request) and fill in all requested information, including logs, system specifications, and reproduction steps.

### Discuss Major Changes First
If you are planning a significant new feature, major architectural refactor, or complex hardware subsystem rewrite, **please discuss it first with maintainers on the xemu Discord**. Discussing your proposed approach ahead of time helps ensure that effort aligns with project goals, fits into current architecture plans, and avoids wasted time on changes that may not be mergeable.

---

## Pull Request Guidelines

### Check for Existing PRs & Avoid Duplicate Work
Before starting work or submitting a pull request, **always check open pull requests** to avoid duplicating effort or duplication of work that is already underway.
- **Collaborate first**: If an open PR already touches the same issue or subsystem, please reach out and attempt to collaborate with the other PR author before creating a competing PR.
- **Acknowledge and justify overlapping PRs**: If there is genuine reason to open a separate PR that overlaps with an existing one (e.g., the original PR has been abandoned), please link to the existing PR, acknowledge the duplication, and provide clear reasoning in your PR description.

### Workflow
1. **Check for Existing Work**: Ensure no existing PR is already addressing your change (as described above).
2. **Fork and Branch**: Fork the repository on GitHub and create a descriptive feature branch from `master`.
3. **Keep PRs Focused**: Each pull request should address a single bug fix, improvement, or feature. Avoid bundling unrelated changes, refactorings, or formatting updates into a single PR.
4. **Keep Branches Up-to-Date**: Rebase your branch onto the latest upstream `master` branch before submitting and when requested during review.
5. **CI Checks**: Ensure your code builds cleanly and passes all automated GitHub Actions checks. Note that first time contributors may need a maintainer to enable the tests; you may wish to run them yourself via a mock PR against your own repository.

---

## Testing & Validation Requirements

All proposed changes must be thoroughly tested before submitting a pull request.

- **Automated & Unit Tests**: Ensure existing tests pass, and add new unit tests when adding new logic or helper utilities where feasible.
- **Test XBEs for Hardware Parity**: When submitting changes intended to match original Xbox hardware behavior, providing a test XBE is strongly encouraged. These should be structured to allow execution on original Xbox hardware as well as xemu to facilitate direct comparison of the results.
- **Manual testing**: In cases where automated tests are unusually challenging, please perform manual testing on as many platforms as possible. Be sure to include instructions capturing how to test the change in your PR description.

Providing test cases or test XBEs enables reviewers to independently reproduce and validate the change and allows the test to be integrated into automated regression testing suites to guard against future regressions.

---

## Commit Message Conventions

Commit messages are an important part of the project history and documentation. xemu follows a structured commit message format:

```text
<subsystem>: <short description>

[optional detailed body explaining why the change was made]
```

### Subject Line
- Format: `<subsystem>: <short description>`
- Write in the imperative mood (e.g., `nv2a: Fix texture cache invalidation` rather than `nv2a: Fixed...` or `nv2a: Fixes...`).
- Keep the subject line concise (aim for ~50–72 characters).

> [!TIP]
> When in doubt about which subsystem prefix to use or how to structure your commit message, run `git log` on the file or subsystem you modified:
> ```bash
> git log -n 5 path/to/modified/file
> ```

### Message Body
- Separate the subject line from the body with a blank line.
- Use the body to explain why the change is necessary, what hardware behavior or quirk was observed, references to documentation or hardware test results, and any trade-offs or technical decisions made.
- For non-trivial changes, a detailed body is strongly encouraged.

---

## Code Style & Standards

- **Formatting Newly Added Files**: The use of `clang-format` is **required** for all newly added files.
- **Modifying Existing Files**: When modifying existing files, changes should match the local style of the file.
- **No Unrelated Style Changes in Functional PRs**: Stylistic changes (reformatting, renaming, whitespace adjustments) that are unrelated to functionality changes **must be made in a separate PR**, unless discussed and agreed upon on the xemu Discord ahead of time. Mixing style changes with bug fixes or features complicates code reviews, obscures git history, and increases merge conflicts.
- **Comment Tricky Code Only**: Comments should be reserved for code that is not self-explanatory. Remember that comments add maintenance burden; if someone changes the code and forgets to update the comment it becomes difficult to determine the actual intent.

---

## Use of AI Tooling

Generative AI tools (such as GitHub Copilot, ChatGPT, Claude, Gemini, and similar LLM-based assistants) are generally permitted as aids in writing code and documentation. However, there are some common-sense requirements:

1. **You must understand the code**: You are personally responsible for every line of code you propose. You must thoroughly read, understand, verify, and test all AI-generated code before submitting it.
2. **No unreviewed AI output**: Submitting blindly copied, unverified, or hallucinated AI output ("AI slop") is strictly forbidden - please don't waste maintainers' limited review time or your token budget.
3. **Active ownership and review participation**: You must be willing and technically capable of guiding your pull request through the entire review process. This includes answering technical questions, explaining architectural decisions, and making necessary adjustments based on reviewer feedback. If you cannot explain or defend the code you submitted, the PR will not be accepted. Note that this does not mean simply acting as a proxy for your chatbot.

Please also note which model or models were used in the creation of your PR. This is useful for review purposes and to guide other contributors towards models that have been shown to be successful.

---

## The Review Process & Expectations

Please be aware of the following regarding code reviews:

- **Reviews take time**: The review process can be quite long, often on the order of months. Emulation development is complex and demanding. A seemingly innocuous change can break dozens of games, often in subtle ways. In addition, xemu maintainers are unpaid volunteers who are donating their limited free time to the project.
- **Feedback & Iteration**: Code review comments are aimed at maintaining the quality and long-term maintainability of the project. Please remain receptive to feedback and prepared to update your changes as needed.
- **Patience is appreciated**: Please do not repeatedly ping reviewers.
