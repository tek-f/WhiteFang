#ifndef WHITEFANG_AST_H
#define WHITEFANG_AST_H

#include <stdint.h>

typedef enum {
    WF_TYPE_INT,
    WF_TYPE_FLOAT,
    WF_TYPE_BOOL,
    WF_TYPE_CHAR,
    WF_TYPE_VOID,
} WfType;

const char *wf_type_name(WfType type);

typedef enum {
    EXPR_INT_LIT,
    EXPR_FLOAT_LIT,
    EXPR_BOOL_LIT,
    EXPR_CHAR_LIT,
    EXPR_IDENTIFIER,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_CALL,
    EXPR_PRINT,
} ExprKind;

typedef enum {
    BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV, BIN_MOD,
    BIN_EQ, BIN_NE, BIN_LT, BIN_GT, BIN_LE, BIN_GE,
    BIN_AND, BIN_OR,
} BinOp;

typedef enum { UN_NEG, UN_NOT } UnOp;

typedef struct Expr Expr;

/* _PI/_E/_G are reserved constant-literal keywords (see
 * docs/DECISIONS.md, "Global variables removed") -- the parser turns
 * them directly into EXPR_FLOAT_LIT, so there's no dedicated ExprKind
 * for them here. */
struct Expr {
    ExprKind kind;
    int line;
    union {
        int32_t int_lit;
        double float_lit;
        int bool_lit; /* 0 or 1 */
        char char_lit;
        struct {
            const char *name;
            int name_len;
        } identifier;
        struct {
            UnOp op;
            Expr *operand;
        } unary;
        struct {
            BinOp op;
            Expr *left;
            Expr *right;
        } binary;
        struct {
            const char *name;
            int name_len;
            Expr **args;
            int arg_count;
            int arg_cap;
        } call;
        /* PRINT is a reserved keyword (docs/DECISIONS.md, "print
         * renamed PRINT"), not a user-callable name -- its own
         * ExprKind rather than routed through `call` above, same
         * treatment _PI/_E/_G/_R2 get as EXPR_FLOAT_LIT. Fixed arity
         * 1, so no args array is needed. */
        struct {
            Expr *arg;
        } print;
    } as;
};

typedef enum {
    STMT_VAR_DECL,
    STMT_ASSIGN,
    STMT_IF,
    STMT_WHILE,
    STMT_RETURN,
    STMT_EXPR,
    STMT_BLOCK,
} StmtKind;

typedef struct Stmt Stmt;

struct Stmt {
    StmtKind kind;
    int line;
    union {
        /* let type: name = init; -- one variable per declaration, see
         * docs/DECISIONS.md ("Variable declarations: exactly one name
         * per let"). Local-only: there are no global variables in M0. */
        struct {
            WfType type;
            const char *name;
            int name_len;
            Expr *init;
        } var_decl;
        struct {
            const char *name;
            int name_len;
            Expr *value;
        } assign;
        /* else_branch is NULL, a STMT_BLOCK, or a nested STMT_IF
         * (else-if chains via nesting, no dedicated grammar for it). */
        struct {
            Expr *cond;
            Stmt *then_branch;
            Stmt *else_branch;
        } if_stmt;
        struct {
            Expr *cond;
            Stmt *body; /* STMT_BLOCK */
        } while_stmt;
        struct {
            Expr *value; /* NULL for a bare "return;" */
        } return_stmt;
        struct {
            Expr *expr;
        } expr_stmt;
        struct {
            Stmt **stmts;
            int count;
            int cap;
        } block;
    } as;
};

typedef struct {
    WfType type;
    const char *name;
    int name_len;
} Param;

typedef struct {
    const char *name;
    int name_len;
    Param *params;
    int param_count;
    int param_cap;
    WfType return_type;
    Stmt *body; /* STMT_BLOCK */
    int line;
} FunctionDecl;

/* A program is just a list of functions -- there are no top-level
 * variable declarations in M0 (see docs/DECISIONS.md, "Global
 * variables removed"), so no Decl/DeclKind wrapper is needed.
 * Named AstProgram, not Program, because docs/VM.md already reserves
 * "Program" for the compiled Chunk-based struct in src/bytecode/ --
 * this is the parsed-but-not-yet-compiled tree. */
typedef struct {
    FunctionDecl *funcs;
    int count;
    int cap;
} AstProgram;

/* Node constructors: heap-allocate and zero-initialize. The AST is
 * never explicitly freed -- see docs/DECISIONS.md ("AST nodes are
 * malloc'd and never freed"). */
Expr *expr_new(ExprKind kind, int line);
Stmt *stmt_new(StmtKind kind, int line);

void program_add_func(AstProgram *program, FunctionDecl func);
void stmt_block_add(Stmt *block, Stmt *stmt);
void func_add_param(FunctionDecl *func, Param param);
void call_add_arg(Expr *call, Expr *arg);

void ast_dump_program(const AstProgram *program);

#endif
