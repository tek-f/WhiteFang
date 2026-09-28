#include "vm.h"

#include <stdio.h>
#include <stdlib.h>

#include "opcode.h"

/* "Generously-sized fixed operand stack shared by the whole program"
 * (docs/VM.md sec 7) -- no precomputed per-chunk max, deliberately. */
#define STACK_MAX 65536
/* Suggested starting point from docs/VM.md sec 4. */
#define FRAMES_MAX 256

/* return_chunk/return_ip describe where to resume once *this* frame
 * returns (i.e. back in whichever chunk called it); frame_base is
 * where *this* frame's own locals begin on the shared stack. Both are
 * set once, at CALL time, and read once, at this frame's own RETURN
 * -- see docs/DECISIONS.md. */
typedef struct {
    const Chunk *return_chunk;
    int return_ip;
    int frame_base;
} Frame;

typedef struct {
    Value stack[STACK_MAX];
    int stack_top;

    Frame frames[FRAMES_MAX];
    int frame_count;

    const Chunk *chunk; /* chunk currently executing */
    int ip;
    int frame_base; /* cached copy of frames[frame_count-1].frame_base */

    const Program *program;
} VM;

static void runtime_error(const char *message) {
    fprintf(stderr, "runtime error: %s\n", message);
    exit(70);
}

static void push(VM *vm, Value value) {
    if (vm->stack_top >= STACK_MAX) {
        runtime_error("Stack overflow.");
    }
    vm->stack[vm->stack_top++] = value;
}

static Value pop(VM *vm) { return vm->stack[--vm->stack_top]; }

static uint8_t read_byte(VM *vm) { return vm->chunk->code[vm->ip++]; }

static uint16_t read_u16(VM *vm) {
    uint16_t hi = vm->chunk->code[vm->ip];
    uint16_t lo = vm->chunk->code[vm->ip + 1];
    vm->ip += 2;
    return (uint16_t)((hi << 8) | lo);
}

