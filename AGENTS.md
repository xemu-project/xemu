# Instructions for Autonomous Agents & AI Assistants

This file provides mandatory instructions and guidelines for autonomous AI agents, coding assistants, and models contributing code or creating pull requests for the xemu project.

---

## 1. Strict Adherence to Project Standards

All contributions must strictly comply with the guidelines defined in [CONTRIBUTING.md](CONTRIBUTING.md).

---

## 2. Mandatory Agent Declarations

When submitting code or opening a pull request generated with or assisted by an agent:

1. **PR Description Declaration**:
   - The pull request description **must** include an explicit declaration stating which AI agent and model were used to generate or assist with the change.
   - Example:
     ```markdown
     > **Agent Declaration**: This pull request was created with assistance from [Agent Name / Model Name].
     ```

---

## 3. Scoping & Granularity Guidelines

To produce high-quality, easily reviewable pull requests, agents must observe the following constraints:

- **Single Responsibility**: Each pull request must address exactly one bug fix, hardware improvement, or specific feature. Never bundle multiple independent bug fixes, features, or cleanups into a single commit or pull request. Break independent changes into separate, logically sequenced PRs.
- **Minimal Change**: Touch only the files and lines necessary to accomplish the stated task. Do not refactor surrounding functions or reorganize include headers unless explicitly requested.
- **Verify Against Upstream**: Always ensure the branch is rebased on the latest upstream `master` and that changes do not stomp on or duplicate existing open PRs.

---

## 4. Verification & Testing

- **Compilation**: Verify that all modified files compile without warnings or errors.
- **Emulation Accuracy**: Do not hallucinate register definitions, bitfields, or hardware behaviors. Cross-reference existing implementations under `hw/xbox/` or verified hardware documentation.
- **Test Coverage & Parity**: Whenever altering hardware emulation (NV2A, APU/DSP, MCPX, memory controller, etc.), provide or suggest a test XBE that can be run on both bare-metal Xbox hardware and xemu to validate behavior.

---

## 5. Agent Pre-Submission Checklist

Before finalizing any commit or pull request, ensure:
- [ ] Commit message uses `<subsystem>: <short description>` followed by a detailed explanatory body.
- [ ] `clang-format` is applied to new files, and existing code style is respected.
- [ ] No unrelated formatting or refactoring changes are included.
- [ ] The pull request description includes the agent/model declaration.
- [ ] Existing open pull requests have been searched to avoid duplicating work.
