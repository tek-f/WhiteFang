#include "chunk.h"

#include <stdio.h>
#include <stdlib.h>

#include "dynarray.h"

static void fatal(const char *message) {
    fprintf(stderr, "compile error: %s\n", message);
    exit(65);
}

void chunk_init(Chunk *chunk, const char *name, int name_len, int arity) {
    chunk->code = NULL;
    chunk->code_count = 0;
    chunk->code_cap = 0;
    chunk->constants = NULL;
    chunk->constant_count = 0;
    chunk->constant_cap = 0;
    chunk->arity = arity;
    chunk->name = name;
    chunk->name_len = name_len;
}

int chunk_add_constant(Chunk *chunk, Value value) {
    if (chunk->constant_count >= 255) {
        fatal("Too many constants in one function (max 255).");
    }
    DA_APPEND(chunk->constants, chunk->constant_cap, chunk->constant_count, value);
    return chunk->constant_count - 1;
}

int chunk_write_byte(Chunk *chunk, uint8_t byte) {
    DA_APPEND(chunk->code, chunk->code_cap, chunk->code_count, byte);
    return chunk->code_count - 1;
}

int chunk_write_u16(Chunk *chunk, uint16_t value) {
    int offset = chunk_write_byte(chunk, (uint8_t)((value >> 8) & 0xFF));
    chunk_write_byte(chunk, (uint8_t)(value & 0xFF));
    return offset;
}

void chunk_patch_u16(Chunk *chunk, int offset, uint16_t value) {
    chunk->code[offset] = (uint8_t)((value >> 8) & 0xFF);
    chunk->code[offset + 1] = (uint8_t)(value & 0xFF);
}

void program_init(Program *program) {
    program->functions = NULL;
    program->function_count = 0;
    program->function_cap = 0;
    program->start_index = -1;
}

int program_add_chunk(Program *program, const char *name, int name_len, int arity) {
    if (program->function_count >= 255) {
        fatal("Too many functions in one program (max 255).");
    }
    Chunk chunk;
    chunk_init(&chunk, name, name_len, arity);
    DA_APPEND(program->functions, program->function_cap, program->function_count, chunk);
    return program->function_count - 1;
}

Chunk *program_chunk(Program *program, int func_idx) { return &program->functions[func_idx]; }
