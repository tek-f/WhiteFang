#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dynarray.h"
#include "opcode.h"

/* ---- name matching (names are non-owning slices, not
 * null-terminated -- same convention as Token/AST names) ---- */

static int names_eq(const char *a, int a_len, const char *b, int b_len) {
    return a_len == b_len && memcmp(a, b, (size_t)a_len) == 0;
}

/* ---- errors: stop on first, no recovery (docs/DECISIONS.md) ---- */

static void fatal(int line, const char *message) {
    fprintf(stderr, "[line %d] compile error: %s\n", line, message);
    exit(65);
}

static void fatal_no_line(const char *message) {
    fprintf(stderr, "compile error: %s\n", message);
    exit(65);
}

/* ---- function signature table (pass 1) ----
 *
 * Every function's signature is collected before any body is
 * compiled, so calls resolve regardless of declaration order --
 * including recursion and mutual recursion. See docs/DECISIONS.md,
 * "Compilation structure". */

typedef struct {
    const char *name;
    int name_len;
    WfType *param_types;
    int param_count;
    WfType return_type;
    int func_idx;
} FuncSig;

typedef struct {
    FuncSig *entries;
    int count;
    int cap;
} SigTable;

static const FuncSig *find_signature(const SigTable *sigs, const char *name, int name_len) {
    for (int i = 0; i < sigs->count; i++) {
        if (names_eq(sigs->entries[i].name, sigs->entries[i].name_len, name, name_len)) {
            return &sigs->entries[i];
        }
    }
    return NULL;
}

static SigTable build_signatures(const AstProgram *ast, Program *program) {
    SigTable sigs;
    sigs.entries = NULL;
    sigs.count = 0;
    sigs.cap = 0;

    for (int i = 0; i < ast->count; i++) {
        const FunctionDecl *decl = &ast->funcs[i];

        if (find_signature(&sigs, decl->name, decl->name_len) != NULL) {
            fatal(decl->line, "Duplicate function name.");
        }

        int is_start = names_eq(decl->name, decl->name_len, "start", 5);
        if (is_start && (decl->param_count != 0 || decl->return_type != WF_TYPE_INT)) {
            fatal(decl->line, "'start' must take no parameters and return int.");
        }
        if (decl->param_count > 255) {
            fatal(decl->line, "Too many parameters (max 255).");
        }

        FuncSig sig;
        sig.name = decl->name;
        sig.name_len = decl->name_len;
        sig.return_type = decl->return_type;
        sig.param_count = decl->param_count;
        sig.param_types =
            decl->param_count > 0 ? malloc(sizeof(WfType) * (size_t)decl->param_count) : NULL;
        for (int p = 0; p < decl->param_count; p++) {
            if (decl->params[p].type == WF_TYPE_VOID) {
                fatal(decl->line, "A parameter cannot have type void.");
            }
            sig.param_types[p] = decl->params[p].type;
        }
        sig.func_idx = program_add_chunk(program, decl->name, decl->name_len, decl->param_count);

        DA_APPEND(sigs.entries, sigs.cap, sigs.count, sig);

        if (is_start) {
            program->start_index = sig.func_idx;
        }
    }

    if (program->start_index < 0) {
        fatal_no_line("Program has no 'start' function.");
    }

    return sigs;
}

/* ---- per-function compile state: locals + scope tracking ----
 *
 * A flat array of Local{name,type,slot,depth} plus a scope_depth
 * counter mirrors the VM's frame-relative locals exactly -- see
 * docs/DECISIONS.md, "Symbol table / local scope tracking". Reset for
 * each function (locals never cross function boundaries); a
 * function's parameters are added first, occupying slots 0..N-1. */

typedef struct {
    const char *name;
    int name_len;
    WfType type;
    int slot;
    int depth;
} Local;

typedef struct {
    Chunk *chunk;
    const SigTable *sigs;

    Local *locals;
    int local_count;
    int local_cap;
    int scope_depth;

    WfType return_type; /* the function currently being compiled */
} Compiler;

static const Local *find_local(Compiler *c, const char *name, int name_len) {
    for (int i = c->local_count - 1; i >= 0; i--) {
        if (names_eq(c->locals[i].name, c->locals[i].name_len, name, name_len)) {
            return &c->locals[i];
        }
    }
    return NULL;
}

