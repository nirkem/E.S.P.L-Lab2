CC = gcc
CFLAGS = -g -Wall

all: myshell looper mypipe

myshell: myshell.o LineParser.o
	$(CC) $(CFLAGS) -o $@ myshell.o LineParser.o

looper: Looper.o
	$(CC) $(CFLAGS) -o $@ Looper.o

mypipe: mypipe.o
	$(CC) $(CFLAGS) -o $@ mypipe.o

myshell.o: myshell.c LineParser.h
LineParser.o: LineParser.c LineParser.h

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: all clean
clean:
	rm -f *.o myshell looper mypipe
