LDFLAGS = -lncurses -lm
CFLAGS = -std=gnu11 -Wall -Wextra -Wconversion -g

default: 3t

3t: darray.o

clean:
	-rm -f 3t.o darray.o 3t
