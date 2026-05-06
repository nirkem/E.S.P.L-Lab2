# format is target-name: target dependencies 
#       actions 

# All Targets 
all: exec looper

# Tool invocations 
# Executable "exec" depends on the files myshell.o, LineParser.o, and Looper.o. 
exec: myshell.o LineParser.o 
	gcc -m32 -g -Wall -o exec myshell.o LineParser.o 

# Depends on the source files 
myshell.o: myshell.c LineParser.h
	gcc -g -Wall -m32 -c -o myshell.o myshell.c

LineParser.o: LineParser.c LineParser.h
	gcc -g -Wall -m32 -c -o LineParser.o LineParser.c

looper: looper.o
	gcc -g -Wall -m32 -o looper looper.o

looper.o: Looper.c
	gcc -g -Wall -m32 -c -o looper.o Looper.c

# tell make that "clean" is not a file name! 
.PHONY: clean 

# Clean the build directory 
clean:
	rm -f *.o exec 
