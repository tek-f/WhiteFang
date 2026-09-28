# WhiteFang VM & Bytecode Design — M3

Status: design complete for M0's language surface (see `SPEC.md`).
Nothing here is implemented yet — this is the design that M3's actual
code (`src/bytecode/`, `src/vm/`) should follow. Update this doc first
if a decision here needs to change, same convention as `SPEC.md`.

Companion docs: `SPEC.md` (language syntax/semantics — the *what*),
`DECISIONS.md` (rationale log — the *why*, including why bytecode/VM
instead of transpiling to C). This doc is the *how* of the bytecode
compiler and VM specifically.

---

## 1. Execution model

Stack-based, not register-based: a single **operand stack** that every
instruction pushes results to / pops operands from. No numbered
registers. Compiling an expression is a straightforward recursive
walk — e.g. `a + b * c` compiles as: compile `a` (pushes it), compile
`b * c` (pushes it), emit `ADD`. The ordering of pushes/pops does all
the bookkeeping a register allocator would otherwise need to do.

Cost: every intermediate value round-trips through the stack array
instead of staying in a CPU register — the main source of the
~10-30x-slower-than-native performance gap accepted when bytecode/VM
was chosen over transpiling to C (see `DECISIONS.md`).

## 2. Value representation

WhiteFang is statically typed (locked in, `SPEC.md`), so the VM never
needs to ask "what type is this value?" at runtime — the compiler
already resolved every expression's type, and encodes that choice in
*which opcode* it emits (`IADD` vs `FADD`, etc.), not in the value
itself. This means values are **untagged**.

```c
typedef union {
    int32_t i;
    double  f;
} Value;
```

Uniform 8-byte slots regardless of logical type. `bool` (0/1) and
`char` (its ordinal value) both live in the `.i` field — physically
indistinguishable from `int`. This is why `int`, `bool`, and `char` all
share one family of opcodes (`IADD`, `IEQ`, etc.) rather than each
needing their own: the opcode set operates on physical representation,
and it's the compiler's job (type-checking, not built yet as its own
named phase — folded into bytecode compilation) to only ever emit
`IADD`/`IEQ`/etc. for source-level combinations that are actually
legal.

Explicitly not done: tagged values (`{ TypeTag tag; union {...}; }` per
slot). Would matter if WhiteFang ever grew dynamic typing; pure
overhead given static typing already guarantees correctness before
codegen runs.

## 3. Constant pool

Every literal (`int`/`float`/`char`) that isn't `true`/`false` lives in
a **per-function constant pool** — an array of `Value` attached to that
function's chunk (see §7). Instructions reference a literal by a
1-byte index (`PUSH_CONST <idx>`), not by embedding its raw bytes
inline in the instruction stream. This includes `_PI`/`_E`/`_G` (§5) —
by the time the compiler sees one, it's already an ordinary
`EXPR_FLOAT_LIT` AST node, so it takes the exact same path into the
pool as any other float literal. No special-casing needed anywhere in
codegen for the built-in constants.

Chosen over embedding literals as immediate operands specifically
because **strings are a committed stretch goal** (`SPEC.md` §9) and
can't be immediates at all (variable length) — building the pool
mechanism now means strings slot into the same `PUSH_CONST`-style
mechanism later instead of needing an entirely separate one. Full
pros/cons in `DECISIONS.md`.

- **One opcode for all pool types.** `PUSH_CONST` doesn't need
  int/float variants — it just pushes whatever bits sit in that slot;
  correctness comes from the compiler having already picked
  type-appropriate downstream opcodes.
- **`true`/`false` skip the pool entirely** — dedicated zero-operand
  `PUSH_TRUE`/`PUSH_FALSE` opcodes, since there are only two possible
  values ever.
- **Pool scope is per-function** ("chunk"), matching Python's
  `co_consts` / Lua's per-function constant tables — not one global
  pool shared across the whole program.
- **No deduplication** — the same literal appearing twice gets two
  pool entries. Only matters for strings later; irrelevant at fixed
  int/float/char widths. Cheap optimization to add later if wanted.
- **Index width**: `uint8_t` (256 constants/function) — plenty for
  M0-scale functions; widen to `uint16_t` later if a function ever
  needs more (mechanical change, not a design change).

## 4. Call frames & function calls

### Locals live on the operand stack

