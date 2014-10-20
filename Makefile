mem: mem.c mem.h
	gcc -c -fpic mem.c -Wall -Werror
	gcc -shared -o libmem1.so mem.o

clean:
	rm -rf mem.o libmem.so
