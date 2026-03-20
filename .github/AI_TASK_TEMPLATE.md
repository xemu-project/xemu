# AI Task Template

> **Required for every AI-assisted coding task in OpenXbox.**
> Copy this template into a GitHub issue body, pull request description,
> or a Copilot workspace `task.yaml` and fill in **all** fields before
> assigning work to an AI agent.
>
> Background: [docs/devel/xbox-architecture.rst] and [docs/devel/ai-tasks.rst]

---

## Task Declaration

```yaml
target_folder: "<domain root — one of: hw/xbox/, hw/xbox/nv2a/, net/, ui/xui/, tests/xbox/>"
expected_behavior: >
  <Observable outcome after the change.  Write this as a user-facing or
  test-verifiable statement, e.g. "nvnet no longer drops broadcast packets
  when PROMISC is set".>
test_plan: >
  <How correctness will be validated.  Reference at least one of:
    - An existing suite:  meson test -C build --suite xbox
    - A new test file to be added under tests/xbox/
    - A manual procedure with explicit pass/fail criteria>
perf_risk: "<None|Low|Medium|High> — <one-sentence rationale>"
save_state_risk: "<None|Low|Medium|High> — <one-sentence rationale>"
multiplayer_risk: "<None|Low|Medium|High> — <one-sentence rationale>"
```

---

## Checklist

- [ ] `target_folder` is one of the five defined domain roots (or a
      cross-domain change is explicitly justified above)
- [ ] `expected_behavior` describes a verifiable outcome
- [ ] `test_plan` references a concrete test or manual procedure
- [ ] `perf_risk` assessed; NV2A / APU / DSP changes are ≥ Medium
- [ ] `save_state_risk` assessed; any VMState struct change is ≥ High
- [ ] `multiplayer_risk` assessed; net/ or nvnet changes are ≥ Medium
- [ ] OWNERS file(s) for affected domain(s) consulted

---

## Domain Quick Reference

| Domain | Subtree | Key risk flags |
|--------|---------|----------------|
| Xbox machine | `hw/xbox/` | save_state_risk if SMBus/APU structs change |
| NV2A GPU | `hw/xbox/nv2a/` | perf_risk ≥ Medium for render-loop changes |
| Network transport | `net/` | multiplayer_risk ≥ Medium |
| Frontend UX | `ui/xui/` | perf_risk if overlay is drawn every frame |
| Xbox tests | `tests/xbox/` | None by default |