Parameters and local variables *are* stack slots, addressed relative
to the current call's **frame base** — `LOAD_LOCAL n` /
`STORE_LOCAL n` mean `stack[frame_base + n]`. No separate locals
storage.

- **Params need no copying.** The caller has already pushed each
  argument (evaluating them left-to-right) by the time `CALL`
  executes. `CALL` just sets `new_frame_base = stack_top - arg_count`
  — those slots become the callee's locals 0..N-1 in place.
- **A fresh `let` pushes a new slot** at compile-time, giving it the
  next sequential index. **Reassignment (`x = expr;`) instead computes
  the value and `STORE_LOCAL`s into the *existing* slot** — net zero
  stack growth. This distinction matters: if reassignment also pushed
  a new slot, a loop with `i = i + 1;` in its body would grow the
  stack unboundedly every iteration.

### Blocks must clean up their own locals

If a `let` inside an `if`/`while` block isn't undone when that block
exits, two things break: (1) code after the block gets an
inconsistent slot number depending on which runtime branch ran, and
(2) a loop body with a `let` inside it grows the stack every
iteration without bound.

**Fix:** every block tracks how many locals existed when it was
entered. On exit (falling through, or looping back on a `while`'s
back-edge), the compiler emits `POP_N <count>` for exactly the locals
declared inside that block, restoring the stack height it had before
the block started. This keeps slot numbers valid regardless of which
branch ran or how many iterations occurred.

### `return` bypasses block bookkeeping entirely

`return` can fire from arbitrarily nested blocks. Rather than emitting
precise per-scope pops on that path, it's a blunt safety net:

- **`RETURN`** (`return expr;`): pop the return value, truncate the
  stack straight to `frame_base` (discarding everything above it,
  regardless of nesting), push the return value back — now onto the
  *caller's* stack.
- **`RETURN_VOID`** (`return;`): truncate to `frame_base`, no value
  shuffling.

### Call stack discipline

Every non-void `CALL` leaves exactly one value (the return value) on
the caller's stack once it completes; void `CALL`s leave nothing
extra. Known at compile time from the callee's declared return type.
A non-void call used as a bare statement (`fib(i);` with the result
discarded) gets a `POP` emitted right after it; void calls used as
statements need nothing extra.

### The call-frame stack

A separate small array, `frames[]`, records per active call: return
chunk + return instruction pointer, and `frame_base`. `CALL` pushes an
entry and switches the VM to the callee's chunk; `RETURN`/
`RETURN_VOID` pop it and switch back. Fixed max depth — suggest 256 to
start — exceeded depth is a clean runtime "stack overflow" error
rather than a segfault.

### Why recursion just works

Each call gets its own `frame_base`, further up the shared operand
stack than its caller's. `fib(n-1)` and `fib(n-2)` never see each
other's locals because `LOAD_LOCAL`/`STORE_LOCAL` are always relative
to whichever frame is currently active — nothing recursion-specific
needed; it falls out of frame-relative addressing for free.

## 5. ~~Global variables~~ — removed