/* Same-block redeclaration is a compile error (docs/DECISIONS.md);
 * shadowing an outer block's local is fine (that's the normal case). */
static void add_local(Compiler *c, const char *name, int name_len, WfType type, int line) {
    for (int i = c->local_count - 1; i >= 0 && c->locals[i].depth == c->scope_depth; i--) {
        if (names_eq(c->locals[i].name, c->locals[i].name_len, name, name_len)) {
            fatal(line, "A variable with this name already exists in this scope.");
        }
    }
    if (c->local_count >= 255) {
        fatal(line, "Too many local variables in one function (max 255).");
    }
    Local local;
    local.name = name;
    local.name_len = name_len;
    local.type = type;
    local.slot = c->local_count;
    local.depth = c->scope_depth;
    DA_APPEND(c->locals, c->local_cap, c->local_count, local);
}

static void begin_scope(Compiler *c) { c->scope_depth++; }

/* Pops every local declared at the depth just left. The count popped
 * *is* POP_N's operand, falling out of this bookkeeping for free --
 * see docs/DECISIONS.md. Prefers plain POP for the single-local case.
 *
 * skip_emit is set when the block's last statement was already a
 * `return`: RETURN unconditionally truncates the stack to frame_base
 * itself (docs/VM.md sec 4, "return bypasses block bookkeeping
 * entirely"), so any cleanup instruction emitted here would be dead
 * code -- control has already left before it could ever run. The
 * local-count bookkeeping below still has to happen either way, for
 * the compiler's own slot accounting in whatever follows this block. */
static void end_scope(Compiler *c, int skip_emit) {
    c->scope_depth--;
    int popped = 0;
    while (c->local_count > 0 && c->locals[c->local_count - 1].depth > c->scope_depth) {
        c->local_count--;
        popped++;
    }
    if (skip_emit) return;
    if (popped == 1) {
        chunk_write_byte(c->chunk, (uint8_t)OP_POP);
    } else if (popped > 1) {
        chunk_write_byte(c->chunk, (uint8_t)OP_POP_N);
        chunk_write_byte(c->chunk, (uint8_t)popped);
    }
}

/* ---- emit helpers ---- */

static void emit_op(Compiler *c, OpCode op) { chunk_write_byte(c->chunk, (uint8_t)op); }

static void emit_op_u8(Compiler *c, OpCode op, uint8_t operand) {
    chunk_write_byte(c->chunk, (uint8_t)op);
    chunk_write_byte(c->chunk, operand);
}

/* Forward jumps: writes the opcode + a placeholder u16 operand,
 * returning the operand's offset for patch_jump to fill in once the
 * target is known. */
static int emit_jump(Compiler *c, OpCode op) {
    chunk_write_byte(c->chunk, (uint8_t)op);
    return chunk_write_u16(c->chunk, 0xFFFF);
}

static void patch_jump(Compiler *c, int operand_offset) {
    int distance = c->chunk->code_count - (operand_offset + 2);
    if (distance > 0xFFFF) {
        fatal_no_line("Jump too far (function body too large).");
    }
    chunk_patch_u16(c->chunk, operand_offset, (uint16_t)distance);
}

/* Backward jump: LOOP always subtracts (docs/VM.md sec 6), so no
 * placeholder/patch is needed -- the distance is already known. */
static void emit_loop(Compiler *c, int loop_start) {
    chunk_write_byte(c->chunk, (uint8_t)OP_LOOP);
    int operand_offset = chunk_write_u16(c->chunk, 0);
    int distance = (operand_offset + 2) - loop_start;
    if (distance > 0xFFFF) {
        fatal_no_line("Loop body too large.");
    }
    chunk_patch_u16(c->chunk, operand_offset, (uint16_t)distance);
}

/* ---- expressions: bottom-up type computation + codegen ----
 *
 * See docs/DECISIONS.md, "Type-checking algorithm": every binary
 * operator requires identical operand types (no implicit int/float
 * mixing); arithmetic is int/float only; comparison covers
 * int/float/bool/char; logical is bool only. */

static WfType compile_expr(Compiler *c, const Expr *expr);

