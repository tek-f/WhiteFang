#include "lexer.h"

#include <string.h>

static int is_digit(char c) { return c >= '0' && c <= '9'; }

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_at_end(const Lexer *lexer) { return *lexer->current == '\0'; }

static char peek(const Lexer *lexer) { return *lexer->current; }

static char peek_next(const Lexer *lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

static char advance(Lexer *lexer) {
    lexer->current++;
    return lexer->current[-1];
}

static int match(Lexer *lexer, char expected) {
    if (is_at_end(lexer)) return 0;
    if (*lexer->current != expected) return 0;
    lexer->current++;
    return 1;
}

static Token make_token(const Lexer *lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (int)(lexer->current - lexer->start);
    token.line = lexer->line;
    return token;
}

static Token error_token(const Lexer *lexer, const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer->line;
    return token;
}

/* Skips whitespace and comments. Line comments (/`) run to end of
 * line; block comments (``...``) run to the next `` after the
 * opener, with no nesting (see docs/SPEC.md sec 1). A lone backtick
 * with no immediately-following backtick is left in place for
 * lexer_next_token to report as an error, since no construct in M0
 * uses a single backtick.
 *
 * Returns 1 if it stopped mid-way on an unterminated block comment
 * (hit EOF before the closing ``), 0 otherwise. */
static int skip_whitespace_and_comments(Lexer *lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                lexer->line++;
                advance(lexer);
                break;
            case '/':
                if (peek_next(lexer) == '`') {
                    advance(lexer); /* '/' */
                    advance(lexer); /* '`' */
                    while (!is_at_end(lexer) && peek(lexer) != '\n') {
                        advance(lexer);
                    }
                } else {
                    return 0; /* division operator, not a comment */
                }
                break;
            case '`':
                if (peek_next(lexer) == '`') {
                    advance(lexer); /* first '`' */
                    advance(lexer); /* second '`' */
                    for (;;) {
                        if (is_at_end(lexer)) return 1; /* unterminated */
                        if (peek(lexer) == '`' && peek_next(lexer) == '`') {
                            advance(lexer);
                            advance(lexer);
                            break;
                        }
                        if (peek(lexer) == '\n') lexer->line++;
                        advance(lexer);
                    }
                } else {
                    return 0; /* lone backtick, not a comment opener */
                }
                break;
            default:
                return 0;
        }
    }
}

static const struct {
    const char *keyword;
    TokenType type;
} KEYWORDS[] = {
    {"func", TOKEN_FUNC},   {"let", TOKEN_LET},     {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},   {"while", TOKEN_WHILE}, {"return", TOKEN_RETURN},
    {"true", TOKEN_TRUE},   {"false", TOKEN_FALSE}, {"int", TOKEN_INT},
    {"float", TOKEN_FLOAT}, {"bool", TOKEN_BOOL},   {"char", TOKEN_CHAR},
    {"void", TOKEN_VOID},   {"_PI", TOKEN_PI},      {"_E", TOKEN_E},
    {"_G", TOKEN_G},        {"_R2", TOKEN_R2},      {"PRINT", TOKEN_PRINT},
};
#define KEYWORD_COUNT (sizeof(KEYWORDS) / sizeof(KEYWORDS[0]))

static TokenType identifier_type(const Lexer *lexer) {
    int length = (int)(lexer->current - lexer->start);
    for (size_t i = 0; i < KEYWORD_COUNT; i++) {
        size_t klen = strlen(KEYWORDS[i].keyword);
        if ((int)klen == length &&
            memcmp(lexer->start, KEYWORDS[i].keyword, klen) == 0) {
            return KEYWORDS[i].type;
        }
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(Lexer *lexer) {
    while (is_alpha(peek(lexer)) || is_digit(peek(lexer))) advance(lexer);
    return make_token(lexer, identifier_type(lexer));
}

/* int: digit+. float: digit+ '.' digit+. No exponent notation, no
 * leading/trailing bare decimal point (e.g. ".5" or "5.") in M0 —
 * not specified in docs/SPEC.md sec 2, not needed yet. */
static Token number(Lexer *lexer) {
    while (is_digit(peek(lexer))) advance(lexer);

    TokenType type = TOKEN_INT_LIT;
    if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
        type = TOKEN_FLOAT_LIT;
        advance(lexer); /* '.' */
        while (is_digit(peek(lexer))) advance(lexer);
    }
    return make_token(lexer, type);
}

/* Char literals: 'x' or a backslash escape 'x. Supported escapes:
 * \n \t \r \\ \' \0 — this list isn't in docs/SPEC.md yet (M0 only
 * specified the bare 'a' form); flagged for redlining, see
 * docs/DECISIONS.md. */
static Token char_literal(Lexer *lexer) {
    if (is_at_end(lexer) || peek(lexer) == '\n') {
        return error_token(lexer, "Unterminated char literal.");
    }
    if (peek(lexer) == '\\') {
        advance(lexer); /* '\' */
        char esc = peek(lexer);
        if (esc != 'n' && esc != 't' && esc != 'r' && esc != '\\' &&
            esc != '\'' && esc != '0') {
            return error_token(lexer, "Unknown escape sequence in char literal.");
        }
        advance(lexer); /* escaped char */
    } else {
        advance(lexer); /* the literal character */
    }
    if (peek(lexer) != '\'') {
        return error_token(lexer, "Unterminated char literal.");
    }
    advance(lexer); /* closing quote */
    return make_token(lexer, TOKEN_CHAR_LIT);
}

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
}