M0 briefly added global variables (a `globals[]` array, `GET_GLOBAL`/
`SET_GLOBAL` opcodes, two-tier local-then-global name resolution) to
give `_PI`/`_E`/`_G` somewhere to live. That's no longer needed: the
built-in constants are now reserved keywords that the parser converts
directly into float-literal AST nodes (see `DECISIONS.md`, "Global
variables removed") — by the time the compiler sees them, they're
indistinguishable from writing the literal number, so there's nothing
for the VM to store or look up. There are no global variables in M0 at
all; every name the compiler resolves is a local (§4).

This section is kept as a stub (not deleted) per the doc's usual
convention for superseded design — see `DECISIONS.md` for the full
reasoning. Section numbers below are left as-is rather than
renumbered, to avoid a wave of unrelated cross-reference churn.

## 6. Opcode set

| Category | Opcodes | Notes |
|---|---|---|
| Constants | `PUSH_CONST <u8 idx>`, `PUSH_TRUE`, `PUSH_FALSE` | §3 |
| Stack manipulation | `POP`, `POP_N <u8 count>`, `DUP` | `POP_N` for block cleanup (§4), `DUP` for short-circuit (below) |
| Locals | `LOAD_LOCAL <u8 idx>`, `STORE_LOCAL <u8 idx>` | frame-relative (§4) |
| Int-family arithmetic | `IADD` `ISUB` `IMUL` `IDIV` `IMOD` `INEG` | `int` only — arithmetic on `bool`/`char` isn't legal (`SPEC.md` §5); see M3 type-checking design in `DECISIONS.md` |
| Float arithmetic | `FADD` `FSUB` `FMUL` `FDIV` `FNEG` | no `FMOD` — `%` is int-only per `SPEC.md` §5 |
| Int-family comparison | `IEQ` `INE` `ILT` `IGT` `ILE` `IGE` | covers int/bool/char (§2) |
| Float comparison | `FEQ` `FNE` `FLT` `FGT` `FLE` `FGE` | |
| Logical | `NOT` | unary `!` only — no `AND`/`OR` opcode, see below |
| Control flow | `JUMP <u16 offset>`, `JUMP_IF_FALSE <u16 offset>`, `JUMP_IF_TRUE <u16 offset>`, `LOOP <u16 offset>` | `JUMP`/`JUMP_IF_*` always forward, `LOOP` always backward (subtracts) — avoids signed-offset encoding |
| Functions | `CALL <u8 func_idx> <u8 arg_count>`, `RETURN`, `RETURN_VOID` | `func_idx` indexes the program-wide function table (§7), separate from any function's own constant pool |
| I/O | `PRINT_INT`, `PRINT_FLOAT`, `PRINT_BOOL`, `PRINT_CHAR` | type-specialized, same reasoning as arithmetic |

**No `AND`/`OR` opcodes.** `&&`/`||` short-circuit (confirmed,
matching C), which naive binary opcodes can't do — they'd always
evaluate both sides. Compiled instead via jumps:
```
-- a && b:
compile(a)
DUP                  ; keep a copy of a's value
JUMP_IF_FALSE end    ; if a is false, skip b — a's value is the result
POP                  ; a was true, discard the duplicate
compile(b)           ; b's value becomes the result
end:
```
`||` mirrors this with `JUMP_IF_TRUE`.

**No `HALT` opcode.** When `RETURN` pops the outermost frame (the
call-frame stack becomes empty — happens when `start` itself returns),
the VM recognizes there's no caller to resume and halts, using the
value that was about to be returned as the process exit code. Handled
as a condition in the `RETURN` handler, not a separate instruction.

## 7. Chunk & program format (in-memory)

Compiler and VM run in the same process for M0 — read `.wf` source →
lex → parse → compile directly into these structs → hand straight to
the VM → execute. No serialization/file format needed for that to
work end-to-end (see "Deferred" below).

```c
typedef struct {
    uint8_t*    code;
    int         code_count;
    Value*      constants;      /` this function's constant pool (§3)
    int         constant_count;
    int         arity;          /` debug aid / sanity check, not load-bearing —
                                    CALL's own arg_count operand is what the
                                    VM actually uses for frame setup
    const char* name;           /` for error messages & disassembly
} Chunk;

typedef struct {
    Chunk* functions;    /` one chunk per function, indexed by CALL's func_idx
    int    function_count;
    int    start_index;  /` which functions[] entry is `start`
} Program;
```

Deliberately **not** tracked: a precomputed `max_stack` per chunk (peak
operand-stack depth). Some VMs (e.g. the JVM class file format) store
this for pre-sizing/safety-checking. For M0, use one generously-sized
fixed operand stack shared by the whole program instead — cheap to add
precise per-chunk bounds later if it's ever actually needed.

**Startup sequence:**
1. Push a frame for `functions[start_index]`, `frame_base = 0`, begin
   executing.
2. `start`'s `RETURN` empties the call-frame stack → halt, using the
   returned value as the process exit code.

**Deferred: on-disk bytecode file format.** Nothing in M0 needs
compiled bytecode to outlive the process that produced it. A real file
format (magic-number header, versioning, endianness handling, a
section per chunk) would matter if a "compile once, run many times"
workflow or a VM-only Docker image (see `DECISIONS.md`, "Dockerize the
VM") is ever built — at that point it's a mechanical extension of the
structs above, not a redesign. Not building it now since nothing
currently needs it.

## 8. Worked example: `fib` / `start`

From `SPEC.md` §7:
```
func fib(int n) {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
} -> int