int vm_run(const Program *program) {
    VM vm;
    vm.program = program;
    vm.stack_top = 0;

    /* Treat `start` as if it were itself the result of an initial
     * "call": frame[0] is a synthetic frame with frame_base 0. Its
     * return_chunk/return_ip are never read -- when this frame's
     * RETURN fires, frame_count reaches 0 and vm_run halts before
     * ever trying to "resume the caller". This makes every RETURN,
     * including start's own, go through identical logic with no
     * special-casing for the outermost call. See docs/DECISIONS.md. */
    vm.frame_count = 1;
    vm.frames[0].return_chunk = NULL;
    vm.frames[0].return_ip = 0;
    vm.frames[0].frame_base = 0;
    vm.frame_base = 0;

    vm.chunk = &program->functions[program->start_index];
    vm.ip = 0;

    for (;;) {
        OpCode instruction = (OpCode)read_byte(&vm);
        switch (instruction) {
            case OP_PUSH_CONST: {
                uint8_t idx = read_byte(&vm);
                push(&vm, vm.chunk->constants[idx]);
                break;
            }
            case OP_PUSH_TRUE:
                push(&vm, (Value){.i = 1});
                break;
            case OP_PUSH_FALSE:
                push(&vm, (Value){.i = 0});
                break;

            case OP_POP:
                vm.stack_top--;
                break;
            case OP_POP_N:
                vm.stack_top -= read_byte(&vm);
                break;
            case OP_DUP:
                push(&vm, vm.stack[vm.stack_top - 1]);
                break;

            case OP_LOAD_LOCAL: {
                uint8_t idx = read_byte(&vm);
                push(&vm, vm.stack[vm.frame_base + idx]);
                break;
            }
            case OP_STORE_LOCAL: {
                uint8_t idx = read_byte(&vm);
                vm.stack[vm.frame_base + idx] = pop(&vm);
                break;
            }

            case OP_IADD: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.i + r.i}); break; }
            case OP_ISUB: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.i - r.i}); break; }
            case OP_IMUL: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.i * r.i}); break; }
            case OP_IDIV: {
                Value r = pop(&vm), l = pop(&vm);
                if (r.i == 0) runtime_error("Division by zero.");
                push(&vm, (Value){.i = l.i / r.i});
                break;
            }
            case OP_IMOD: {
                Value r = pop(&vm), l = pop(&vm);
                if (r.i == 0) runtime_error("Division by zero (in '%').");
                push(&vm, (Value){.i = l.i % r.i});
                break;
            }

            case OP_FADD: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.f = l.f + r.f}); break; }
            case OP_FSUB: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.f = l.f - r.f}); break; }
            case OP_FMUL: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.f = l.f * r.f}); break; }
            case OP_FDIV: {
                /* IEEE 754 float division by zero is well-defined
                 * (+-inf/nan), not undefined behavior -- no check
                 * needed, unlike the integer case above. */
                Value r = pop(&vm), l = pop(&vm);
                push(&vm, (Value){.f = l.f / r.f});
                break;
            }

            case OP_INEG: { Value v = pop(&vm); push(&vm, (Value){.i = -v.i}); break; }
            case OP_FNEG: { Value v = pop(&vm); push(&vm, (Value){.f = -v.f}); break; }

            case OP_IEQ: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.i == r.i}); break; }
            case OP_INE: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.i != r.i}); break; }
            case OP_ILT: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.i < r.i}); break; }
            case OP_IGT: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.i > r.i}); break; }
            case OP_ILE: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.i <= r.i}); break; }
            case OP_IGE: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.i >= r.i}); break; }

            case OP_FEQ: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.f == r.f}); break; }
            case OP_FNE: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.f != r.f}); break; }
            case OP_FLT: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.f < r.f}); break; }
            case OP_FGT: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.f > r.f}); break; }
            case OP_FLE: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.f <= r.f}); break; }
            case OP_FGE: { Value r = pop(&vm), l = pop(&vm); push(&vm, (Value){.i = l.f >= r.f}); break; }

            case OP_NOT: { Value v = pop(&vm); push(&vm, (Value){.i = v.i ? 0 : 1}); break; }

            case OP_JUMP: {
                uint16_t offset = read_u16(&vm);
                vm.ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = read_u16(&vm);
                if (pop(&vm).i == 0) vm.ip += offset;
                break;
            }
            case OP_JUMP_IF_TRUE: {
                uint16_t offset = read_u16(&vm);
                if (pop(&vm).i != 0) vm.ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = read_u16(&vm);
                vm.ip -= offset;
                break;
            }

            case OP_CALL: {
                uint8_t func_idx = read_byte(&vm);
                uint8_t arg_count = read_byte(&vm);
                if (vm.frame_count >= FRAMES_MAX) {
                    runtime_error("Stack overflow (call depth exceeded).");
                }
                Frame *frame = &vm.frames[vm.frame_count++];
                frame->return_chunk = vm.chunk;
                frame->return_ip = vm.ip; /* already past CALL's operands */
                frame->frame_base = vm.stack_top - arg_count;
                vm.frame_base = frame->frame_base;
                vm.chunk = &vm.program->functions[func_idx];
                vm.ip = 0;
                break;
            }
            case OP_RETURN: {
                Value ret = pop(&vm);
                Frame *frame = &vm.frames[vm.frame_count - 1];
                vm.stack_top = frame->frame_base;
                push(&vm, ret);
                vm.frame_count--;
                if (vm.frame_count == 0) {
                    return (int)ret.i;
                }
                vm.chunk = frame->return_chunk;
                vm.ip = frame->return_ip;
                vm.frame_base = vm.frames[vm.frame_count - 1].frame_base;
                break;
            }
            case OP_RETURN_VOID: {
                Frame *frame = &vm.frames[vm.frame_count - 1];
                vm.stack_top = frame->frame_base;
                vm.frame_count--;
                if (vm.frame_count == 0) {
                    return 0; /* unreachable: `start` is always -> int (docs/DECISIONS.md) */
                }
                vm.chunk = frame->return_chunk;
                vm.ip = frame->return_ip;
                vm.frame_base = vm.frames[vm.frame_count - 1].frame_base;
                break;
            }

            case OP_PRINT_INT:
                printf("%d\n", pop(&vm).i);
                break;
            case OP_PRINT_FLOAT:
                printf("%g\n", pop(&vm).f);
                break;
            case OP_PRINT_BOOL:
                printf("%s\n", pop(&vm).i ? "true" : "false");
                break;
            case OP_PRINT_CHAR:
                printf("%c\n", (char)pop(&vm).i);
                break;
        }
    }
}
