#include <poll.h>
#include <stdio.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
    struct pollfd pfd;
    pfd.fd = 0;
    pfd.events = POLLIN;

    char buf[80] = {0};

    struct termios old_term;
    struct termios new_term;
    tcgetattr(0, &old_term);
    new_term = old_term;
    new_term.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(0, TCSANOW, &new_term);

    struct timespec tm_asked = { .tv_sec = 0, .tv_nsec = (long)(1e9 * 1.0/15.0)};
    struct timespec tm_left  = { 0 };
    while(1) {
        while(poll(&pfd, 1, 0)) {
            ssize_t bytes_read = read(0, buf, 80-1);
            if(bytes_read > 0) { buf[bytes_read] = '\0'; }
            printf("\n%s", buf);
        }

        putchar('.');

        buf[0] = '\0';
        fflush(stdout);
        //sleep(4);
        nanosleep(&tm_asked, &tm_left);
    }

    tcsetattr(0, TCSANOW, &old_term);
    puts("");
    return 0;
}
