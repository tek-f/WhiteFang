#ifndef WHITEFANG_VM_H
#define WHITEFANG_VM_H

#include "chunk.h"

/* Runs a compiled Program starting at functions[start_index].
 * Returns start's own return value, which becomes the process exit
 * code (docs/VM.md sec 7). Runtime errors (stack overflow, division
 * by zero) are reported to stderr and exit the process with status
 * 70 -- distinct from the compiler/parser's 65, since these are
 * runtime failures, not source-level mistakes. */
int vm_run(const Program *program);

#endif
