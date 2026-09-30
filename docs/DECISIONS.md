# WhiteFang Decisions Log

Rationale behind the non-obvious calls in `SPEC.md`, so we don't
relitigate them later. Ordered roughly by when each was decided.
Update this whenever a decision is revisited — don't just change
`SPEC.md` silently.

---

### ~~Transpile to C, not a compiler written in C~~ — SUPERSEDED

Original call: WhiteFang source compiles to generated `.c`, then built
with gcc/clang. Fastest path to a working pipeline, leans on C's
existing optimizer/backend. **Superseded by the bytecode/VM decision
below** — kept here for history, not current behavior.

### Compile to custom bytecode, executed by a VM (not transpile to C)

WhiteFang source compiles to a custom bytecode format; a VM (an
interpreter loop written in C) executes it directly. No gcc/clang in
the runtime path at all — the compiler and VM are both C programs, but
a WhiteFang program no longer becomes a standalone native binary.

Reasoning: there was no hard requirement to target C source specifically
(the project requirement is that the compiler/VM be *implemented* in C,
not that WhiteFang compile *to* C). A custom bytecode target is more
in the spirit of "build your own compiler" for a learning project than
delegating codegen, register allocation, and ABI handling to gcc.

Considered and rejected: emitting native machine code directly (own
register allocation + assembler/linker) — a much larger scope increase
(realistically weeks of added work) than the project's front-end
(lexer/parser/AST, unaffected by this decision either way) justified.

Known cost, accepted: interpreted bytecode is slower than gcc-compiled
native code — roughly 10-30x for a naive switch-dispatch stack VM,
improvable to ~3-10x with computed-goto dispatch and superinstructions,
without adding a JIT (out of scope). Worth revisiting if this repo ends
up benchmarking algorithms written in WhiteFang against native
baselines — irrelevant if the language itself is the deliverable.

Follow-on design choices, not yet made: stack-based vs. register-based
VM (leaning stack-based for simplicity — see conversation), bytecode
file format, opcode set, how function calls/locals map to VM call
frames. To be settled when M3 (codegen) design starts.

### Static typing, explicit annotations, no inference (M0)

Every variable and parameter has a declared type in M0; no `auto`-style
inference. Dynamic typing was ruled out early — it would need a
tagged-value runtime representation in the generated C, which is a lot
of machinery before the pipeline even runs "hello world." Explicit
static types transpile the most directly to C's own type system.
Inference may be worth adding later; not needed to get M0 working.

### Value types only, no heap in M0

M0 is deliberately limited to `int`/`float`/`bool`/`char` — no strings,
arrays, or structs. Heap allocation drags in an ownership/lifetime
story (manual free? GC? borrow rules?) that's a real design question on
its own, not something to decide as a side effect of shipping a lexer.
Deferred to the stretch goals (strings, arrays) once the core pipeline
works end-to-end.

### Variables: `let type: name = value;` (type before name)

Inverted from the more common `let name: type` order. A deliberate
uniqueness choice — this is a learning project and "looks like every
other C-family language" wasn't a goal for the variable form. Note it
reads in the *opposite* order from function params (`type name` in
§4), which is a real inconsistency — acceptable for now since both
forms individually read fine, but worth knowing if it ever causes
confusion in practice.

### `let` requires an initializer, no default/zero values

Bare `let int: a;` is a compile error. Removes an entire class of
"used before initialized" bugs from M0 without needing definite-
assignment analysis — the simplest correct answer for a first pass.

### ~~Multiple declarations: shared-initializer vs per-name forms~~ — SUPERSEDED

Original call: `let int: a, b, c = 5;` (all get 5) and
`let int: a = 5, b = 6, c = 7;` (each own value) both valid; mixing
the two in one declaration a parse error. Chosen because both forms
were genuinely useful shorthand and neither was a special case of the
other. **Superseded by "one variable per `let`" below** — kept here
for history, not current behavior.

### Variable declarations: exactly one name per `let` (multi-declare removed)

Each `let` now declares a single variable; the two multi-name forms
above are gone. Surfaced during M2 (parser) design, not a change made
for its own sake: implementing the old grammar required the parser to
read the first name, then branch on whether the *next* token was `,`
(shared-init form, keep collecting names until an `=` shows up) or `=`
(per-name form, commit to repeating `name = expr, name = expr, ...`) —
real lookahead-dependent parsing logic, plus a transient buffer to
hold the collected names, plus the "mixing is a parse error" rule,
which existed purely to patch the ambiguity *between* the two forms
rather than expressing anything about the language itself.