static WfType compile_binary(Compiler *c, const Expr *expr) {
    BinOp op = expr->as.binary.op;

    /* && / || short-circuit: the right side is only compiled/executed
     * conditionally, so it can't go through the eager
     * compile-both-sides path below. See docs/VM.md sec 6. */
    if (op == BIN_AND || op == BIN_OR) {
        WfType lt = compile_expr(c, expr->as.binary.left);
        if (lt != WF_TYPE_BOOL) {
            fatal(expr->line, "'&&' and '||' require bool operands.");
        }
        emit_op(c, OP_DUP);
        int jump = emit_jump(c, op == BIN_AND ? OP_JUMP_IF_FALSE : OP_JUMP_IF_TRUE);
        emit_op(c, OP_POP);
        WfType rt = compile_expr(c, expr->as.binary.right);
        if (rt != WF_TYPE_BOOL) {
            fatal(expr->line, "'&&' and '||' require bool operands.");
        }
        patch_jump(c, jump);
        return WF_TYPE_BOOL;
    }

    WfType lt = compile_expr(c, expr->as.binary.left);
    WfType rt = compile_expr(c, expr->as.binary.right);
    if (lt != rt) {
        fatal(expr->line, "Both operands of a binary operator must have the same type.");
    }
    WfType t = lt;

    if (op == BIN_ADD || op == BIN_SUB || op == BIN_MUL || op == BIN_DIV || op == BIN_MOD) {
        if (t != WF_TYPE_INT && t != WF_TYPE_FLOAT) {
            fatal(expr->line, "Arithmetic requires int or float operands.");
        }
        if (op == BIN_MOD && t != WF_TYPE_INT) {
            fatal(expr->line, "'%' requires int operands.");
        }
        OpCode arith_op;
        if (t == WF_TYPE_INT) {
            switch (op) {
                case BIN_ADD: arith_op = OP_IADD; break;
                case BIN_SUB: arith_op = OP_ISUB; break;
                case BIN_MUL: arith_op = OP_IMUL; break;
                case BIN_DIV: arith_op = OP_IDIV; break;
                default: arith_op = OP_IMOD; break;
            }
        } else {
            switch (op) {
                case BIN_ADD: arith_op = OP_FADD; break;
                case BIN_SUB: arith_op = OP_FSUB; break;
                case BIN_MUL: arith_op = OP_FMUL; break;
                default: arith_op = OP_FDIV; break;
            }
        }
        emit_op(c, arith_op);
        return t;
    }

    /* Comparison: int/float/bool/char (docs/VM.md sec 6), same type
     * on both sides already enforced above. */
    OpCode cmp_op;
    if (t == WF_TYPE_FLOAT) {
        switch (op) {
            case BIN_EQ: cmp_op = OP_FEQ; break;
            case BIN_NE: cmp_op = OP_FNE; break;
            case BIN_LT: cmp_op = OP_FLT; break;
            case BIN_GT: cmp_op = OP_FGT; break;
            case BIN_LE: cmp_op = OP_FLE; break;
            default: cmp_op = OP_FGE; break;
        }
    } else {
        switch (op) {
            case BIN_EQ: cmp_op = OP_IEQ; break;
            case BIN_NE: cmp_op = OP_INE; break;
            case BIN_LT: cmp_op = OP_ILT; break;
            case BIN_GT: cmp_op = OP_IGT; break;
            case BIN_LE: cmp_op = OP_ILE; break;
            default: cmp_op = OP_IGE; break;
        }
    }
    emit_op(c, cmp_op);
    return WF_TYPE_BOOL;
}

