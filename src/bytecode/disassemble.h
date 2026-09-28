#ifndef WHITEFANG_DISASSEMBLE_H
#define WHITEFANG_DISASSEMBLE_H

#include "chunk.h"

/* Prints every function's bytecode as a human-readable listing --
 * manual verification tool for M3's compiler, same role as lexdump/
 * parsedump had for M1/M2. Not needed by the VM itself. */
void disassemble_program(const Program *program);

#endif
