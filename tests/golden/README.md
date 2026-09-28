# golden tests

Each test is a `.wf` program checked against its previously-recorded
correct behavior — run via `make test` (or `tests/run_golden.sh`
directly). See `docs/DECISIONS.md` ("Golden-file test design") for the
full rationale.

Format:
- `<name>.wf` — the program
- `<name>.expected` — exact expected stdout (success tests only)
- `<name>.exitcode` — expected exit code; omitted means `0`
- `errors/<name>.wf` — a program that's *supposed* to fail to compile
  or crash at runtime; only its exit code is checked (65 = compile
  error, 70 = runtime error) — stdout is never compared for these,
  see `docs/DECISIONS.md` for why only the exit code, not the error
  message text, is pinned

Adding a test: write the `.wf`, verify its output looks right by eye
(`./build/whitefang your_test.wf`), then run `tests/run_golden.sh
--record` to save that output as the new reference. Same command
after any *deliberate* behavior change, to update existing tests.

These double as example programs demonstrating language features —
`examples/` is for programs that don't need a golden reference.
