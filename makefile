LIBS = -lncurses -lm
CFLAGS = -std=gnu11 -Wall -Wextra -Wconversion -g

default: 3t

darray.o: darray.c
	gcc $(CFLAGS) -c darray.c -o darray.o

3t.o: 3t.c
	gcc $(CFLAGS) -c 3t.c -o 3t.o

3t: darray.o 3t.o
	gcc $(CFLAGS) $(LIBS) darray.o 3t.o -o 3t

clean:
	-rm -f 3t.o darray.o
	-rm -f 3t
