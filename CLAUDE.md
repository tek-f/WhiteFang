# WhiteFang

A custom programming language, built as a project for the Advanced
Algorithms course. Goals: learn about compilers, memory, and data
structures; stretch goal is a language capable enough to implement
algorithms from the course itself.

## Architecture

WhiteFang compiles to a **custom bytecode format, executed by a VM
(interpreter) written in C**. This is a deliberate pivot away from an
earlier plan to transpile to C source and build with gcc/clang — see
`docs/DECISIONS.md` for why. There is no C source generation step and
no dependency on gcc/clang at runtime; the compiler and VM are both C
programs, but a WhiteFang program does not become a standalone native
binary.

Stack-based VM (not register-based). Value representation, constant
pool, call-frame layout, the full opcode set, and the chunk/program
format are all designed and implemented — see `docs/VM.md`.
`./build/whitefang program.wf` runs a WhiteFang program end to end
(lex → parse → compile → execute, one process, no intermediate file).

## Docs — read these before making language or architecture decisions

- `docs/SPEC.md` — the language spec, source of truth for syntax and
  semantics. Update it whenever a language decision is made or
  changed; don't let it drift from what's actually implemented.
- `docs/VM.md` — the bytecode compiler & VM design: value
  representation, constant pool, call frames, the opcode set, and the
  chunk/program format. Source of truth for how M3 should be
  implemented; update it first if a VM-level decision changes.
- `docs/DECISIONS.md` — rationale log for every non-obvious call,
  including *why*, alternatives considered, and known costs accepted.
  When a past decision is revisited, mark the old entry superseded
  (don't delete it) and add a new entry — same pattern used for the
  transpile-to-C → bytecode/VM pivot.
- `Journal/Journal_ddmmyyyy.txt` — the user's personal progress/
  experience journal, one file per day, in their own voice. Write
  entries only when asked, from what the user dictates — light
  copyedit (typos, paragraph breaks) is fine, don't rephrase or
  invent content.

## Milestone plan

| Milestone | Deliverable | Status |
|---|---|---|
| M0 | Core language spec | done |
| M1 | Lexer | done |
| M2 | Parser → AST | done |
| M3 | Bytecode compiler (AST → bytecode) + VM | done |
| stretch | Strings, arrays, `for` loops (roughly in that order — strings/arrays share a heap/ownership design question; `for` pairs naturally with arrays) | not started |
| later, unscheduled | structs, generics, closures, modules, GC, broader stdlib | not started |

## Planned, not yet scheduled

- **Dockerize the VM** — concrete decision, deferred until M3 produces
  a working binary. **That trigger is now met** (`./build/whitefang`
  exists and works) — not yet acted on, needs the user to actually ask
  for it. Multi-stage `Dockerfile`, build stage with gcc, minimal
  runtime image. See `docs/DECISIONS.md` for full rationale and the
  still-open question (wrap just the VM, or the whole pipeline).

## Testing strategy

Golden-file tests: `make test` (or `tests/run_golden.sh` directly)
runs every `.wf` program in `tests/golden/` through `whitefang` and
diffs stdout + exit code against a recorded reference; programs in
`tests/golden/errors/` are supposed to fail, so only their exit code
(65 compile error, 70 runtime error) is checked. `tests/run_golden.sh
--record` (re)generates the reference after adding a test or a
deliberate behavior change. See `tests/golden/README.md` and
`docs/DECISIONS.md` ("Golden-file test design") for the full format
and rationale. 10 tests currently: 4 covering recursion/control-flow/
logic/constants, 6 covering the major error paths.

## Repo layout

```
docs/            language spec, VM design, decisions log
Journal/         user's personal progress journal
src/
  lexer/         WhiteFang source -> tokens
  parser/        tokens -> AST
  ast/           AST node definitions
  bytecode/      AST -> bytecode compiler
  vm/            bytecode interpreter
tests/
  run_golden.sh  golden-file test runner (`make test`)
  golden/        .wf programs + expected stdout/exit code
    errors/      .wf programs that should fail -- exit code only
examples/         example WhiteFang programs (non-test, currently empty)
```

Build tooling: plain `Makefile`, C11, `cc -std=c11 -Wall -Wextra
-Werror`. `make` builds:
- `./build/lexdump tests/golden/fib.wf` — prints every token (M1)
- `./build/parsedump tests/golden/fib.wf` — prints the parsed AST as an
  indented tree (M2)
- `./build/bcdump tests/golden/fib.wf` — disassembles the compiled
  bytecode (M3 manual-verification tool)
- `./build/whitefang tests/golden/fib.wf` — runs a WhiteFang program end
  to end; exit code is the program's own `start` return value
- `make test` — runs the golden-file test suite (see Testing strategy
  below)

## Working conventions

- The user commits and pushes to git themselves — don't run `git
  commit`/`git push` proactively in this repo.
- Distinctive/non-standard syntax choices in WhiteFang (e.g.
  type-before-name variable declarations, trailing return-type arrow,
  backtick-based comments) are intentional design goals for this
  project, not mistakes to "fix" toward C-family convention.
