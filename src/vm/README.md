# vm

The bytecode interpreter: dispatch loop, value stack, call frames.
Executes the `Program`/`Chunk` format produced by `src/bytecode/`.
M3 VM half — done.

- `vm.h` / `vm.c` — `vm_run(const Program *)`. A 65536-`Value` operand
  stack and a 256-deep call-frame stack (both fixed-size, per
  `docs/VM.md` sec 7's "generously-sized" choice). `start` is treated
  as if it were itself an initial "call" (a synthetic frame 0), so
  every `RETURN` — including `start`'s own — goes through identical
  logic with no special-casing for the outermost frame; see
  `docs/DECISIONS.md`.
- Runtime errors (stack overflow, integer division by zero) report to
  stderr and exit 70 — distinct from the compiler/parser's 65, since
  these are runtime failures rather than source-level mistakes. Float
  division by zero is not an error (IEEE 754 `inf`/`nan`, well-defined
  behavior, no check needed).
- No separate `src/main.c` driver lives here — `src/main.c` at the
  repo root wires lexer → parser → compiler → `vm_run` into the
  `whitefang` binary.
