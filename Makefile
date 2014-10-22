mem: mem.c mem.h
	gcc -c -fpic mem.c -Wall -Werror
	gcc -shared -o libmem1.so mem.o
	gcc -shared -o libmem2.so mem.o
	gcc -shared -o libmem3.so mem.o



clean:
	rm -rf mem.o libmem1.so libmem2.so libmem3.so
