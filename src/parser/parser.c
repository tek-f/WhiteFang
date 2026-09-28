#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void advance(Parser *parser);
static int check(Parser *parser, TokenType type);
static int match(Parser *parser, TokenType type);
static void consume(Parser *parser, TokenType type, const char *message);
static void error_at(Token token, const char *message);
static void error_at_current(Parser *parser, const char *message);

static WfType parse_type(Parser *parser);

static Expr *parse_expression(Parser *parser);
static Expr *parse_logic_or(Parser *parser);
static Expr *parse_logic_and(Parser *parser);
static Expr *parse_equality(Parser *parser);
static Expr *parse_comparison(Parser *parser);
static Expr *parse_term(Parser *parser);
static Expr *parse_factor(Parser *parser);
static Expr *parse_unary(Parser *parser);
static Expr *parse_primary(Parser *parser);
static Expr *parse_call(Parser *parser, Token name_token);

static Stmt *parse_statement(Parser *parser);
static Stmt *parse_block(Parser *parser);
static Stmt *parse_var_decl_stmt(Parser *parser);
static Stmt *parse_assign_or_expr_statement(Parser *parser);
static Stmt *parse_if_statement(Parser *parser);
static Stmt *parse_while_statement(Parser *parser);
static Stmt *parse_return_statement(Parser *parser);

static FunctionDecl parse_function_decl(Parser *parser);

/* ---- lexer plumbing ---- */

static Token next_real_token(Parser *parser) {
    for (;;) {
        Token token = lexer_next_token(&parser->lexer);
        if (token.type != TOKEN_ERROR) return token;
        fprintf(stderr, "[line %d] lex error: %.*s\n", token.line, token.length, token.start);
        exit(65);
    }
}

void parser_init(Parser *parser, const char *source) {
    lexer_init(&parser->lexer, source);
    parser->current = next_real_token(parser);
    parser->next = next_real_token(parser);
}

static void advance(Parser *parser) {
    parser->previous = parser->current;
    parser->current = parser->next;
    parser->next = next_real_token(parser);
}

static int check(Parser *parser, TokenType type) { return parser->current.type == type; }

static int match(Parser *parser, TokenType type) {
    if (!check(parser, type)) return 0;
    advance(parser);
    return 1;
}

/* No recovery in M2 (see docs/DECISIONS.md, "stop on first error"):
 * report and abort the process immediately rather than threading a
 * pass/fail result back up through every recursive-descent function. */
