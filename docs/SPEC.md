# WhiteFang Language Spec — M0 (Core)

Status: all open questions resolved. Grammar/semantics below are settled
for M0; still open to redlining anything as implementation surfaces
issues.

M0 scope: the smallest subset that can compile and run a real program.
No heap allocation, no strings/arrays/structs yet — those come in a later
milestone. Goal is: variables, arithmetic, functions, control flow,
printing primitives, compiled to bytecode and executed by a VM.

Key decisions:
- Compiles to a custom bytecode format, executed by a VM (interpreter)
  written in C — not transpiled to C source (see DECISIONS.md)
- Static typing, explicit annotations (no inference in M0)
- Value types only, no heap in M0
- Functions are `func name(type param, ...) { body } -> returnType`
- Variables are `let type: name = value;`

---

## 1. Lexical structure

**Comments**:
```
/` line comment
``multi-line comment, no nesting``
```
Line comments open with `` /` `` (forward-slash then backtick). Block
comments use the same two-backtick token (`` `` ``) to open and close —
a block comment ends at the *next* occurrence of `` `` `` after it
opens (no nesting).

Lexer notes:
- `/` is also the division operator (§5), so tokenizing `/` requires
  one character of lookahead: `` /` `` → line comment, bare `/` →
  division.
- A single backtick with no second backtick immediately following is
  not currently a valid token on its own in M0 (no other construct uses
  a lone backtick).

**Identifiers**: `[a-zA-Z_][a-zA-Z0-9_]*`, case-sensitive.

**Whitespace**: not significant except to separate tokens (no
indentation rules — blocks use braces, see below).

**Statement termination**: every statement ends in `;`.

**Blocks**: delimited by `{` `}`.

**Keywords (M0)**: `func let if else while return true false int float
bool char void`

---

## 2. Types (M0)

| Type    | C equivalent | Notes                          |
|---------|--------------|---------------------------------|
| `int`   | `int32_t`    | 32-bit signed                   |
| `float` | `double`     | 64-bit                          |
| `bool`  | custom       | `true` / `false` literals       |
| `char`  | `char`       | single character, `'a'` literal |

No implicit conversions in M0 — `int` and `float` do not mix without an
explicit cast (cast syntax TBD, not needed until a test case demands it).

---

## 3. Variable declarations

`let`, then type, then `:`, then name — type comes before the name
(opposite order from the function param style in §4, which is
`type name`):

```
let int: a = 5;
let float: pi = 3.14159;
let bool: ready = true;
```

**Multiple declarations in one `let`**, sharing a type, in two forms:

Shared initializer (one `=`, applies to every name in the list):
```
let int: a, b, c = 5;      /` a = 5, b = 5, c = 5
```

Per-name initializer (each name has its own `=`):
```
let int: a = 5, b = 6, c = 7;
```

Mixing the two forms in one declaration (e.g. `a, b = 5, c = 7`) is not
defined in M0 — not needed yet, disallow as a parse error.

Declaration without initializer: **not allowed.** Every `let` requires
an initializer; `let int: a;` is a compile error in M0 (no default/zero
values).

Reassignment:
```
x = 10;
```

No `const`/`mut` distinction in M0 (everything declared with `let` is
reassignable). Immutability can be a later milestone if wanted.

---

## 4. Functions

Declared with a `func` keyword, then name, params are C-style typed
(`type name`), and the return type trails the body as `-> type`:

```
func add(int a, int b) { return a + b; } -> int

func greet() { print(42); } -> void
```

- Params are always explicitly typed (`int a`, not `a: int`).
- Every function declares a return type via the trailing `-> type`.
- `void`: every function writes `-> void` explicitly when it has no
  return value — the arrow is never omitted, keeping the parser rule
  uniform ("every function decl ends in `-> type`").
- Program entry point is `func start() { ... } -> int` — the VM begins
  execution here; its return value becomes the process exit code.
- No default arguments, no overloading, no varargs in M0.

---

## 5. Expressions & operators

Arithmetic: `+ - * / %` (int and float; `%` int only)
Comparison: `== != < > <= >=`
Logical: `&& || !`
Assignment: `=` (statement, not expression — `x = y = 1;` not allowed in M0)
Grouping: `( )`
Precedence: standard C precedence for the above operators.

---

## 6. Statements

**If / else:**
```
if (x > 0) {
    print(x);
} else {
    print(0);
}
```
`else if` via nested `if` in the `else` branch, standard C-style chaining:
```
if (x > 0) {
    print(1);
} else if (x < 0) {
    print(-1);
} else {
    print(0);
}
```

**While:**
```
while (x > 0) {
    x = x - 1;
}
```

**Return:**
```
return expr;   /` non-void function
return;        /` void function
```

**Print** — M0 needs some way to observe output for tests. `print` is a
builtin function (not a dedicated statement keyword), so it composes
with the existing function-call grammar.
```
print(x);       /` int, float, bool, or char
```
Codegen maps it to the right `printf` format specifier per argument
type (resolved at compile time since types are static).

No `for` loop in M0 (arrives with arrays, since a range/iterator story
makes more sense once there's something to iterate). `while` covers the
core case for now.

---

## 7. Example program

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

---

## 8. Explicitly out of scope for M0

Deferred to later milestones: strings, arrays, `for` loops, structs,
generics, closures/first-class functions, modules/imports, error
handling, GC/ownership, standard library beyond `print`.

## 9. Stretch goals (post-M0)

Called out as the concrete next targets after M0 lands, ahead of
anything else in §8:

- **Strings** — needs a memory-model decision M0 deliberately deferred
  (heap allocation, ownership/lifetime story). Design TBD when we get
  here.
- **Arrays** — same heap/ownership question as strings; also needs
  indexing syntax and bounds-check behavior decided.
- **`for` loops** — natural pairing with arrays (iterate over an array),
  so likely sequenced right after/alongside arrays; syntax TBD.

Everything else in §8 (structs, generics, closures, modules, GC,
broader stdlib) stays unscheduled beyond "later."

---

## Open questions summary (for quick redlining)

1. ~~Comments~~ **Resolved:** `` /` `` for line comments, two backticks
   (`` `` ``) as the open/close token for block comments.
2. ~~Function syntax~~ **Resolved:** `func name(type param, ...) { body } -> returnType`,
   return type trails the body.
3. ~~Variable syntax~~ **Resolved:** `let type: name = value;`
   (e.g. `let int: a = 5;`).
4. ~~Declaration without initializer~~ **Resolved:** disallowed, no
   default values — every `let` requires an initializer.
5. ~~`void` return type~~ **Resolved:** every function writes an
   explicit `-> void`, arrow is never omitted.
6. ~~`print`~~ **Resolved:** builtin function, not a dedicated
   statement keyword.
7. ~~Multiple declarations~~ **Resolved:** `let type: a, b, c = 5;`
   (shared init) and `let type: a = 5, b = 6, c = 7;` (per-name init)
   are both valid; mixing the two forms is a parse error in M0.
