# WhiteFang

## Submission Details

The video walkthrough and project report can both be found in the folder 'submission/'.
To run the code, see the section "Running it with Docker". Otherwise see the section "Building natively".
If available, I would recommnend running using docker.

## Overview

WhiteFang is a small, statically-typed, C-family programming language,
built as a project for an Advanced Algorithms course. It compiles to a
custom bytecode format and runs on a hand-written virtual machine —
lexer, parser, bytecode compiler, and VM are all written in C, and a
WhiteFang program never depends on an external compiler at runtime.

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
        PRINT(fib(i));
        i = i + 1;
    }
    return 0;
} -> int
```

## Running it with Docker

No local toolchain required beyond Docker itself:

```
docker build -t whitefang .
docker run --rm -v "$PWD":/work whitefang tests/golden/fib.wf
```

The image mounts the repo root at `/work`, so any WhiteFang program's
path is given relative to wherever `$PWD` is when you run it — the
command above works unchanged from the repo root. The build is
multi-stage (an Alpine stage compiles the interpreter, the runtime
stage ships only the resulting binary), producing a ~12MB image.

## Building natively

```
make
```

produces four binaries under `build/`: `whitefang` (the interpreter)
and three manual-verification tools described below. Requires a C11
compiler and `make`; no other dependencies.

```
./build/whitefang tests/golden/fib.wf
```

runs a program end to end — lex, parse, compile, execute, all in one
process — and exits with that program's own `start` return value.

## The pipeline, and a debug tool for each stage

WhiteFang source becomes a running program in four stages, and each
one has its own small command-line tool for inspecting its output in
isolation — useful for seeing exactly what a given piece of source
turns into at each step, and how each tool was verified while it was
being built.

**Lexer** — turns source text into a stream of tokens.
```
./build/lexdump tests/golden/fib.wf
```
```
   1  FUNC         'func'
   1  IDENTIFIER   'fib'
   1  LPAREN       '('
   ...
```

**Parser** — turns the token stream into an abstract syntax tree.
```
./build/parsedump tests/golden/fib.wf
```
```
Program
  FuncDecl fib(int n) -> int
    Block
      If
        Binary <=
          Identifier n
          IntLit 1
      ...
```

**Bytecode compiler** — resolves names and types, then emits
instructions for the virtual machine.
```
./build/bcdump tests/golden/fib.wf
```
```
== fib (arity 1) ==
constants:
  [0] as int=1 as float=...
   0  LOAD_LOCAL     0
   2  PUSH_CONST     0
   4  ILE
   ...
```

**Virtual machine** — executes that bytecode directly. There's no
separate dump tool for this stage; `whitefang` itself *is* the VM
running to completion, so its normal output is the thing to look at:
```
./build/whitefang tests/golden/fib.wf
```
```
0
1
1
2
3
5
8
13
21
34
```

All four tools take a single `.wf` file argument and print to stdout;
none of them modify anything.

## Testing

```
make test
```

runs the golden-file suite in `tests/golden/`: every `.wf` program
there is run through `whitefang` and its stdout and exit code are
checked against a recorded reference (`tests/golden/errors/` holds
programs that are supposed to fail — only their exit code is checked
for those). See `tests/golden/README.md` for the format, and how to
add or update a test.

## Repo layout

```
docs/            language spec, VM/bytecode design, decisions log
src/
  lexer/         source -> tokens
  ast/           AST node definitions
  parser/        tokens -> AST
  bytecode/      AST -> bytecode compiler
  vm/            bytecode interpreter
  main.c         the whitefang binary: wires the four stages together
tests/
  golden/        .wf programs + expected output, and the test runner
examples/        example WhiteFang programs (non-test)
Dockerfile       multi-stage build -> minimal runtime image
```

## Further reading

- `docs/SPEC.md` — the language specification (syntax and semantics)
- `docs/VM.md` — the bytecode format and virtual machine design
- `docs/DECISIONS.md` — the rationale behind every non-obvious choice,
  including the ones that were later reversed