static void error_at(Token token, const char *message) {
    fprintf(stderr, "[line %d] error", token.line);
    if (token.type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else {
        fprintf(stderr, " at '%.*s'", token.length, token.start);
    }
    fprintf(stderr, ": %s\n", message);
    exit(65);
}

static void error_at_current(Parser *parser, const char *message) {
    error_at(parser->current, message);
}

static void consume(Parser *parser, TokenType type, const char *message) {
    if (check(parser, type)) {
        advance(parser);
        return;
    }
    error_at_current(parser, message);
}

/* ---- types ---- */

static WfType parse_type(Parser *parser) {
    switch (parser->current.type) {
        case TOKEN_INT: advance(parser); return WF_TYPE_INT;
        case TOKEN_FLOAT: advance(parser); return WF_TYPE_FLOAT;
        case TOKEN_BOOL: advance(parser); return WF_TYPE_BOOL;
        case TOKEN_CHAR: advance(parser); return WF_TYPE_CHAR;
        case TOKEN_VOID: advance(parser); return WF_TYPE_VOID;
        default:
            error_at_current(parser, "Expected a type.");
            return WF_TYPE_VOID; /* unreachable: error_at_current exits */
    }
}

/* ---- literal decoding ---- */

/* token.start points at the opening quote; the lexer already
 * validated the escape (or lack of one) is well-formed (M1, see
 * docs/DECISIONS.md), so this just decodes rather than re-validates. */
static char decode_char_literal(Token token) {
    const char *p = token.start + 1;
    if (*p == '\\') {
        switch (p[1]) {
            case 'n': return '\n';
            case 't': return '\t';
            case 'r': return '\r';
            case '\\': return '\\';
            case '\'': return '\'';
            case '0': return '\0';
            default: return p[1];
        }
    }
    return *p;
}

/* _PI/_E/_G/_R2 are reserved keywords that behave exactly like float
 * literals -- see docs/DECISIONS.md ("Global variables removed").
 * From this point on (and everywhere downstream, including M3) they
 * are indistinguishable from writing the literal number itself. */
static Expr *parse_builtin_constant(Parser *parser) {
    double value;
    switch (parser->current.type) {
        case TOKEN_PI: value = 3.141592653589793; break;
        case TOKEN_E: value = 2.718281828459045; break;
        case TOKEN_G: value = 1.618033988749895; break;
        case TOKEN_R2: value = 1.414213562373095; break;
        default: value = 0.0; break; /* unreachable */
    }
    int line = parser->current.line;
    advance(parser);
    Expr *expr = expr_new(EXPR_FLOAT_LIT, line);
    expr->as.float_lit = value;
    return expr;
}

/* ---- expressions ----
 *
 * Precedence climbing via cascading functions, one per C precedence
 * level, loosest to tightest -- see docs/DECISIONS.md. Assignment is
 * not part of this grammar at all: it's a statement, not an
 * expression (see parse_assign_or_expr_statement). */

static Expr *parse_call(Parser *parser, Token name_token) {
    Expr *call = expr_new(EXPR_CALL, name_token.line);
    call->as.call.name = name_token.start;
    call->as.call.name_len = name_token.length;
    consume(parser, TOKEN_LPAREN, "Expected '(' after function name.");
    if (!check(parser, TOKEN_RPAREN)) {
        do {
            call_add_arg(call, parse_expression(parser));
        } while (match(parser, TOKEN_COMMA));
    }
    consume(parser, TOKEN_RPAREN, "Expected ')' after arguments.");
    return call;
}

static Expr *parse_primary(Parser *parser) {
    if (match(parser, TOKEN_INT_LIT)) {
        Expr *expr = expr_new(EXPR_INT_LIT, parser->previous.line);
        expr->as.int_lit = (int32_t)strtol(parser->previous.start, NULL, 10);
        return expr;
    }
    if (match(parser, TOKEN_FLOAT_LIT)) {
        Expr *expr = expr_new(EXPR_FLOAT_LIT, parser->previous.line);
        expr->as.float_lit = strtod(parser->previous.start, NULL);
        return expr;
    }
    if (match(parser, TOKEN_CHAR_LIT)) {
        Expr *expr = expr_new(EXPR_CHAR_LIT, parser->previous.line);
        expr->as.char_lit = decode_char_literal(parser->previous);
        return expr;
    }
    if (match(parser, TOKEN_TRUE)) {
        Expr *expr = expr_new(EXPR_BOOL_LIT, parser->previous.line);
        expr->as.bool_lit = 1;
        return expr;
    }
    if (match(parser, TOKEN_FALSE)) {
        Expr *expr = expr_new(EXPR_BOOL_LIT, parser->previous.line);
        expr->as.bool_lit = 0;
        return expr;
    }
    if (check(parser, TOKEN_PI) || check(parser, TOKEN_E) || check(parser, TOKEN_G) ||
        check(parser, TOKEN_R2)) {
        return parse_builtin_constant(parser);
    }
    if (match(parser, TOKEN_PRINT)) {
        int line = parser->previous.line;
        consume(parser, TOKEN_LPAREN, "Expected '(' after 'PRINT'.");
        Expr *arg = parse_expression(parser);
        consume(parser, TOKEN_RPAREN, "Expected ')' after PRINT argument.");
        Expr *expr = expr_new(EXPR_PRINT, line);
        expr->as.print.arg = arg;
        return expr;
    }
    if (match(parser, TOKEN_IDENTIFIER)) {
        Token name = parser->previous;
        if (check(parser, TOKEN_LPAREN)) {
            return parse_call(parser, name);
        }
        Expr *expr = expr_new(EXPR_IDENTIFIER, name.line);
        expr->as.identifier.name = name.start;
        expr->as.identifier.name_len = name.length;
        return expr;
    }
    if (match(parser, TOKEN_LPAREN)) {
        Expr *expr = parse_expression(parser);
        consume(parser, TOKEN_RPAREN, "Expected ')' after expression.");
        return expr;
    }
    error_at_current(parser, "Expected an expression.");
    return NULL; /* unreachable: error_at_current exits */
}

static Expr *parse_unary(Parser *parser) {
    if (check(parser, TOKEN_BANG) || check(parser, TOKEN_MINUS)) {
        UnOp op = parser->current.type == TOKEN_BANG ? UN_NOT : UN_NEG;
        int line = parser->current.line;
        advance(parser);
        Expr *operand = parse_unary(parser);
        Expr *expr = expr_new(EXPR_UNARY, line);
        expr->as.unary.op = op;
        expr->as.unary.operand = operand;
        return expr;
    }
    return parse_primary(parser);
}

static Expr *make_binary(BinOp op, int line, Expr *left, Expr *right) {
    Expr *expr = expr_new(EXPR_BINARY, line);
    expr->as.binary.op = op;
    expr->as.binary.left = left;
    expr->as.binary.right = right;
    return expr;
}

static Expr *parse_factor(Parser *parser) {
    Expr *expr = parse_unary(parser);
    for (;;) {
        BinOp op;
        if (check(parser, TOKEN_STAR)) op = BIN_MUL;
        else if (check(parser, TOKEN_SLASH)) op = BIN_DIV;
        else if (check(parser, TOKEN_PERCENT)) op = BIN_MOD;
        else break;
        int line = parser->current.line;
        advance(parser);
        expr = make_binary(op, line, expr, parse_unary(parser));
    }
    return expr;
}

static Expr *parse_term(Parser *parser) {
    Expr *expr = parse_factor(parser);
    for (;;) {
        BinOp op;
        if (check(parser, TOKEN_PLUS)) op = BIN_ADD;
        else if (check(parser, TOKEN_MINUS)) op = BIN_SUB;
        else break;
        int line = parser->current.line;
        advance(parser);
        expr = make_binary(op, line, expr, parse_factor(parser));
    }
    return expr;
}

static Expr *parse_comparison(Parser *parser) {
    Expr *expr = parse_term(parser);
    for (;;) {
        BinOp op;
        if (check(parser, TOKEN_LESS)) op = BIN_LT;
        else if (check(parser, TOKEN_GREATER)) op = BIN_GT;
        else if (check(parser, TOKEN_LESS_EQ)) op = BIN_LE;
        else if (check(parser, TOKEN_GREATER_EQ)) op = BIN_GE;
        else break;
        int line = parser->current.line;
        advance(parser);
        expr = make_binary(op, line, expr, parse_term(parser));
    }
    return expr;
}

static Expr *parse_equality(Parser *parser) {
    Expr *expr = parse_comparison(parser);
    for (;;) {
        BinOp op;
        if (check(parser, TOKEN_EQ_EQ)) op = BIN_EQ;
        else if (check(parser, TOKEN_BANG_EQ)) op = BIN_NE;
        else break;
        int line = parser->current.line;
        advance(parser);
        expr = make_binary(op, line, expr, parse_comparison(parser));
    }
    return expr;
}

static Expr *parse_logic_and(Parser *parser) {
    Expr *expr = parse_equality(parser);
    while (check(parser, TOKEN_AND_AND)) {
        int line = parser->current.line;
        advance(parser);
        expr = make_binary(BIN_AND, line, expr, parse_equality(parser));
    }
    return expr;
}

static Expr *parse_logic_or(Parser *parser) {
    Expr *expr = parse_logic_and(parser);
    while (check(parser, TOKEN_OR_OR)) {
        int line = parser->current.line;
        advance(parser);
        expr = make_binary(BIN_OR, line, expr, parse_logic_and(parser));
    }
    return expr;
}

static Expr *parse_expression(Parser *parser) { return parse_logic_or(parser); }

/* ---- statements ---- */

static Stmt *parse_var_decl_stmt(Parser *parser) {
    int line = parser->current.line;
    consume(parser, TOKEN_LET, "Expected 'let'.");
    WfType type = parse_type(parser);
    consume(parser, TOKEN_COLON, "Expected ':' after type in variable declaration.");
    consume(parser, TOKEN_IDENTIFIER, "Expected a variable name.");
    Token name = parser->previous;
    consume(parser, TOKEN_EQ, "Expected '=' (every 'let' requires an initializer).");
    Expr *init = parse_expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

    Stmt *stmt = stmt_new(STMT_VAR_DECL, line);
    stmt->as.var_decl.type = type;
    stmt->as.var_decl.name = name.start;
    stmt->as.var_decl.name_len = name.length;
    stmt->as.var_decl.init = init;
    return stmt;
}

/* "x = 10;" (assignment, a statement) vs. "print(x);" (an expression
 * used as a statement) both start with an identifier -- the 2-token
 * lookahead (current + next) is what tells them apart before
 * committing to either. See docs/DECISIONS.md. */
static Stmt *parse_assign_or_expr_statement(Parser *parser) {
    if (check(parser, TOKEN_IDENTIFIER) && parser->next.type == TOKEN_EQ) {
        int line = parser->current.line;
        advance(parser); /* identifier */
        Token name = parser->previous;
        advance(parser); /* '=' */
        Expr *value = parse_expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expected ';' after assignment.");

        Stmt *stmt = stmt_new(STMT_ASSIGN, line);
        stmt->as.assign.name = name.start;
        stmt->as.assign.name_len = name.length;
        stmt->as.assign.value = value;
        return stmt;
    }

    int line = parser->current.line;
    Expr *expr = parse_expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after expression.");
    Stmt *stmt = stmt_new(STMT_EXPR, line);
    stmt->as.expr_stmt.expr = expr;
    return stmt;
}

static Stmt *parse_if_statement(Parser *parser) {
    int line = parser->current.line;
    consume(parser, TOKEN_IF, "Expected 'if'.");
    consume(parser, TOKEN_LPAREN, "Expected '(' after 'if'.");
    Expr *cond = parse_expression(parser);
    consume(parser, TOKEN_RPAREN, "Expected ')' after condition.");
    Stmt *then_branch = parse_block(parser);

    Stmt *else_branch = NULL;
    if (match(parser, TOKEN_ELSE)) {
        /* else-if chains via nesting -- no dedicated grammar rule */
        else_branch = check(parser, TOKEN_IF) ? parse_if_statement(parser) : parse_block(parser);
    }

    Stmt *stmt = stmt_new(STMT_IF, line);
    stmt->as.if_stmt.cond = cond;
    stmt->as.if_stmt.then_branch = then_branch;
    stmt->as.if_stmt.else_branch = else_branch;
    return stmt;
}

static Stmt *parse_while_statement(Parser *parser) {
    int line = parser->current.line;
    consume(parser, TOKEN_WHILE, "Expected 'while'.");
    consume(parser, TOKEN_LPAREN, "Expected '(' after 'while'.");
    Expr *cond = parse_expression(parser);
    consume(parser, TOKEN_RPAREN, "Expected ')' after condition.");
    Stmt *body = parse_block(parser);

    Stmt *stmt = stmt_new(STMT_WHILE, line);
    stmt->as.while_stmt.cond = cond;
    stmt->as.while_stmt.body = body;
    return stmt;
}

static Stmt *parse_return_statement(Parser *parser) {
    int line = parser->current.line;
    consume(parser, TOKEN_RETURN, "Expected 'return'.");
    Expr *value = NULL;
    if (!check(parser, TOKEN_SEMICOLON)) {
        value = parse_expression(parser);
    }
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after return statement.");

    Stmt *stmt = stmt_new(STMT_RETURN, line);
    stmt->as.return_stmt.value = value;
    return stmt;
}

static Stmt *parse_block(Parser *parser) {
    int line = parser->current.line;
    consume(parser, TOKEN_LBRACE, "Expected '{'.");
    Stmt *block = stmt_new(STMT_BLOCK, line);
    while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
        stmt_block_add(block, parse_statement(parser));
    }
    consume(parser, TOKEN_RBRACE, "Expected '}'.");
    return block;
}

