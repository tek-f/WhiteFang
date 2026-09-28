#ifndef WHITEFANG_OPCODE_H
#define WHITEFANG_OPCODE_H

/* Full opcode set, see docs/VM.md sec 6. Operand widths are noted per
 * opcode below; all operands are encoded big-endian where wider than
 * one byte. */
typedef enum {
    OP_PUSH_CONST,    /* u8 idx */
    OP_PUSH_TRUE,
    OP_PUSH_FALSE,

    OP_POP,
    OP_POP_N,         /* u8 count */
    OP_DUP,

    OP_LOAD_LOCAL,    /* u8 idx, frame-relative */
    OP_STORE_LOCAL,   /* u8 idx, frame-relative */

    OP_IADD,
    OP_ISUB,
    OP_IMUL,
    OP_IDIV,
    OP_IMOD,

    OP_FADD,
    OP_FSUB,
    OP_FMUL,
    OP_FDIV,

    OP_INEG,
    OP_FNEG,

    OP_IEQ,
    OP_INE,
    OP_ILT,
    OP_IGT,
    OP_ILE,
    OP_IGE,

    OP_FEQ,
    OP_FNE,
    OP_FLT,
    OP_FGT,
    OP_FLE,
    OP_FGE,

    OP_NOT,

    OP_JUMP,           /* u16 offset, forward */
    OP_JUMP_IF_FALSE,  /* u16 offset, forward */
    OP_JUMP_IF_TRUE,   /* u16 offset, forward */
    OP_LOOP,           /* u16 offset, backward (subtracted) */

    OP_CALL,           /* u8 func_idx, u8 arg_count */
    OP_RETURN,
    OP_RETURN_VOID,

    OP_PRINT_INT,
    OP_PRINT_FLOAT,
    OP_PRINT_BOOL,
    OP_PRINT_CHAR,
} OpCode;

#endif
