CFLAGS  = -std=gnu11 -Wall -Wextra -Wpedantic -Wconversion -g
CFLAGS += -I../include -fpic -shared

libalist.so: alist.c
	$(CC) $(CFLAGS) -o libalist.so alist.c

.PHONY: clean
clean:
	-rm -f libalist.so
