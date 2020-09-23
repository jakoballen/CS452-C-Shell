main: myshell.o lex.yy.o Makefile
	gcc myshell.o lex.yy.o -lfl -o main

myshell.o: myshell.c
	gcc -c myshell.c -std=c99

lex.yy.o: lex.yy.c
	gcc -c lex.yy.c

lex.yy.c : lex.c
	flex lex.c

clean:
	rm -f *.o lex.yy.c main
