/* Manual verification tool for M3's compiler: reads a .wf file,
 * runs it through the full lex -> parse -> compile pipeline, and
 * disassembles the resulting bytecode. Lets the compiler be checked
 * against docs/VM.md's worked trace before the VM exists to actually
 * run anything. */

#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "compiler.h"
#include "disassemble.h"
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
        fprintf(stderr, "Usage: bcdump <path.wf>\n");
        return 64;
    }

    char *source = read_file(argv[1]);

    Parser parser;
    parser_init(&parser, source);
    AstProgram *ast = parser_parse_program(&parser);

    /* compile_program exits(65) itself on the first semantic error --
     * see docs/DECISIONS.md ("stop on first error"). */
    Program *program = compile_program(ast);
    disassemble_program(program);

    free(source);
    return 0;
}