static WfType compile_expr(Compiler *c, const Expr *expr) {
    switch (expr->kind) {
        case EXPR_INT_LIT: {
            Value v;
            v.i = expr->as.int_lit;
            emit_op_u8(c, OP_PUSH_CONST, (uint8_t)chunk_add_constant(c->chunk, v));
            return WF_TYPE_INT;
        }
        case EXPR_FLOAT_LIT: {
            Value v;
            v.f = expr->as.float_lit;
            emit_op_u8(c, OP_PUSH_CONST, (uint8_t)chunk_add_constant(c->chunk, v));
            return WF_TYPE_FLOAT;
        }
        case EXPR_BOOL_LIT:
            emit_op(c, expr->as.bool_lit ? OP_PUSH_TRUE : OP_PUSH_FALSE);
            return WF_TYPE_BOOL;
        case EXPR_CHAR_LIT: {
            Value v;
            v.i = (int32_t)(unsigned char)expr->as.char_lit;
            emit_op_u8(c, OP_PUSH_CONST, (uint8_t)chunk_add_constant(c->chunk, v));
            return WF_TYPE_CHAR;
        }
        case EXPR_IDENTIFIER: {
            const Local *local = find_local(c, expr->as.identifier.name, expr->as.identifier.name_len);
            if (local == NULL) {
                fatal(expr->line, "Undeclared variable.");
            }
            emit_op_u8(c, OP_LOAD_LOCAL, (uint8_t)local->slot);
            return local->type;
        }
        case EXPR_UNARY: {
            WfType t = compile_expr(c, expr->as.unary.operand);
            if (expr->as.unary.op == UN_NEG) {
                if (t != WF_TYPE_INT && t != WF_TYPE_FLOAT) {
                    fatal(expr->line, "Unary '-' requires an int or float operand.");
                }
                emit_op(c, t == WF_TYPE_INT ? OP_INEG : OP_FNEG);
                return t;
            }
            /* UN_NOT */
            if (t != WF_TYPE_BOOL) {
                fatal(expr->line, "Unary '!' requires a bool operand.");
            }
            emit_op(c, OP_NOT);
            return WF_TYPE_BOOL;
        }
        case EXPR_BINARY:
            return compile_binary(c, expr);
        case EXPR_CALL: {
            const FuncSig *sig = find_signature(c->sigs, expr->as.call.name, expr->as.call.name_len);
            if (sig == NULL) {
                fatal(expr->line, "Call to undeclared function.");
            }
            if (expr->as.call.arg_count != sig->param_count) {
                fatal(expr->line, "Wrong number of arguments in call.");
            }
            for (int i = 0; i < expr->as.call.arg_count; i++) {
                WfType at = compile_expr(c, expr->as.call.args[i]);
                if (at != sig->param_types[i]) {
                    fatal(expr->line, "Argument type does not match parameter type.");
                }
            }
            emit_op(c, OP_CALL);
            chunk_write_byte(c->chunk, (uint8_t)sig->func_idx);
            chunk_write_byte(c->chunk, (uint8_t)expr->as.call.arg_count);
            return sig->return_type;
        }
        case EXPR_PRINT: {
            WfType t = compile_expr(c, expr->as.print.arg);
            switch (t) {
                case WF_TYPE_INT: emit_op(c, OP_PRINT_INT); break;
                case WF_TYPE_FLOAT: emit_op(c, OP_PRINT_FLOAT); break;
                case WF_TYPE_BOOL: emit_op(c, OP_PRINT_BOOL); break;
                case WF_TYPE_CHAR: emit_op(c, OP_PRINT_CHAR); break;
                case WF_TYPE_VOID:
                default:
                    fatal(expr->line, "PRINT requires an int, float, bool, or char argument.");
            }
            return WF_TYPE_VOID;
        }
    }
    fatal_no_line("internal error: unhandled expression kind.");
    return WF_TYPE_VOID; /* unreachable */
}

/* ---- statements ---- */

static void compile_stmt(Compiler *c, const Stmt *stmt);

/* True if a block's literal last top-level statement is a return --
 * see docs/DECISIONS.md, "Missing-return check is syntactic, not full
 * control-flow analysis". Also used by end_scope's skip_emit above. */
static int block_ends_in_return(const Stmt *block) {
    if (block->as.block.count == 0) return 0;
    return block->as.block.stmts[block->as.block.count - 1]->kind == STMT_RETURN;
}

static void compile_block(Compiler *c, const Stmt *block) {
    begin_scope(c);
    for (int i = 0; i < block->as.block.count; i++) {
        compile_stmt(c, block->as.block.stmts[i]);
    }
    end_scope(c, block_ends_in_return(block));
}

