/* Manual verification tool for M1: reads a .wf file and prints every
 * token the lexer produces, one per line. Not a golden test (those
 * start at M3 once the full pipeline exists) — this is a debugging
 * aid for eyeballing lexer output during development. */

#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"

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
        fprintf(stderr, "Usage: lexdump <path.wf>\n");
        return 64;
    }

    char *source = read_file(argv[1]);

    Lexer lexer;
    lexer_init(&lexer, source);

    int had_error = 0;
    for (;;) {
        Token token = lexer_next_token(&lexer);
        printf("%4d  %-12s '%.*s'\n", token.line, token_type_name(token.type),
               token.length, token.start);
        if (token.type == TOKEN_ERROR) had_error = 1;
        if (token.type == TOKEN_EOF) break;
    }

    free(source);
    return had_error ? 65 : 0;
}
