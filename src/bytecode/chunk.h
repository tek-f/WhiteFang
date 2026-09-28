#ifndef WHITEFANG_CHUNK_H
#define WHITEFANG_CHUNK_H

#include <stdint.h>

#include "value.h"

/* One function's compiled bytecode + its own constant pool
 * (docs/VM.md sec 3, 7). name/name_len is a slice into the source,
 * same non-owning convention as Token/AST names -- not the literal
 * null-terminated `const char*` VM.md's sketch showed, since function
 * names come from Tokens that were never null-terminated to begin
 * with; printed with "%.*s" wherever needed. */
typedef struct {
    uint8_t *code;
    int code_count;
    int code_cap;

    Value *constants;
    int constant_count;
    int constant_cap;

    int arity; /* debug aid only -- CALL's own arg_count operand is load-bearing */
    const char *name;
    int name_len;
} Chunk;

/* One chunk per function, indexed by CALL's func_idx (docs/VM.md
 * sec 7). There are no global variables in M0 (docs/DECISIONS.md,
 * "Global variables removed"), so unlike VM.md's original sketch
 * there is no globals_init/global_count here either. */
typedef struct {
    Chunk *functions;
    int function_count;
    int function_cap;
    int start_index; /* which functions[] entry is `start` */
} Program;

void chunk_init(Chunk *chunk, const char *name, int name_len, int arity);

/* Appends value to chunk's constant pool, returning its index.
 * Fatal (exit 65) if the pool would exceed 255 entries -- PUSH_CONST's
 * operand is a u8 (docs/VM.md sec 3). */
int chunk_add_constant(Chunk *chunk, Value value);

/* Appends one byte, returning the offset it was written to. */
int chunk_write_byte(Chunk *chunk, uint8_t byte);

/* Appends a placeholder u16 (big-endian), returning the offset of its
 * first byte -- for jump operands, patched once the real target is
 * known via chunk_patch_u16. */
int chunk_write_u16(Chunk *chunk, uint16_t value);
void chunk_patch_u16(Chunk *chunk, int offset, uint16_t value);

void program_init(Program *program);

/* Appends a new, empty Chunk and returns its index (the new
 * function's func_idx). The caller fills in code/constants via the
 * returned pointer's target -- see program_chunk. */
int program_add_chunk(Program *program, const char *name, int name_len, int arity);
Chunk *program_chunk(Program *program, int func_idx);

#endif
