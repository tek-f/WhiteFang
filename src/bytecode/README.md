# bytecode

AST -> bytecode compiler, per `docs/VM.md`. M3 compiler half — done.

- `value.h` — the untagged `Value` union (`docs/VM.md` sec 2)
- `opcode.h` — the full `OpCode` enum (`docs/VM.md` sec 6, plus
  `INEG`/`FNEG` added during implementation — see `docs/DECISIONS.md`)
- `chunk.h` / `chunk.c` — `Chunk`/`Program` structs and the growable
  code/constant-pool helpers
- `compiler.h` / `compiler.c` — `compile_program`: two-pass (function
  signatures collected before any body is compiled), a flat-array
  symbol table for locals (block-scoped, mirrors the VM's frame-
  relative slots), bottom-up type-checking, and every semantic check
  M2 deferred (undeclared names, type mismatches, wrong arg counts,
  duplicate/missing `start`, a non-void function falling off the end).
  Stops on the first error (exit 65), same as the parser. See
  `docs/DECISIONS.md` for the full design rationale.
- `disassemble.h` / `disassemble.c`, `bc_dump.c` — `./build/bcdump
  file.wf` prints the compiled bytecode as a readable listing. Manual
  verification tool, not an automated test (those start with golden
  files once the VM exists to actually run anything).
