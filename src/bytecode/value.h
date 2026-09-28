#ifndef WHITEFANG_VALUE_H
#define WHITEFANG_VALUE_H

#include <stdint.h>

/* Untagged: WhiteFang is statically typed, so the compiler already
 * knows every value's type and encodes that in which opcode it
 * emits (IADD vs FADD, etc.) rather than in the value itself. bool
 * (0/1) and char (its ordinal value) both live in .i, physically
 * indistinguishable from int. See docs/VM.md sec 2. */
typedef union {
    int32_t i;
    double f;
} Value;

#endif
