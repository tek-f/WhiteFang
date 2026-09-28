# lexer

WhiteFang source text -> token stream, per `docs/SPEC.md` §1
(lexical structure). M1 — done.

- `token.h` — `TokenType`, `Token` (a source-slice, not an owned
  string — see `docs/DECISIONS.md`)
- `lexer.h` / `lexer.c` — `Lexer`, `lexer_init`, `lexer_next_token`
- `lex_dump.c` — CLI tool: `./build/lexdump path/to/file.wf` prints
  every token. Manual verification aid, not an automated test (those
  start at M3 with golden files in `tests/golden/`).
