#include "ast.h"

#include <stdio.h>
#include <stdlib.h>

#include "dynarray.h"

const char *wf_type_name(WfType type) {
    switch (type) {
        case WF_TYPE_INT: return "int";
        case WF_TYPE_FLOAT: return "float";
        case WF_TYPE_BOOL: return "bool";
        case WF_TYPE_CHAR: return "char";
        case WF_TYPE_VOID: return "void";
    }
    return "?";
}

Expr *expr_new(ExprKind kind, int line) {
    Expr *expr = calloc(1, sizeof(Expr));
    expr->kind = kind;
    expr->line = line;
    return expr;
}

Stmt *stmt_new(StmtKind kind, int line) {
    Stmt *stmt = calloc(1, sizeof(Stmt));
    stmt->kind = kind;
    stmt->line = line;
    return stmt;
}

void program_add_func(AstProgram *program, FunctionDecl func) {
    DA_APPEND(program->funcs, program->cap, program->count, func);
}

void stmt_block_add(Stmt *block, Stmt *stmt) {
    DA_APPEND(block->as.block.stmts, block->as.block.cap, block->as.block.count, stmt);
}

void func_add_param(FunctionDecl *func, Param param) {
    DA_APPEND(func->params, func->param_cap, func->param_count, param);
}

void call_add_arg(Expr *call, Expr *arg) {
    DA_APPEND(call->as.call.args, call->as.call.arg_cap, call->as.call.arg_count, arg);
}

/* ---- dump (parsedump's rendering; see docs/DECISIONS.md for why an
 * indented tree rather than S-expressions) ---- */

static void indent(int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
}

static const char *bin_op_name(BinOp op) {
    switch (op) {
        case BIN_ADD: return "+";
        case BIN_SUB: return "-";
        case BIN_MUL: return "*";
        case BIN_DIV: return "/";
        case BIN_MOD: return "%";
        case BIN_EQ: return "==";
        case BIN_NE: return "!=";
        case BIN_LT: return "<";
        case BIN_GT: return ">";
        case BIN_LE: return "<=";
        case BIN_GE: return ">=";
        case BIN_AND: return "&&";
        case BIN_OR: return "||";
    }
    return "?";
}

/* Renders a char literal's value back into escaped source form for
 * the dump (the inverse of parser.c's decode_char_literal) -- printing
 * the raw byte would make '\n' look like a literal blank line. */
static void print_char_lit(char c) {
    switch (c) {
        case '\n': printf("'\\n'"); break;
        case '\t': printf("'\\t'"); break;
        case '\r': printf("'\\r'"); break;
        case '\\': printf("'\\\\'"); break;
        case '\'': printf("'\\''"); break;
        case '\0': printf("'\\0'"); break;
        default: printf("'%c'", c); break;
    }
}

static const char *un_op_name(UnOp op) {
    switch (op) {
        case UN_NEG: return "-";
        case UN_NOT: return "!";
    }
    return "?";
}

static void dump_expr(const Expr *expr, int depth) {
    indent(depth);
    switch (expr->kind) {
        case EXPR_INT_LIT:
            printf("IntLit %d\n", expr->as.int_lit);
            break;
        case EXPR_FLOAT_LIT:
            printf("FloatLit %g\n", expr->as.float_lit);
            break;
        case EXPR_BOOL_LIT:
            printf("BoolLit %s\n", expr->as.bool_lit ? "true" : "false");
            break;
        case EXPR_CHAR_LIT:
            printf("CharLit ");
            print_char_lit(expr->as.char_lit);
            printf("\n");
            break;
        case EXPR_IDENTIFIER:
            printf("Identifier %.*s\n", expr->as.identifier.name_len, expr->as.identifier.name);
            break;
        case EXPR_UNARY:
            printf("Unary %s\n", un_op_name(expr->as.unary.op));
            dump_expr(expr->as.unary.operand, depth + 1);
            break;
        case EXPR_BINARY:
            printf("Binary %s\n", bin_op_name(expr->as.binary.op));
            dump_expr(expr->as.binary.left, depth + 1);
            dump_expr(expr->as.binary.right, depth + 1);
            break;
        case EXPR_CALL:
            printf("Call %.*s\n", expr->as.call.name_len, expr->as.call.name);
            for (int i = 0; i < expr->as.call.arg_count; i++) {
                dump_expr(expr->as.call.args[i], depth + 1);
            }
            break;
        case EXPR_PRINT:
            printf("Print\n");
            dump_expr(expr->as.print.arg, depth + 1);
            break;
    }
}

static void dump_stmt(const Stmt *stmt, int depth) {
    indent(depth);
    switch (stmt->kind) {
        case STMT_VAR_DECL:
            printf("VarDecl %s\n", wf_type_name(stmt->as.var_decl.type));
            indent(depth + 1);
            printf("%.*s =\n", stmt->as.var_decl.name_len, stmt->as.var_decl.name);
            dump_expr(stmt->as.var_decl.init, depth + 2);
            break;
        case STMT_ASSIGN:
            printf("Assign %.*s =\n", stmt->as.assign.name_len, stmt->as.assign.name);
            dump_expr(stmt->as.assign.value, depth + 1);
            break;
        case STMT_IF:
            printf("If\n");
            dump_expr(stmt->as.if_stmt.cond, depth + 1);
            dump_stmt(stmt->as.if_stmt.then_branch, depth + 1);
            if (stmt->as.if_stmt.else_branch != NULL) {
                indent(depth);
                printf("Else\n");
                dump_stmt(stmt->as.if_stmt.else_branch, depth + 1);
            }
            break;
        case STMT_WHILE:
            printf("While\n");
            dump_expr(stmt->as.while_stmt.cond, depth + 1);
            dump_stmt(stmt->as.while_stmt.body, depth + 1);
            break;
        case STMT_RETURN:
            printf("Return\n");
            if (stmt->as.return_stmt.value != NULL) {
                dump_expr(stmt->as.return_stmt.value, depth + 1);
            }
            break;
        case STMT_EXPR:
            printf("ExprStmt\n");
            dump_expr(stmt->as.expr_stmt.expr, depth + 1);
            break;
        case STMT_BLOCK:
            printf("Block\n");
            for (int i = 0; i < stmt->as.block.count; i++) {
                dump_stmt(stmt->as.block.stmts[i], depth + 1);
            }
            break;
    }
}

void ast_dump_program(const AstProgram *program) {
    printf("Program\n");
    for (int i = 0; i < program->count; i++) {
        const FunctionDecl *func = &program->funcs[i];
        indent(1);
        printf("FuncDecl %.*s(", func->name_len, func->name);
        for (int p = 0; p < func->param_count; p++) {
            if (p > 0) printf(", ");
            printf("%s %.*s", wf_type_name(func->params[p].type), func->params[p].name_len,
                   func->params[p].name);
        }
        printf(") -> %s\n", wf_type_name(func->return_type));
        dump_stmt(func->body, 2);
    }
}
