CC = gcc
CFLAGS = -g -Wall -Wno-unused-function -Wno-unused-variable

TARGET = minicompiler

OBJS = lex.yy.o parser.tab.o loop_preprocessor.o \
       main.o ast.o symtab.o semantic.o tac.o codegen.o

all: $(TARGET)

$(TARGET): parser.tab.h $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) -lm

# parser.tab.h is our hand-patched version with FOR/DO tokens — never overwrite it
parser.tab.h:
	@if [ ! -f parser.tab.h ]; then cp parser_tab.h parser.tab.h; fi
	@echo "parser.tab.h ready"

lex.yy.o: lex_yy.c parser.tab.h ast.h
	$(CC) $(CFLAGS) -c lex_yy.c -o lex.yy.o

parser.tab.o: parser_tab.c parser_tab.h ast.h
	$(CC) $(CFLAGS) -c parser_tab.c -o parser.tab.o

loop_preprocessor.o: loop_preprocessor.c parser.tab.h
	$(CC) $(CFLAGS) -c loop_preprocessor.c -o loop_preprocessor.o

main.o: main.c ast.h symtab.h semantic.h tac.h codegen.h
	$(CC) $(CFLAGS) -c main.c

ast.o: ast.c ast.h
	$(CC) $(CFLAGS) -c ast.c

symtab.o: symtab.c symtab.h
	$(CC) $(CFLAGS) -c symtab.c

semantic.o: semantic.c semantic.h ast.h symtab.h
	$(CC) $(CFLAGS) -c semantic.c

tac.o: tac.c tac.h ast.h
	$(CC) $(CFLAGS) -c tac.c

codegen.o: codegen.c codegen.h tac.h symtab.h
	$(CC) $(CFLAGS) -c codegen.c

clean:
	rm -f $(TARGET) $(OBJS) *.s *.tac
	@echo "Cleaned (parser.tab.h preserved)"

test: $(TARGET)
	./$(TARGET) test_loops.c test_loops.s

test_errors: $(TARGET)
	./$(TARGET) test_semantic_errors.c errors.s || true

.PHONY: all clean test test_errors