static void compile_stmt(Compiler *c, const Stmt *stmt) {
    switch (stmt->kind) {
        case STMT_VAR_DECL: {
            if (stmt->as.var_decl.type == WF_TYPE_VOID) {
                fatal(stmt->line, "A variable cannot have type void.");
            }
            WfType t = compile_expr(c, stmt->as.var_decl.init);
            if (t != stmt->as.var_decl.type) {
                fatal(stmt->line, "Variable initializer type does not match declared type.");
            }
            /* The init expression's pushed value *is* this local's
             * slot -- no extra instruction (docs/VM.md sec 4). */
            add_local(c, stmt->as.var_decl.name, stmt->as.var_decl.name_len, t, stmt->line);
            break;
        }
        case STMT_ASSIGN: {
            const Local *local = find_local(c, stmt->as.assign.name, stmt->as.assign.name_len);
            if (local == NULL) {
                fatal(stmt->line, "Undeclared variable.");
            }
            WfType t = compile_expr(c, stmt->as.assign.value);
            if (t != local->type) {
                fatal(stmt->line, "Assigned value's type does not match variable's declared type.");
            }
            emit_op_u8(c, OP_STORE_LOCAL, (uint8_t)local->slot);
            break;
        }
        case STMT_IF: {
            WfType cond_t = compile_expr(c, stmt->as.if_stmt.cond);
            if (cond_t != WF_TYPE_BOOL) {
                fatal(stmt->line, "'if' condition must be bool.");
            }
            int else_jump = emit_jump(c, OP_JUMP_IF_FALSE);
            compile_stmt(c, stmt->as.if_stmt.then_branch);
            if (stmt->as.if_stmt.else_branch != NULL) {
                int end_jump = emit_jump(c, OP_JUMP);
                patch_jump(c, else_jump);
                compile_stmt(c, stmt->as.if_stmt.else_branch);
                patch_jump(c, end_jump);
            } else {
                patch_jump(c, else_jump);
            }
            break;
        }
        case STMT_WHILE: {
            int loop_start = c->chunk->code_count;
            WfType cond_t = compile_expr(c, stmt->as.while_stmt.cond);
            if (cond_t != WF_TYPE_BOOL) {
                fatal(stmt->line, "'while' condition must be bool.");
            }
            int exit_jump = emit_jump(c, OP_JUMP_IF_FALSE);
            compile_stmt(c, stmt->as.while_stmt.body);
            emit_loop(c, loop_start);
            patch_jump(c, exit_jump);
            break;
        }
        case STMT_RETURN: {
            if (stmt->as.return_stmt.value == NULL) {
                if (c->return_type != WF_TYPE_VOID) {
                    fatal(stmt->line, "Non-void function must return a value.");
                }
                emit_op(c, OP_RETURN_VOID);
            } else {
                if (c->return_type == WF_TYPE_VOID) {
                    fatal(stmt->line, "Void function cannot return a value.");
                }
                WfType t = compile_expr(c, stmt->as.return_stmt.value);
                if (t != c->return_type) {
                    fatal(stmt->line, "Return type does not match function's declared return type.");
                }
                emit_op(c, OP_RETURN);
            }
            break;
        }
        case STMT_EXPR: {
            WfType t = compile_expr(c, stmt->as.expr_stmt.expr);
            if (t != WF_TYPE_VOID) {
                emit_op(c, OP_POP); /* discard unused result, docs/VM.md sec 4 */
            }
            break;
        }
        case STMT_BLOCK:
            compile_block(c, stmt);
            break;
    }
}

/* ---- top level ---- */

Program *compile_program(const AstProgram *ast) {
    Program *program = malloc(sizeof(Program));
    program_init(program);

    SigTable sigs = build_signatures(ast, program);

    Compiler c;
    c.sigs = &sigs;
    c.locals = NULL;
    c.local_cap = 0;

    for (int i = 0; i < ast->count; i++) {
        const FunctionDecl *decl = &ast->funcs[i];
        const FuncSig *sig = find_signature(&sigs, decl->name, decl->name_len);

        c.chunk = program_chunk(program, sig->func_idx);
        c.local_count = 0;
        c.scope_depth = 0;
        c.return_type = decl->return_type;

        for (int p = 0; p < decl->param_count; p++) {
            add_local(&c, decl->params[p].name, decl->params[p].name_len, decl->params[p].type,
                      decl->line);
        }

        compile_stmt(&c, decl->body);

        int ends_in_return = block_ends_in_return(decl->body);
        if (decl->return_type == WF_TYPE_VOID) {
            if (!ends_in_return) {
                emit_op(&c, OP_RETURN_VOID);
            }
        } else if (!ends_in_return) {
            fatal(decl->line, "Non-void function may fall off the end without returning a value.");
        }
    }

    return program;
}
