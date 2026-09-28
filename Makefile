CC       = cc
CFLAGS   = -std=c11 -Wall -Wextra -Werror -g -Isrc/lexer -Isrc/ast -Isrc/parser -Isrc/bytecode -Isrc/vm
BUILDDIR = build

LEXER_SRCS     = src/lexer/lexer.c
AST_SRCS       = src/ast/ast.c
PARSER_SRCS    = src/parser/parser.c
BYTECODE_SRCS  = src/bytecode/chunk.c src/bytecode/compiler.c src/bytecode/disassemble.c
VM_SRCS        = src/vm/vm.c

LEXER_OBJS     = $(LEXER_SRCS:%.c=$(BUILDDIR)/%.o)
AST_OBJS       = $(AST_SRCS:%.c=$(BUILDDIR)/%.o)
PARSER_OBJS    = $(PARSER_SRCS:%.c=$(BUILDDIR)/%.o)
BYTECODE_OBJS  = $(BYTECODE_SRCS:%.c=$(BUILDDIR)/%.o)
VM_OBJS        = $(VM_SRCS:%.c=$(BUILDDIR)/%.o)

FRONTEND_OBJS  = $(LEXER_OBJS) $(AST_OBJS) $(PARSER_OBJS) $(BYTECODE_OBJS)

.PHONY: all clean lexdump parsedump bcdump whitefang test

all: lexdump parsedump bcdump whitefang

lexdump: $(BUILDDIR)/lexdump
parsedump: $(BUILDDIR)/parsedump
bcdump: $(BUILDDIR)/bcdump
whitefang: $(BUILDDIR)/whitefang

$(BUILDDIR)/lexdump: $(LEXER_OBJS) $(BUILDDIR)/src/lexer/lex_dump.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/parsedump: $(LEXER_OBJS) $(AST_OBJS) $(PARSER_OBJS) $(BUILDDIR)/src/parser/parse_dump.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/bcdump: $(FRONTEND_OBJS) $(BUILDDIR)/src/bytecode/bc_dump.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/whitefang: $(FRONTEND_OBJS) $(VM_OBJS) $(BUILDDIR)/src/main.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

test: whitefang
	./tests/run_golden.sh

clean:
	rm -rf $(BUILDDIR)
