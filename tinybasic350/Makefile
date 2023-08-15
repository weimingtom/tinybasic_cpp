CC := gcc
CPP := g++

all: tinybasic

tinybasic: tinybasic.cpp
	$(CPP) -g3 -O0 -o $@ $<

clean:
	rm -rf tinybasic *.exe

test1:
	#gdb --args ./tinybasic BAS-EX1.BAS
	./tinybasic BAS-EX1.BAS

test2:
	./tinybasic BAS-EX2.BAS

test3:
	./tinybasic BAS-EX3.BAS

test4:
	./tinybasic BAS-EX4.BAS

