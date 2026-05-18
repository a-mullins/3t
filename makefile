CFLAGS = -std=gnu11 -Wall -Wextra -Wpedantic -Wconversion -g

# If ncurses has been installed with MacPorts, or with Homebrew on Intel
# hardware, uncomment the following lines:
#CFLAGS += -I/opt/local/include
#LDFLAGS = -L/opt/local/lib

# If ncurses has been installed with Homebrow on Apple silicon,
# uncomment the following lines:
# CFLAGS += -I/opt/homebrew/include
# LDFLAGS = -L/opt/homebrew/lib

default: 3t

3t: LDFLAGS = -lncursesw -lm
3t: darray.c

clean:
	-rm -f 3t.o darray.o 3t