static Stmt *parse_statement(Parser *parser) {
    if (check(parser, TOKEN_LET)) return parse_var_decl_stmt(parser);
    if (check(parser, TOKEN_IF)) return parse_if_statement(parser);
    if (check(parser, TOKEN_WHILE)) return parse_while_statement(parser);
    if (check(parser, TOKEN_RETURN)) return parse_return_statement(parser);
    if (check(parser, TOKEN_LBRACE)) return parse_block(parser);
    return parse_assign_or_expr_statement(parser);
}

/* ---- top level ----
 *
 * A program is just a list of function declarations -- there are no
 * top-level variable declarations in M0 (docs/DECISIONS.md, "Global
 * variables removed"), so top level has exactly one production. */

static FunctionDecl parse_function_decl(Parser *parser) {
    int line = parser->current.line;
    consume(parser, TOKEN_FUNC, "Expected 'func'.");
    consume(parser, TOKEN_IDENTIFIER, "Expected a function name.");
    Token name = parser->previous;
    consume(parser, TOKEN_LPAREN, "Expected '(' after function name.");

    FunctionDecl func;
    memset(&func, 0, sizeof(func));
    func.name = name.start;
    func.name_len = name.length;
    func.line = line;

    if (!check(parser, TOKEN_RPAREN)) {
        do {
            WfType ptype = parse_type(parser);
            consume(parser, TOKEN_IDENTIFIER, "Expected a parameter name.");
            Token pname = parser->previous;
            Param param;
            param.type = ptype;
            param.name = pname.start;
            param.name_len = pname.length;
            func_add_param(&func, param);
        } while (match(parser, TOKEN_COMMA));
    }
    consume(parser, TOKEN_RPAREN, "Expected ')' after parameters.");

    func.body = parse_block(parser);
    consume(parser, TOKEN_ARROW, "Expected '->' after function body.");
    func.return_type = parse_type(parser);
    return func;
}

AstProgram *parser_parse_program(Parser *parser) {
    AstProgram *program = calloc(1, sizeof(AstProgram));
    while (!check(parser, TOKEN_EOF)) {
        if (!check(parser, TOKEN_FUNC)) {
            error_at_current(parser, "Expected a function declaration.");
        }
        program_add_func(program, parse_function_decl(parser));
    }
    return program;
}
