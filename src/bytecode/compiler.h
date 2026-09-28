#ifndef WHITEFANG_COMPILER_H
#define WHITEFANG_COMPILER_H

#include "ast.h"
#include "chunk.h"

/* Compiles a parsed AstProgram into a bytecode Program (docs/VM.md).
 * This is also where every semantic check M2's parser deliberately
 * left undone happens: undeclared names, duplicate declarations, type
 * mismatches, wrong argument counts, a missing/malformed `start`, a
 * non-void function falling off the end (docs/DECISIONS.md).
 *
 * On the first such error, reports it to stderr and exits the process
 * with status 65 -- no recovery, same convention as the parser
 * (docs/DECISIONS.md, "stop on first error"). */
Program *compile_program(const AstProgram *ast);

#endif
