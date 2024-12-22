#opt = -std=gnu17 -Wall -Wextra -Werror
opt = -std=gnu17 -g

all: 3t

3t: 3t.c
	gcc $(opt) -o 3t 3t.c

clean:
	-rm -f 3t
