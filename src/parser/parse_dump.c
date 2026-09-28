/* Manual verification tool for M2: reads a .wf file, parses it, and
 * prints the resulting AST as an indented tree. Not a golden test
 * (those start at M3) -- a debugging aid for eyeballing the parser's
 * output during development, same role as the lexer's lex_dump.c. */

#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "parser.h"

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytes_read = fread(buffer, 1, (size_t)size, file);
    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: parsedump <path.wf>\n");
        return 64;
    }

    char *source = read_file(argv[1]);

    Parser parser;
    parser_init(&parser, source);
    /* parser_parse_program exits(65) itself on the first syntax
     * error -- see docs/DECISIONS.md ("stop on first error") -- so
     * reaching this line means the whole program parsed cleanly. */
    AstProgram *program = parser_parse_program(&parser);
    ast_dump_program(program);

    free(source);
    return 0;
}
