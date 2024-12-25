#opt = -std=c11 -Wall -Wextra -Werror
#HEADERS = foo.h bar.h
LIBS = -lncurses -lm
# gcc disable usleep and nanosleep with c11 for some reason, even though
# they are in posix, so we use gnu11 instead.
CFLAGS = -std=gnu11 -Wall -Wextra -Wconversion -g

default: 3t

3t.o: 3t.c
	gcc $(CFLAGS) -c 3t.c -o 3t.o

3t: 3t.o
	gcc $(CFLAGS) $(LIBS) 3t.o -o 3t

clean:
	-rm -f 3t.o
	-rm -f 3t
