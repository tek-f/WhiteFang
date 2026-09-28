# ast

AST node type definitions, shared by the parser (M2) and bytecode
compiler (M3). M2 — done.

- `ast.h` — `Expr`/`Stmt`/`FunctionDecl`/`Program`: tagged unions,
  same idiom as `Token` (see `docs/DECISIONS.md`). A `Program` is just
  a list of functions — no global variables in M0, so no separate
  declaration-kind wrapper is needed.
- `ast.c` — node constructors and `ast_dump_program`, the indented-tree
  printer `parsedump` uses.
- `dynarray.h` — a small growable-array append macro shared with
  `src/parser/parser.c`.
