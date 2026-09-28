#ifndef WHITEFANG_LEXER_H
#define WHITEFANG_LEXER_H

#include "token.h"

typedef struct {
    const char *source;  /* start of the whole buffer, for reference */
    const char *start;   /* start of the token currently being scanned */
    const char *current; /* next unread character */
    int line;
} Lexer;

/* source must be a null-terminated buffer that outlives the lexer and
 * every Token it produces (tokens are slices into it, not copies). */
void lexer_init(Lexer *lexer, const char *source);

/* Scans and returns the next token, advancing the lexer. Skips
 * whitespace and comments internally. Returns a TOKEN_EOF once at end
 * of input; calling again after that continues to return TOKEN_EOF. */
Token lexer_next_token(Lexer *lexer);

#endif
