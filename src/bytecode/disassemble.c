#include "disassemble.h"

#include <stdio.h>

#include "opcode.h"

static const char *op_name(OpCode op) {
    switch (op) {
        case OP_PUSH_CONST: return "PUSH_CONST";
        case OP_PUSH_TRUE: return "PUSH_TRUE";
        case OP_PUSH_FALSE: return "PUSH_FALSE";
        case OP_POP: return "POP";
        case OP_POP_N: return "POP_N";
        case OP_DUP: return "DUP";
        case OP_LOAD_LOCAL: return "LOAD_LOCAL";
        case OP_STORE_LOCAL: return "STORE_LOCAL";
        case OP_IADD: return "IADD";
        case OP_ISUB: return "ISUB";
        case OP_IMUL: return "IMUL";
        case OP_IDIV: return "IDIV";
        case OP_IMOD: return "IMOD";
        case OP_FADD: return "FADD";
        case OP_FSUB: return "FSUB";
        case OP_FMUL: return "FMUL";
        case OP_FDIV: return "FDIV";
        case OP_INEG: return "INEG";
        case OP_FNEG: return "FNEG";
        case OP_IEQ: return "IEQ";
        case OP_INE: return "INE";
        case OP_ILT: return "ILT";
        case OP_IGT: return "IGT";
        case OP_ILE: return "ILE";
        case OP_IGE: return "IGE";
        case OP_FEQ: return "FEQ";
        case OP_FNE: return "FNE";
        case OP_FLT: return "FLT";
        case OP_FGT: return "FGT";
        case OP_FLE: return "FLE";
        case OP_FGE: return "FGE";
        case OP_NOT: return "NOT";
        case OP_JUMP: return "JUMP";
        case OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OP_JUMP_IF_TRUE: return "JUMP_IF_TRUE";
        case OP_LOOP: return "LOOP";
        case OP_CALL: return "CALL";
        case OP_RETURN: return "RETURN";
        case OP_RETURN_VOID: return "RETURN_VOID";
        case OP_PRINT_INT: return "PRINT_INT";
        case OP_PRINT_FLOAT: return "PRINT_FLOAT";
        case OP_PRINT_BOOL: return "PRINT_BOOL";
        case OP_PRINT_CHAR: return "PRINT_CHAR";
    }
    return "?";
}

static int disassemble_instruction(const Chunk *chunk, int offset) {
    OpCode op = (OpCode)chunk->code[offset];
    printf("%4d  %-14s", offset, op_name(op));
    switch (op) {
        case OP_PUSH_CONST:
        case OP_POP_N:
        case OP_LOAD_LOCAL:
        case OP_STORE_LOCAL:
            printf(" %d\n", chunk->code[offset + 1]);
            return offset + 2;
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_TRUE: {
            int rel = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
            printf(" +%d -> %d\n", rel, offset + 3 + rel);
            return offset + 3;
        }
        case OP_LOOP: {
            int rel = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
            printf(" -%d -> %d\n", rel, offset + 3 - rel);
            return offset + 3;
        }
        case OP_CALL:
            printf(" func=%d argc=%d\n", chunk->code[offset + 1], chunk->code[offset + 2]);
            return offset + 3;
        default:
            printf("\n");
            return offset + 1;
    }
}

void disassemble_program(const Program *program) {
    for (int f = 0; f < program->function_count; f++) {
        const Chunk *chunk = &program->functions[f];
        printf("== %.*s (arity %d)%s ==\n", chunk->name_len, chunk->name, chunk->arity,
               f == program->start_index ? " [start]" : "");

        if (chunk->constant_count > 0) {
            printf("constants:\n");
            for (int k = 0; k < chunk->constant_count; k++) {
                printf("  [%d] as int=%d as float=%g\n", k, chunk->constants[k].i,
                       chunk->constants[k].f);
            }
        }

        int offset = 0;
        while (offset < chunk->code_count) {
            offset = disassemble_instruction(chunk, offset);
        }
        printf("\n");
    }
}