Token lexer_next_token(Lexer *lexer) {
    int unterminated_comment = skip_whitespace_and_comments(lexer);
    lexer->start = lexer->current;

    if (unterminated_comment) {
        return error_token(lexer, "Unterminated block comment.");
    }
    if (is_at_end(lexer)) return make_token(lexer, TOKEN_EOF);

    char c = advance(lexer);

    if (is_alpha(c)) return identifier(lexer);
    if (is_digit(c)) return number(lexer);
    if (c == '\'') return char_literal(lexer);

    switch (c) {
        case '(': return make_token(lexer, TOKEN_LPAREN);
        case ')': return make_token(lexer, TOKEN_RPAREN);
        case '{': return make_token(lexer, TOKEN_LBRACE);
        case '}': return make_token(lexer, TOKEN_RBRACE);
        case ',': return make_token(lexer, TOKEN_COMMA);
        case ':': return make_token(lexer, TOKEN_COLON);
        case ';': return make_token(lexer, TOKEN_SEMICOLON);
        case '+': return make_token(lexer, TOKEN_PLUS);
        case '*': return make_token(lexer, TOKEN_STAR);
        case '/': return make_token(lexer, TOKEN_SLASH);
        case '%': return make_token(lexer, TOKEN_PERCENT);
        case '`': return error_token(lexer, "Unexpected lone backtick.");
        case '-':
            if (match(lexer, '>')) return make_token(lexer, TOKEN_ARROW);
            return make_token(lexer, TOKEN_MINUS);
        case '=':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_EQ_EQ);
            return make_token(lexer, TOKEN_EQ);
        case '!':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_BANG_EQ);
            return make_token(lexer, TOKEN_BANG);
        case '<':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_LESS_EQ);
            return make_token(lexer, TOKEN_LESS);
        case '>':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_GREATER_EQ);
            return make_token(lexer, TOKEN_GREATER);
        case '&':
            if (match(lexer, '&')) return make_token(lexer, TOKEN_AND_AND);
            return error_token(lexer, "Unexpected character '&' (did you mean '&&'?).");
        case '|':
            if (match(lexer, '|')) return make_token(lexer, TOKEN_OR_OR);
            return error_token(lexer, "Unexpected character '|' (did you mean '||'?).");
        default:
            return error_token(lexer, "Unexpected character.");
    }
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_INT_LIT: return "INT_LIT";
        case TOKEN_FLOAT_LIT: return "FLOAT_LIT";
        case TOKEN_CHAR_LIT: return "CHAR_LIT";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_PI: return "PI";
        case TOKEN_E: return "E";
        case TOKEN_G: return "G";
        case TOKEN_R2: return "R2";
        case TOKEN_PRINT: return "PRINT";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_FUNC: return "FUNC";
        case TOKEN_LET: return "LET";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_INT: return "INT";
        case TOKEN_FLOAT: return "FLOAT";
        case TOKEN_BOOL: return "BOOL";
        case TOKEN_CHAR: return "CHAR";
        case TOKEN_VOID: return "VOID";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_COLON: return "COLON";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_PERCENT: return "PERCENT";
        case TOKEN_EQ_EQ: return "EQ_EQ";
        case TOKEN_BANG_EQ: return "BANG_EQ";
        case TOKEN_LESS: return "LESS";
        case TOKEN_GREATER: return "GREATER";
        case TOKEN_LESS_EQ: return "LESS_EQ";
        case TOKEN_GREATER_EQ: return "GREATER_EQ";
        case TOKEN_AND_AND: return "AND_AND";
        case TOKEN_OR_OR: return "OR_OR";
        case TOKEN_BANG: return "BANG";
        case TOKEN_EQ: return "EQ";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
    }
    return "UNKNOWN";
}