None of that complexity taught anything not already covered elsewhere
in the parser (precedence climbing and the assignment-vs-expression-
statement lookahead already exercise "resolve ambiguity with
lookahead"). Cost accepted: declaring several variables now takes
several `let` lines instead of one — a minor loss of shorthand, not a
loss of expressiveness. `Stmt`'s var-decl case is also simpler for it:
one name + one initializer expression, not an array of bindings.

### Functions: `func name(type param, ...) { body } -> returnType`

Return type trails the body instead of leading it (unlike C, Rust,
Swift). The standout distinctive choice in the language so far —
deliberately chosen for uniqueness. `func` keyword was dropped in an
early pass for terseness, then added back for readability once we saw
`add(int a, int b) { ... } -> int` in practice without it.

### `void` is always written explicitly, arrow never omitted

Every function ends in `-> type`, even when that type is `void` — no
shorthand where a missing arrow implies void. Chosen to keep the
parser rule uniform ("a function decl always ends in `-> type`")
rather than making the trailing arrow itself optional, which would
need a special case.

### `print` is a builtin function, not a statement keyword

`print(x);` uses ordinary call syntax rather than a dedicated `print
x;` statement form. Simpler grammar — one call-expression rule handles
both user functions and `print`, no separate statement production
needed just for output. (Still true after the rename below — `PRINT`
kept the call-shaped syntax; only the name and its lexer/AST treatment
changed.)

### `print` renamed `PRINT`, made a reserved keyword

Surfaced during M3's function-call-validation design: nothing stopped
a user writing `func print(...) { ... }`, and since `print`'s real
behavior (dispatching on argument type across `int`/`float`/`bool`/
`char`) can't be replicated by any user-defined function in this
language, allowing that name to be shadowed/redefined was more likely
to cause confusion than serve any real use case.

Rather than add a semantic check ("reject a function declared with
this specific name"), made `print` a genuine reserved keyword — lexed
specially, exactly like `_PI`/`_E`/`_G`/`_R2` — so the same guarantee
falls out of the grammar for free: the lexer never produces an
identifier token for it, so `func print(...)` fails to parse for the
same structural reason `let float: _PI = 3;` does. No semantic check
needed, same win as the earlier global-variables removal.

Naming: considered `_PRINT` for consistency with the constants'
leading-underscore convention, but a leading underscore immediately
before a call's `(` read poorly at the call site (`_PRINT(x);`).
Landed on plain uppercase instead — `PRINT` — which still reads as
clearly non-ordinary (no lowercase user identifier looks like it)
without the awkward punctuation-before-parenthesis. This means
WhiteFang now has two different builtin-marking conventions (leading-
underscore-uppercase for constants, plain uppercase for the one
builtin function) rather than one unified rule — accepted because a
function call is already visually distinguished from a bare value by
its trailing `(...)`, so the two categories don't need to look
identical to each other, only each needs to look distinct from
ordinary user-chosen names.

### Entry point is `start`, not `main`

`func start() { ... } -> int` is WhiteFang's entry point; it transpiles
to C's `main` under the hood. Another deliberate uniqueness choice —
`main` is such a strong C convention that keeping it would undercut the
"WhiteFang is its own thing" goal for a name that costs nothing to
change.

### Comments: `` /` `` for line, `` `` `` (two backticks) for block

Both chosen specifically to avoid every mainstream language's comment
syntax (`//`, `#`, `--`, `/* */`) — this went through several rounds
(C-style → `--`/`-->...<--` → reverted line comments to `//` → final
backtick-based forms) before landing here. Real implementation cost:
`/` now needs one-token lookahead to distinguish the line-comment
opener from the division operator, and block comments use an
open-token-equals-close-token design (ends at the *next* `` `` ``, no
nesting) rather than distinct open/close markers — simpler to lex than
matched-pair delimiters, at the cost of not being able to nest.

### Operators kept as standard C

`+ - * / % == != < > <= >= && || ! =` — considered word-based logical
ops and a `:=`/`=` assignment-equality split, but decided the
distinctiveness budget was better spent on comments/functions/entry
point; operators are high-traffic tokens where deviating from C has
the highest ongoing cost (every expression touches them) for
comparatively low novelty payoff.

### ~~Global variables added to M0 (not deferred)~~ — SUPERSEDED

Original call: globals as a small, purely additive extension on top of
the frame-relative locals design — a separate `globals[]` array, two
new opcodes, a second name-lookup tier. **Superseded by "global
variables removed" below** — kept here for history, not current
behavior.

### ~~Built-in global constants: `_PI`, `_E`, `_G`~~ — SUPERSEDED

Original call: `_PI`/`_E`/`_G` as three pre-populated entries in
`globals[]`, redeclaring one a compile error, `_PI = 4;` technically
legal since M0 has no immutability. **Superseded by "global variables
removed" below**, which turns these into reserved literal keywords
instead — kept here for history, not current behavior. The naming
rationale carries forward unchanged: leading underscore + uppercase so
they read as visibly distinct from ordinary identifiers; a `c` (speed
of light) constant was considered and dropped, not needed for the
course-algorithms use case that motivated this feature.

### Global variables removed; `_PI`/`_E`/`_G` are reserved constant-literal keywords

Surfaced during the M2 (parser) design pass, while working through
which semantic checks (duplicate names, builtin redeclaration, type
mismatches) belong in M2 vs. get deferred to M3. Global variables were
only ever added to M0 to support the three built-in constants (see the
superseded entries above) — nothing in the example program or any
real use case needed a user-declared global. Once that was named
explicitly, the better fix was visible: don't give the constants
variable storage at all.

`_PI`, `_E`, `_G` are now reserved keywords, lexed exactly like
`true`/`false` (added to the keyword table, `SPEC.md` §1), and the
parser turns each one directly into a float-literal AST node with the
fixed value baked in — indistinguishable, from the AST onward, from
writing the literal number itself. This is why they can never be
shadowed or redeclared: not because of a semantic check anywhere, but
because the lexer will never produce an identifier token for them in
the first place, the same reason `let float: true = 3;` doesn't
parse. A purely syntactic guarantee, not a semantic one — one less
check that would otherwise have needed a symbol table to enforce.

Consequences, all reductions in planned scope:
- No global variables as a language feature in M0 at all (`SPEC.md`
  §3) — every `let` must be inside a function body.
- `docs/VM.md` §5 (global variables), the `GET_GLOBAL`/`SET_GLOBAL`
  opcodes, the two-tier local-then-global name lookup, and the
  `globals_init`/`global_count` `Program` fields are all removed —
  see `docs/VM.md` for the updated (simpler) design.
- The AST's top level (M2) no longer needs a `Decl`/`DeclKind` wrapper
  distinguishing function vs. global-variable declarations — a
  `Program` is just a list of function declarations.
- The "no const/mut, so `_PI = 4;` is technically legal" caveat from
  the superseded entry above is gone outright: `_PI = 4;` is now a
  syntax error, not a runtime footgun.

### Added `_R2` (√2) as a fourth built-in constant

`_R2 = 1.414213562373095` (√2), added alongside `_PI`/`_E`/`_G` via
the exact same mechanism — a reserved keyword (`SPEC.md` §1) the
lexer recognizes and the parser converts directly into an
`EXPR_FLOAT_LIT` node (see "Global variables removed" above). No new
design decision here, just applying the established pattern to a
fourth value.

### Dockerize the VM — done

Built once M3 produced a working binary, as planned. `Dockerfile`:
a build stage (`alpine` + `gcc`/`musl-dev`/`make`) runs `make
whitefang`; the runtime stage is bare `alpine` plus just that
compiled binary — ~12MB total. `docker build -t whitefang .`, then
`docker run --rm -v "$PWD":/work whitefang path/to/program.wf` (the
repo root mounted at `/work` so a host-relative path works
unchanged). Verified against a passing program (`fib.wf`, correct
output) and a failing one (`missing_start.wf`, correct exit 65) — both
behave identically to the native build.

Reasoning unchanged from the original call: the binary is
dependency-free, so this was low effort and low risk; value is
reproducible build/execution environment and easy distribution/demo,
plus a deliberate side-learning goal given the user's standing
interest in containerization — not solving a portability problem C
didn't already have.

Resolved along the way: the "wrap just the VM vs. the whole pipeline"
question this entry originally left open turned out to be moot.
There's no separate on-disk bytecode format (`docs/VM.md` §7 defers
it) and no VM-only binary — `whitefang` already does lex → parse →
compile → execute in one process, so there was only ever one thing to
containerize.

`scratch` (rather than `alpine`) was considered for the runtime image
for a smaller footprint, but requires a fully static binary — which
would mean adding `-static` to the project's build flags, a change to
the core `Makefile` (affecting every build, not just Docker's) for a
size saving that didn't seem worth that coupling. `alpine`'s musl libc
runs the dynamically-linked binary as-is, no build changes needed, at
an already-small ~12MB.

### Build tooling: plain Makefile, C11, `-Wall -Wextra -Werror`

Settled once M1 actually needed it, per the standing "decide when it's
needed, not speculatively" rule. A hand-written `Makefile` rather than
CMake/etc — the build is small (a handful of `.c` files, no external
deps) and a generator adds indirection without paying for itself yet;
revisit if the project outgrows it. `-Werror` on from the start so
warnings can't silently accumulate across M1-M3. `cc`/`std=c11` rather
than pinning gcc/clang specifically — no reason to require one over
the other yet.

### Lexer: source-slice tokens, no per-token allocation

`Token` holds a `(start, length)` pointer into the original source
buffer plus type and line — not an owned/copied string. Standard
technique for a single-pass scanner (same approach as clox in
*Crafting Interpreters*): avoids allocating per token, at the cost of
requiring the source buffer to outlive every token derived from it.
Acceptable since the compiler pipeline reads a whole file into memory
up front and keeps it alive for the duration of that compile anyway.

### Char literal escapes: `\n \t \r \\ \' \0`

`docs/SPEC.md` §2 only specified the bare `'a'` form for char literals
and didn't say whether escapes exist. Decided yes — a char type that
can't represent a newline or backslash is too limited to be useful
(e.g. can't `PRINT()` a newline char), and this is cheap to lex.
Picked the common C-family escape set, minus anything not meaningful
without strings (`\"` isn't needed since there's no string type yet).
Flagged here rather than silently assumed; `SPEC.md` §2 updated to
match. Revisit if a case needs an escape not in this list.

### Lexer error tokens carry a static message, not an error code

`TOKEN_ERROR`'s `(start, length)` point at a static diagnostic string
literal instead of a slice of the source — reuses the same `Token`
shape rather than adding a separate error-reporting path, at the cost
of `Token` overloading what `start`/`length` mean depending on type.
Good enough for M1 (a `lexdump` CLI tool is the only consumer so far);
revisit if the parser needs richer diagnostics (e.g. column numbers,
suggested fixes) in M2.

### Added `INEG`/`FNEG` opcodes (gap found in `VM.md`'s original opcode set)

`VM.md`'s opcode table (sec 6) covered `!` (`NOT`) but never gave unary
`-` an opcode at all, even though `SPEC.md` §5 and the AST (`UN_NEG`)
both already treat it as a real operator. Found while implementing
expression codegen in M3. Fix: add `INEG`/`FNEG`, symmetric with every
other operation already being type-specialized (`IADD`/`FADD`, etc.) —
not a judgment call, just completing an already-established pattern,
so applied directly rather than raised as an open question.

### Missing-return check is syntactic, not full control-flow analysis

A non-void function must have a `return expr;` as the literal last
statement in its top-level body — checked once, after compiling the
body, with no walking into `if`/`while` branches to look for it.
Surfaced during M3 compiler design: without *some* check, a function
that falls off the end without returning produces undefined behavior
at runtime (whatever garbage is on the stack becomes the "return
value"), which cuts against M0's general pattern of rejecting likely
mistakes outright (required `let` initializers, no implicit
int/float mixing, etc.).

Accepted limitation: this rejects some functions that always return in
practice but not as their literal last statement, e.g. an `if`/`else`
where *both* branches return and nothing follows it — the check can't
see into the branches to know that. Full definite-return analysis
(proving every control-flow path returns) would handle that case
correctly, but is real added machinery — noted here as a candidate
future improvement, not built now, per the "smallest subset that
works" pattern used throughout M0.

### Dead-code cleanup: no bookkeeping emitted after an unconditional `return`

`end_scope`'s `POP`/`POP_N` block-cleanup and `if`/`else`'s "jump over
the else branch" are both skipped when the block in question's last
statement is already a `return` — `RETURN`/`RETURN_VOID` already
truncate the stack to `frame_base` and transfer control away
unconditionally (`docs/VM.md` sec 4, "return bypasses block
bookkeeping entirely"), so anything the compiler would otherwise emit
right after one is dead: physically present in the chunk, but never
reached at runtime, since control has already left. Found by comparing
`bcdump`'s output against `docs/VM.md`'s own worked trace for
`start` — the trace has no trailing `POP` after its final `RETURN`,
and the first version of the compiler emitted one. Purely a cleanliness
fix, not a correctness one: the dead instructions were harmless
either way, just wasted bytes and confusing disassembly output.

### VM: `start` is treated as an initial synthetic "call" (frame 0)

Rather than special-case "the outermost call has no caller to return
to," `vm_run` seeds the call-frame stack with one frame already on it
(`frame_base = 0`, `return_chunk`/`return_ip` unused) before executing
`start`. Every `RETURN`, including `start`'s own, then follows
identical logic: truncate to the current frame's `frame_base`, push
the return value, pop the frame, and check whether the frame count
just reached zero — if so, halt using that return value as the process
exit code (`docs/VM.md` sec 6, "No `HALT` opcode"); otherwise restore
the caller's chunk/ip/`frame_base` from the frame that's now on top.
No branch anywhere for "is this the first call" — it falls out of the
same bookkeeping every other return already needs.

### VM sizing and runtime error handling

- Operand stack: 65536 `Value` slots; call-frame stack: 256 deep (the
  starting point `docs/VM.md` sec 4 suggested). Both fixed, matching
  the "generously-sized fixed stack, no precomputed per-chunk max"
  choice in `docs/VM.md` sec 7 — confirmed generous enough by testing
  200 levels of recursion (succeeds) against 1000 (cleanly reported as
  stack overflow, not a segfault).
- Runtime errors (stack overflow, integer division by zero) exit with
  status 70, distinct from the compiler/parser's 65 — these are
  failures at run time, not mistakes in the source.
- Integer division/modulo by zero is checked explicitly and reported
  as a clean runtime error, since C's own behavior there is undefined
  (crash territory) — consistent with the project's general preference
  for a clean error over undefined behavior. Float division by zero is
  *not* checked: IEEE 754 defines it (`+-inf`/`nan`), so there's
  nothing unsafe to guard against.
- `PRINT` always appends a trailing newline; `PRINT_FLOAT` uses `%g`
  formatting (e.g. `_PI` prints as `3.14159`, 6 significant figures,
  not its full internal precision). Neither is specified in `SPEC.md`
  — implemented as the obvious default rather than raised as an open
  question; revisit if a program ever needs different formatting.

### Golden-file test design

Format settled: flat `<name>.wf` + `<name>.expected` pairs directly in
`tests/golden/` (not a directory per test) — nothing in M0 needs a
test to span multiple source files, so the extra nesting would buy
nothing. Both stdout *and* exit code are checked, not just stdout as
first sketched when this milestone plan was written — exit code is
real observable behavior (`start`'s return value) and a print-less
test would have nothing else to check. Exit code defaults to `0` so
only tests that intentionally return something else need a
`.exitcode` file.

The runner (`tests/run_golden.sh`) is a bash script, not a C program —
looping over files, running a subprocess, diffing text, and tallying
results is exactly what a shell script is for, and writing it in C
would mean reimplementing `diff` and `for` badly. It supports
`--record` to (re)generate the reference output, so adding a test or
absorbing a deliberate behavior change never means hand-editing an
expected-output file.

Compile-time and runtime error paths (`tests/golden/errors/`) are
golden-tested too, but only their exit code (65 compile error, 70
runtime error) is checked — not the exact stderr message text.
Considered pinning the message text as well (stronger regression
coverage, doubles as documentation of exact error wording), but
rejected: it would mean every future wording improvement to an error
message breaks tests unrelated to the actual bug being fixed. Exit
code alone still locks in "this program is correctly rejected/crashes
for the right general reason," which is the part actually worth
protecting against regression.

### Stretch goals: strings, arrays, `for` loops (post-M0)

Called out explicitly as the *next* targets after M0, ahead of
structs/generics/closures/modules/GC (which stay unscheduled). All
three share a dependency: strings and arrays both need the
heap/ownership decision M0 deferred, and `for` loops are sequenced
after/alongside arrays since iterating over a collection is the
motivating use case.
