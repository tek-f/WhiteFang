#ifndef WHITEFANG_TOKEN_H
#define WHITEFANG_TOKEN_H

typedef enum {
    /* Literals */
    TOKEN_INT_LIT,
    TOKEN_FLOAT_LIT,
    TOKEN_CHAR_LIT,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_PI,  /* _PI -- reserved constant-literal keyword, see docs/DECISIONS.md */
    TOKEN_E,   /* _E  -- ditto */
    TOKEN_G,   /* _G  -- ditto */
    TOKEN_R2,  /* _R2 -- ditto */
    TOKEN_PRINT, /* PRINT -- reserved keyword for the builtin print function */
    TOKEN_IDENTIFIER,

    /* Keywords */
    TOKEN_FUNC,
    TOKEN_LET,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_RETURN,
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_BOOL,
    TOKEN_CHAR,
    TOKEN_VOID,

    /* Punctuation */
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_SEMICOLON,
    TOKEN_ARROW,     /* -> */

    /* Operators */
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_EQ_EQ,
    TOKEN_BANG_EQ,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_LESS_EQ,
    TOKEN_GREATER_EQ,
    TOKEN_AND_AND,
    TOKEN_OR_OR,
    TOKEN_BANG,
    TOKEN_EQ,

    TOKEN_EOF,
    TOKEN_ERROR,
} TokenType;

/* A token is a slice into the source buffer (start + length), not an
 * owned/allocated string. The source buffer must outlive every Token
 * produced from it. For TOKEN_ERROR, start/length instead point at a
 * static, null-terminated diagnostic message. */
typedef struct {
    TokenType type;
    const char *start;
    int length;
    int line;
} Token;

const char *token_type_name(TokenType type);

#endif
