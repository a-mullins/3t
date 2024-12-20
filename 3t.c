#include <errno.h>     // errno
#include <fcntl.h>     // open()
#include <signal.h>    // kill, signal()
#include <stdio.h>     // fprintf(), getchar(), stdin, out, err
#include <stdlib.h>    // atexit(), exit()
#include <string.h>    // strerror()
#include <sys/ioctl.h> // ioctl()
#include <termios.h>   // tcgetattr()
#include <unistd.h>    // tcgetattr(), getpid()


void print_size();
void restore_term();


static struct termios old_term;
static struct termios new_term;


int main(int argc, char **argv) {
    if(tcgetattr(0, &old_term)) {
        fprintf(stderr, "ERR: tcgetattr()\n");
        exit(1);
    }
    atexit(restore_term);

    // Disable canonical mode and local echo on term.
    new_term = old_term;
    new_term.c_lflag &= (~ICANON & ~ECHO);
    if(tcsetattr(0, TCSANOW, &new_term)) {
        fprintf(stderr, "ERR: tcsetattr()\n");
        exit(1);
    }

    // Set up signal handler so that window size is printed whenever it
    // changes.
    if(signal(SIGWINCH, print_size) == SIG_ERR) {
        fprintf(stderr, "ERR: signal()\n");
        exit(1);
    }

    // We could call print_size once directly here, but...
    kill(getpid(), SIGWINCH);
    while(getchar() != 'q') { // blocking, so wait for input or SIGWINCH
        /* nop */
    }

    return 0;
}


void print_size() {
    // FIXME ioctl is discouraged, interface in man 3 termios is preferred
    //       is it possible to use that instead?
    int tty_fd = open("/dev/tty", O_NONBLOCK);
    if(tty_fd == -1) {
        fprintf(stderr,
                "ERR: open(), could not open /dev/tty. %d: %s",
                errno,
                strerror(errno));
        exit(1);
    }

    struct winsize ws;
    int result = ioctl(tty_fd, TIOCGWINSZ, &ws);
    close(tty_fd);
    if(result == -1) {
        fprintf(stderr,
                "ERR: ioctl(), could not get terminal size. %d: %s",
                errno,
                strerror(errno));
    }

    printf("\033c");
    for(int row = 1; row < ws.ws_row >> 1;        printf("\n"), row++);
    for(int col = 1; col < (ws.ws_col >> 1) - 12; printf(" "), col++);
    printf("Type 'q' or Ctrl-C to quit.\n");
    for(int col = 1; col < (ws.ws_col >> 1) - 12; printf(" "), col++);
    printf("%d x %d", ws.ws_col, ws.ws_row);
    fflush(stdout);

    return;
}


void restore_term() {
    printf("\033c");
    // don't both error checking, this is called atexit() and there's nothing
    // we can do if it fails anyway.
    tcsetattr(0, TCSANOW, &old_term);
    return;
}