func start() {
    let int: i = 0;
    while (i < 10) {
        print(fib(i));
        i = i + 1;
    }
    return 0;
} -> int
```

### `fib` chunk

Arity 1 (`n` = local slot 0). Constants (no dedup, per §3):
`[0]=1  [1]=1  [2]=2`.

```
0:  LOAD_LOCAL 0        ; push n
1:  PUSH_CONST 0        ; push 1
2:  ILE                 ; n <= 1
3:  JUMP_IF_FALSE 6      ; false -> skip the "return n" branch
4:  LOAD_LOCAL 0        ; push n
5:  RETURN               ; return n
6:  LOAD_LOCAL 0        ; push n         } fib(n-1)
7:  PUSH_CONST 1        ; push 1         }
8:  ISUB                ; n - 1          }
9:  CALL fib, 1         ; fib(n-1)       }
10: LOAD_LOCAL 0        ; push n         } fib(n-2)
11: PUSH_CONST 2        ; push 2         }
12: ISUB                ; n - 2          }
13: CALL fib, 1         ; fib(n-2)       }
14: IADD                ; fib(n-1) + fib(n-2)
15: RETURN               ; return sum
```
(Instruction indices shown for readability — real encoding uses byte
offsets for jump targets, since operand widths vary per opcode.)

### `start` chunk

Arity 0. `i` = local slot 0. Constants: `[0]=0  [1]=10  [2]=1  [3]=0`.

```
0:  PUSH_CONST 0        ; push 0 — this push *is* declaring local i (slot 0)
LOOP_START:
1:  LOAD_LOCAL 0        ; push i
2:  PUSH_CONST 1        ; push 10
3:  ILT                 ; i < 10
4:  JUMP_IF_FALSE LOOP_END
5:  LOAD_LOCAL 0        ; push i          } fib(i)
6:  CALL fib, 1         ; fib(i)          }
7:  PRINT_INT            ; print(fib(i)) — pops & prints, no extra POP needed
8:  LOAD_LOCAL 0        ; push i          } i + 1
9:  PUSH_CONST 2        ; push 1          }
10: IADD                ; i + 1           }
11: STORE_LOCAL 0       ; i = i + 1  (overwrites slot 0 — not a new slot)
12: LOOP LOOP_START      ; back edge
LOOP_END:
13: PUSH_CONST 3        ; push 0
14: RETURN               ; return 0
```
No `POP_N` here — this loop body declares no `let` of its own, so
there's nothing to clean up per iteration. If it did, a `POP_N`
matching that count would appear right before the `LOOP` instruction.

### Program layout for this example

```
functions[0] = fib   (arity 1)
functions[1] = start (arity 0)
start_index  = 1
```

### Trace: first loop iteration

1. Push a frame for `start` (`frame_base = 0`), begin executing.
2. `PUSH_CONST 0` → stack `[0]` (this is `i`, at absolute position 0).
3. Loop condition: `0 < 10` → true, no jump.
4. `LOAD_LOCAL 0` → stack `[0, 0]` (`i`'s value pushed again as fib's argument).
5. `CALL fib, 1` → `new_frame_base = 2 - 1 = 1` (the pushed `0` becomes
   fib's local slot 0, i.e. `n`). Pushes a frame record (return chunk
   = `start`, return ip = 7, `frame_base = 1`). Switches to `fib`,
   `ip = 0`.
6. Inside `fib`: `n = 0`, `0 <= 1` true → falls through to
   `LOAD_LOCAL 0; RETURN`. `RETURN` pops the return value (`0`),
   truncates to `frame_base = 1` (discarding `n`'s slot), pushes `0`
   back. Stack is `[0, 0]` again — but the second `0` is now `fib(0)`'s
   *result*, not `n`. Pops the call frame, restores `ip = 7` in
   `start`.
7. Back in `start`: `PRINT_INT` pops that `0` and prints it. Stack back
   to `[0]` — just `i`.
8. `i = i + 1` computes `1`, `STORE_LOCAL 0`s it — stack stays `[1]`,
   same slot, new value.
9. `LOOP` jumps back to `LOOP_START`; the cycle repeats with `i = 1`.

Every recursive call inside `fib` (for `i >= 2`) works identically —
each nested `CALL` gets its own `frame_base` further up the same
stack, and each `RETURN` unwinds exactly one level, regardless of
recursion depth. Nothing beyond what's shown here is needed for
`fib`'s full recursive call tree.
