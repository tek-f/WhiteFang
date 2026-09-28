/* WhiteFang: reads a .wf source file, runs it through the full
 * lex -> parse -> compile -> execute pipeline in one process (no
 * intermediate file format -- see docs/VM.md sec 7, "Deferred: on-disk
 * bytecode file format"). Exits with the program's own `start` return
 * value. */

#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "compiler.h"
#include "parser.h"
#include "vm.h"

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
        fprintf(stderr, "Usage: whitefang <path.wf>\n");
        return 64;
    }

    char *source = read_file(argv[1]);

    Parser parser;
    parser_init(&parser, source);
    AstProgram *ast = parser_parse_program(&parser);

    Program *program = compile_program(ast);
    int exit_code = vm_run(program);

    free(source);
    return exit_code;
}
