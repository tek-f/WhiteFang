#ifndef WHITEFANG_DYNARRAY_H
#define WHITEFANG_DYNARRAY_H

#include <stdlib.h>

/* Minimal growable-array append: doubles cap as needed, then stores
 * item at arr[count++]. Used throughout ast.c/parser.c for the
 * handful of small array fields (decls, statements, params, call
 * args, var bindings) -- one macro instead of five hand-rolled
 * dynamic arrays. See docs/DECISIONS.md. */
#define DA_APPEND(arr, cap, count, item)                           \
    do {                                                           \
        if ((count) >= (cap)) {                                    \
            (cap) = (cap) == 0 ? 8 : (cap) * 2;                     \
            (arr) = realloc((arr), sizeof(*(arr)) * (size_t)(cap)); \
        }                                                           \
        (arr)[(count)++] = (item);                                  \
    } while (0)

#endif
