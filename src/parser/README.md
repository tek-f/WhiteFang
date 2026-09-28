# parser

Token stream -> AST, per the grammar implied by `docs/SPEC.md`. M2 —
done.

- `parser.h` / `parser.c` — recursive-descent parser, precedence
  climbing for expressions, a standing 2-token lookahead window
  (assignment vs. expression-statement disambiguation), stops on the
  first syntax error (exit code 65, no recovery) — see
  `docs/DECISIONS.md`.
- `parse_dump.c` — CLI tool: `./build/parsedump path/to/file.wf`
  prints the parsed AST as an indented tree. Manual verification aid,
  not an automated test (those start at M3).

Purely syntactic — no name/type checking here. Semantic analysis
(duplicate names, undeclared identifiers, type mismatches) is deferred
to M3, which needs the same symbol-table bookkeeping anyway for
codegen. See `docs/DECISIONS.md`.
