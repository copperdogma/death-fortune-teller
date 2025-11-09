# Story: Modular Runtime Foundation (Pluggable Components)

Status: In Review

---

## Problem Statement
Recent refactors (Stories 012 and 013) pushed the project toward a highly abstracted, host-test-heavy architecture. That trajectory overshot the practical needs for this hobby-first, multi‑prop codebase. We need a smaller, clearer runtime modularization that:

- Keeps Arduino friendliness and straightforward wiring
- Makes key subsystems easy to plug in/out for future animatronics
- Treats CLI as a first-class, always-present component
- Avoids heavy DI/adapter proliferation and large host harnesses

## Goals
- Extract clean module boundaries for core subsystems (CLI, audio, printer, content selection, connectivity) without over-engineering.
- Ensure modules can be enabled/disabled at build time and easily swapped across props.
- Retain today’s working behaviors and Arduino build performance.

## Non-Goals
- No deep dependency injection across every controller.
- No full host-side orchestration tests of the main loop.
- No HIL automation; we’ll rely on logs/self-tests for hardware.

## Acceptance Criteria
- A small, documented set of runtime seams (headers + concrete impls) define pluggable components used by `AppController`.
- CLI remains centralized via a router with injected dependencies for the commands we support across props.
- Content selection logic (directory/skit) stays testable on host; tests continue to pass.
- Audio buffering helper remains isolated and testable.
- README/CONTRIBUTING gains a short section describing how to include/exclude modules per prop.

## Tasks
- [x] Define the “minimal seams” for pluggable modules (CLI, Content Selection, Printer, Connectivity), avoiding heavy adapter layers.
- [x] Trim `AppController` to use those seams with light, direct wiring (Arduino-first).
- [x] Keep and polish: `CliCommandRouter`, `CliService`, `AudioDirectorySelector`, `SkitSelector`, `CircularAudioBuffer` and their unit tests.
- [x] Provide simple compile-time toggles for optional modules (e.g., connectivity).
- [x] Update docs with the module map and usage examples.

## Saved Work (from Stories 012/013) — Where to Find It
To prevent rework, the following items are preserved for reference and cherry‑picking. See: `docs/saved-work/014-keepers/KEEPERS.md` and the archive script `scripts/collect_keepers.sh`.

### Preserved Candidates (Reason)
- `src/cli_command_router.{h,cpp}` (Reusable CLI routing with injectable deps.)
- `src/cli_service.{h,cpp}` (Deterministic command buffering for Serial.)
- `src/infra/circular_audio_buffer.h` (Pure helper + host tests.)
- `src/audio_directory_selector.{h,cpp}` and tests (Host-testable selection API.)
- `src/skit_selector.{h,cpp}` and tests (No-repeat skit selection.)
- Unit test suites under `tests/unit/test_cli_command_router`, `tests/unit/test_circular_audio_buffer`,
  `tests/unit/test_audio_directory_selector`, `tests/unit/test_skit_selector` (lightweight and valuable).
- Story snapshots: `docs/stories/story-012-runtime-modularity-and-coverage.md` and
  `docs/stories/story-013-runtime-diagnostics-and-coverage.md` saved to `docs/saved-work/` for historical context.

Note: Heavier runtime DI/adapters/services added during 012/013 are intentionally not part of the baseline for this story; keep them only as reference in the saved archive.

## Work Log

### 20251106-1551 — Drafted story and preservation plan
- Result: Success
- Notes: Captured scope/criteria; listed keepers. Created plan to archive selected files and story snapshots prior to revert.
- Next: Archive keepers and story snapshots via `scripts/collect_keepers.sh`; then proceed with revert discussion.

### 20251109-0930 — Implemented modular runtime foundation
- Result: Success
- Notes: Restored keeper modules/tests, introduced lightweight `AppController` seams with compile-time toggles, rewired main entrypoint, and documented module options in README.
- Next: Await hardware verification of new runtime wiring and decide on follow-up host orchestrations if needed.

