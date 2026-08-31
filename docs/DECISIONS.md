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

### Multiple declarations: shared-initializer vs per-name forms

`let int: a, b, c = 5;` (all get 5) and `let int: a = 5, b = 6, c = 7;`
(each own value) are both valid; mixing the two forms in one
declaration is a parse error. Chosen because both forms are genuinely
useful shorthand and neither is a special case of the other — but
combining them (`a, b = 5, c = 7`) has no obvious meaning (does `5`
apply to `a` and `b`, or just `b`?), so it's excluded rather than
guessed at.

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
needed just for output.

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

### Stretch goals: strings, arrays, `for` loops (post-M0)

Called out explicitly as the *next* targets after M0, ahead of
structs/generics/closures/modules/GC (which stay unscheduled). All
three share a dependency: strings and arrays both need the
heap/ownership decision M0 deferred, and `for` loops are sequenced
after/alongside arrays since iterating over a collection is the
motivating use case.
