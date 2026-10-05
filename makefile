CC = gcc
CFLAGS = -g -Wall -Wextra -std=gnu11

PROGRAMS = myshell looper mypipe mypipeline

all: $(PROGRAMS)

myshell: myshell.o LineParser.o jobs.o history.o
	$(CC) $(CFLAGS) -o $@ $^

looper: Looper.o
	$(CC) $(CFLAGS) -o $@ $^

mypipe: mypipe.o
	$(CC) $(CFLAGS) -o $@ $^

mypipeline: mypipeline.o
	$(CC) $(CFLAGS) -o $@ $^

myshell.o: myshell.c LineParser.h jobs.h history.h
jobs.o: jobs.c jobs.h LineParser.h
history.o: history.c history.h
LineParser.o: LineParser.c LineParser.h

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: all
	./tests/run.sh

.PHONY: all clean test
clean:
	rm -f *.o $(PROGRAMS)
