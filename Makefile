TMPFILES = lex.c parse.c
MODULES = main parse output
OBJECTS = $(MODULES:=.o)
CC = gcc -g -Wall
LINK = $(CC)
LIBS = -lreadline

shell: $(OBJECTS)
	$(LINK) $(OBJECTS) -o $@ $(LIBS)

# Compiling:
%.o: %.c
	$(CC) -c $<

# parsers

parse.o: parse.c lex.c
parse.c: parse.y global.h
	bison parse.y -o $@
lex.c: lex.l
	flex -o$@ lex.l

# clean

clean:
	rm -f shell $(OBJECTS) $(TMPFILES) core* *.o
	rm -rf NVILLANI-Shell

final:
	make clean
	mkdir NVILLANI-Shell
	cp global.h lex.l main.c Makefile output.c parse.y NVILLANI-Shell/
	tar czf NVILLANI-Shell.tar.gz NVILLANI-Shell
