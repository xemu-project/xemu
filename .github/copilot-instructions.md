When performing a code review, consider developer documentation located at `/docs/devel`.

Style guide is located at `/docs/devel/style.rst`.

Before writing or reviewing any Xbox-specific code, read:
- `/docs/devel/xbox-architecture.rst` — ownership boundaries for `hw/xbox/`, `hw/xbox/nv2a/`, `net/`, `ui/xui/`, and `tests/xbox/`
- `/docs/devel/ai-tasks.rst` — required task declaration format (target_folder, expected_behavior, test_plan, perf_risk, save_state_risk, multiplayer_risk)

Every AI coding task that touches OpenXbox code must include a complete task declaration (see `.github/AI_TASK_TEMPLATE.md`). Do not begin implementation until all six fields are filled in.
