#include <errno.h>     // errno
#include <fcntl.h>     // open()
#include <signal.h>    // kill, signal()
#include <stdio.h>     // fprintf(), getchar(), stdin, out, err
#include <stdlib.h>    // atexit(), exit()
#include <string.h>    // strerror()
#include <sys/ioctl.h> // ioctl()
#include <termios.h>   // tcgetattr()
#include <time.h>      //
#include <unistd.h>    // tcgetattr(), getpid()


#define RESET_SCREEN          "\033c"
// CSI = Control Sequence Introducer
#define CSI                   "\033["
#define CLEAR_SCREEN      CSI "2J"
#define CLEAR_ALL         CSI "3J"
#define HIDE_CURSOR       CSI "?25l"
#define SHOW_CURSOR       CSI "?25h"
#define GET_SCREEN_PIXELS CSI "14t"
#define GO_HOME           CSI "H"

// fatal error printf
#define FEPRINTF(msg)                           \
    fprintf(stderr,                             \
            "ERR: %s:%d %s\n",                  \
            __FILE__, __LINE__, msg);           \
    exit(1);                                    \


struct screen {
    int rows;
    int cols;
    size_t buf_size;
    char *buf;
};

static int tty_fd = 0;
static struct termios old_term = {0};
static struct screen screen = {0};


void on_win_change();
void restore_term();
void write_out();
void draw(unsigned int x, unsigned int y, char c);
void draw_str(unsigned int x, unsigned int y, const char *s);


void write_out() {
    for(unsigned int row = 0; row <= screen.rows; row++) {
        write(1, screen.buf + row*screen.cols, screen.cols);
        //memcpy(line_buf, screen.buf + row*screen.cols, screen.cols);
        //snprintf(line_buf, size, "%s", screen.buf + row*screen.cols);
        // for(unsigned int col = 0; col < screen.cols; col++) {
        //     putchar(*(screen.buf + row * screen.cols + col));
        // }
        //printf("%s", line_buf);

        if(row == screen.rows) {
            printf("\r");
        } else {
            printf("\n");
        }
    }
    //printf("screen rows: %d", screen.rows);
    // for(unsigned short i = 0; i < screen.cols; i++) {
    //     putchar('.');
    // }
    printf("%s", CSI "H");
    fflush(stdout);
}


void draw(unsigned int x, unsigned int y, char c) {
    if( x < screen.cols && y < screen.cols ) {
        *(screen.buf + y * screen.cols + x) = c;
    }
}


void draw_str(unsigned int x, unsigned int y, const char *s) {
    char *target = screen.buf + y*screen.cols + x;
    // could use strcpy, but we don't want the null.
    memcpy(target, s, strlen(s));
}


void on_win_change() {
    // TODO get screen pixels for more accurate aspect ratio
    struct winsize ws = {0};

    if(ioctl(tty_fd, TIOCGWINSZ, &ws) == -1) {
        FEPRINTF("ioctl to get win size failed");
    }

    size_t old_size = screen.buf_size;
    size_t new_size = sizeof (char) * (size_t)ws.ws_row * (size_t)ws.ws_col;
    if(old_size < new_size) {
        // this next call will overwrite screen.buf with NULL if the alloc fails,
        // so in theory it would be nice to keep a copy of the old pointer and
        // restore it in that case.
        // but since we can't continue without a screen buffer, we will just die.
        screen.buf = realloc(screen.buf, new_size);
        if(screen.buf == NULL) {
            FEPRINTF("could not realloc memory for screen buffer");
        }
        screen.buf_size = new_size;

        // fill the newly allocated memory with safe characters.
        // we wouldn't want accidentally print 2000 BEL. :-|
        memset(screen.buf + old_size, ' ', new_size - old_size);
    }

    screen.rows = ws.ws_row - 1;
    screen.cols = ws.ws_col - 1;
}


// FIXME not called it, say, we get SIGINT
void restore_term() {
    puts(SHOW_CURSOR);
    if(tcsetattr(0, TCSANOW, &old_term)) { FEPRINTF("restore_term failed"); }
    printf("%s", RESET_SCREEN);
}


int main(int argc, char **argv) {
    screen.buf_size = (size_t)1920;
    screen.buf = malloc(1920 * sizeof (char));
    memset(screen.buf, ' ', 1920);

    // get fd for term we are connected to
    tty_fd = open("/dev/tty", O_NONBLOCK);
    if(tty_fd == -1) {
        FEPRINTF("open(/dev/tty, ...)");
    }

    // hook window resizes
    if(signal(SIGWINCH, on_win_change) == SIG_ERR) {
        FEPRINTF("signal(SIGWINCH, ...)");
    }

    // save current terminal configuration, so that it can be restored on exit
    if(tcgetattr(0, &old_term)) {
        FEPRINTF("tcgetattr(0, ...)");
    }
    atexit(restore_term);
    // set up new terminal configuration
    static struct termios new_term;
    new_term = old_term;
    // disable canonical mode and local echo
    new_term.c_lflag &= (~ICANON & ~ECHO);
    if(tcsetattr(0, TCSANOW, &new_term)) {
        FEPRINTF("tcsetattr(...)");
    }
    puts(CLEAR_SCREEN);
    //puts(HIDE_CURSOR);
    // set up screen
    on_win_change();

    struct timespec tm_asked = { .tv_sec = 0, .tv_nsec = (long)(1e9 * 1.0/10.0)};
    struct timespec tm_left  = { 0 };
    long long frame_cnt = 0;
    char spinner[] = "||/--\\";
    short i = 0;
    char diag[80] = {0};
    char line[8] = {0};
    while( 1 ) {
        memset(screen.buf, ' ', screen.buf_size);

        for(unsigned int row = 0; row <= screen.rows; row++) {
            // if(row == 0 && row == screen.rows) {
            //     for(int i = 0; i < screen.cols; i++) {draw(i, row, '.');}
            // }
            snprintf(line, 8, "row%02d", row);
            draw_str(0, row, line);
            draw(screen.cols-1, row, '|');
        }

        if(i == sizeof (spinner)) {i = 0;}
        snprintf(diag, 80, "%c frames: %d", spinner[i++], frame_cnt);
        draw_str(16, 4, diag);
        write_out();
        frame_cnt++;
        nanosleep(&tm_asked, &tm_left);
    }

    return 0;
}
