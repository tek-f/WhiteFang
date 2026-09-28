#ifndef WHITEFANG_PARSER_H
#define WHITEFANG_PARSER_H

#include "ast.h"
#include "lexer.h"

/* Carries a standing 2-token lookahead window (current + next) so the
 * parser can distinguish "x = 10;" (assignment) from "print(x);"
 * (expression-statement) before committing to either -- see
 * docs/DECISIONS.md. */
typedef struct {
    Lexer lexer;
    Token previous;
    Token current;
    Token next;
} Parser;

/* source must outlive the parser and every node in the AstProgram it
 * returns (AST nodes hold slices into it, same as Token). */
void parser_init(Parser *parser, const char *source);

/* Parses the whole program. On the first syntax error, reports it to
 * stderr and exits the process with status 65 -- there is no error
 * recovery in M2, see docs/DECISIONS.md ("stop on first error"). */
AstProgram *parser_parse_program(Parser *parser);

#endif
