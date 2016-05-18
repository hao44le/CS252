#
# CS252 - Shell Project
#
#Use GNU compiler
cc = gcc -g
CC = g++ -g

LEX=lex
YACC=yacc

all: git-commit shell cat_grep grep_sort ctrl-c regular read-line-example

lex.yy.o: shell.l 
	$(LEX) shell.l
	$(CC) -c lex.yy.c

y.tab.o: shell.y
	$(YACC) -d shell.y
	$(CC) -c y.tab.c

command.o: command.cc
	$(CC) -c -g command.cc

shell: y.tab.o lex.yy.o command.o read-line.o tty-raw-mode.o
	$(CC) -o shell lex.yy.o y.tab.o command.o read-line.o tty-raw-mode.o -lfl

tty-raw-mode.o: tty-raw-mode.c
	gcc -c tty-raw-mode.c

read-line.o: read-line.c
	$(cc) -c read-line.c

read-line-example: read-line-example.c tty-raw-mode.o read-line.o
	gcc -o read-line-example read-line-example.c tty-raw-mode.o read-line.o

cat_grep: cat_grep.cc
	$(CC) -o cat_grep cat_grep.cc

grep_sort: grep_sort.cc
	$(CC) -o grep_sort grep_sort.cc	

ctrl-c: ctrl-c.cc
	$(CC) -o ctrl-c ctrl-c.cc

regular: regular.cc
	$(CC) -o regular regular.cc 

git-commit:
	git add *.cc *.l *.y Makefile >> .local.git.out || echo
	git commit -a -m "`cat test-shell/testall.out 2>/dev/null`" >> .local.git.out || echo

clean:
	rm -f lex.yy.c y.tab.c  y.tab.h shell ctrl-c regular cat_grep *.o

