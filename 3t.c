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



#define LF                    "\012"
#define CR                    "\015"
#define RESET_TERM            "\033c"
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


// struct screen {
//     int rows;
//     int cols;
//     size_t buf_size;
//     char *buf;
// };

//static int tty_fd = 0;
// static struct termios old_term = {0};
// static struct screen screen = {0};


// void on_win_change();
// void restore_term();
// void draw(unsigned int x, unsigned int y, char c);
// void draw_str(unsigned int x, unsigned int y, const char *s);


// void write_out() {
//     printf("%s", CLEAR_SCREEN);
//     printf("%s", CSI "H");
//     for(unsigned int row = 0; row < screen.rows; row++) {
//         write(1, screen.buf + row*screen.cols, screen.cols);
//         //memcpy(line_buf, screen.buf + row*screen.cols, screen.cols);
//         //snprintf(line_buf, size, "%s", screen.buf + row*screen.cols);
//         // for(unsigned int col = 0; col < screen.cols; col++) {
//         //     putchar(*(screen.buf + row * screen.cols + col));
//         // }
//         //printf("%s", line_buf);
//         if(row == screen.rows) {
//             printf("\r");
//         } else {
//             printf("\n");
//         }
//     }
//     //printf("screen rows: %d", screen.rows);
//     // for(unsigned short i = 0; i < screen.cols; i++) {
//     //     putchar('.');
//     // }
//     printf("%s", CSI "H");
//     fflush(stdout);
// }


// void draw(unsigned int x, unsigned int y, char c) {
//     if( x < screen.cols && y < screen.cols ) {
//         *(screen.buf + y * screen.cols + x) = c;
//     }
// }


// void draw_str(unsigned int x, unsigned int y, const char *s) {
//     char *target = screen.buf + y*screen.cols + x;
//     // could use strcpy, but we don't want the null.
//     memcpy(target, s, strlen(s));
// }


// void on_win_change() {
//     // TODO get screen pixels for more accurate aspect ratio
//     struct winsize ws = {0};

//     if(ioctl(tty_fd, TIOCGWINSZ, &ws) == -1) {
//         FEPRINTF("ioctl to get win size failed");
//     }

//     // size_t old_size = screen.buf_size;
//     // size_t new_size = sizeof (char) * (size_t)ws.ws_row * (size_t)ws.ws_col;
//     // if(old_size < new_size) {
//     //     // this next call will overwrite screen.buf with NULL if the alloc fails,
//     //     // so in theory it would be nice to keep a copy of the old pointer and
//     //     // restore it in that case.
//     //     // but since we can't continue without a screen buffer, we will just die.
//     //     screen.buf = realloc(screen.buf, new_size);
//     //     if(screen.buf == NULL) {
//     //         FEPRINTF("could not realloc memory for screen buffer");
//     //     }
//     //     screen.buf_size = new_size;

//     //     // fill the newly allocated memory with safe characters.
//     //     // we wouldn't want accidentally print 2000 BEL. :-|
//     //     memset(screen.buf + old_size, ' ', new_size - old_size);
//     // }

//     screen.rows = ws.ws_row;
//     screen.cols = ws.ws_col;
// }


int main(int argc, char **argv) {
    // screen.buf_size = (size_t)1920;
    // screen.buf = malloc(1920 * sizeof (char));
    // memset(screen.buf, ' ', 1920);;

    // get fd for term we are connected to
    int tty_fd = open("/dev/tty", O_NONBLOCK);
    if(tty_fd == -1) {
        FEPRINTF("open(/dev/tty, ...)");
    }

    // // hook window resizes
    // if(signal(SIGWINCH, on_win_change) == SIG_ERR) {
    //     FEPRINTF("signal(SIGWINCH, ...)");
    // }

    static struct termios old_term;
    static struct termios new_term;
    // save current terminal configuration, so that it can be restored on exit
    if(tcgetattr(0, &old_term)) {
        FEPRINTF("tcgetattr(0, ...)");
    }
    // set up new terminal configuration
    new_term = old_term;
    // disable canonical mode and local echo
    new_term.c_lflag &= (~ICANON & ~ECHO);
    if(tcsetattr(0, TCSANOW, &new_term)) {
        FEPRINTF("tcsetattr(...)");
    }
    getchar(); // pause for attachment by gdb. :-)
    //puts(HIDE_CURSOR);

    struct winsize ws = {0};
    if(ioctl(tty_fd, TIOCGWINSZ, &ws) == -1) {
        FEPRINTF("ioctl to get win size failed");
    }

    // size_t buf_size = (ws.ws_row+1)*(ws.ws_col+1);
    // char *buf = calloc(buf_size, sizeof (char));
    // memset(buf, ' ', buf_size);
    char buf[ws.ws_row][ws.ws_col];
    memset(buf, ' ', ws.ws_row * ws.ws_col);
    for(unsigned short row = 0; row < ws.ws_row; row++) {
        for(unsigned short col = 0; col < ws.ws_col; col++) {
            if      (col   == 0)         buf[row][col] = (char)row%10+0x30;
            else if (col+1 == ws.ws_col) buf[row][col] = 'E';
            //else                         buf[row][col] = '.';
        }
    }

    fputs(RESET_TERM, stdout);

    srand(time(NULL));

    // GAME LOOP
    float target_fps = 48.0;
    struct timespec tm_asked = { .tv_sec = 0, .tv_nsec = (long)(1e9 * 1.0/target_fps)};
    struct timespec tm_left  = { 0 };
    struct timespec tm_stamp_start = { 0 };
    struct timespec tm_stamp_now = { 0 };
    clock_gettime(CLOCK_REALTIME, &tm_stamp_start);
    unsigned long tm_diff_sec = 0;
    unsigned int frame_cnt = 0;
    char cnt_buf[128] = {0};
    char hi_spinner[] = "               __--==^^==--__               ";
    char md_spinner[] = "        __--==^              ^==--__        ";
    char lo_spinner[] = "__--==^^                            ^^==--__";
    while(1) {
        fputs(GO_HOME, stdout); fflush(stdout);
        fputs(CSI "J", stdout); fflush(stdout);

        clock_gettime(CLOCK_REALTIME, &tm_stamp_now);
        tm_diff_sec = (tm_stamp_now.tv_sec - tm_stamp_start.tv_sec);

        // flowers
        unsigned int fl_row = (rand() % ws.ws_row);
        unsigned int fl_col = 1 + (rand() % (ws.ws_col - 2));
        char fl_c;
        switch(buf[fl_row][fl_col]) {
        case ' ':
            fl_c = '.';
            break;
        case '.':
            fl_c = '_';
            break;
        case '_':
            fl_c = '|';
            break;
        case '|':
            fl_c = ' ';
            break;
        }
        buf[fl_row][fl_col] = fl_c;

        snprintf(cnt_buf, 40, "%c target fps: %2.1f",
                 hi_spinner[frame_cnt % (sizeof (hi_spinner) - 1)],
                 target_fps);
        memcpy(&buf[1][8], cnt_buf, strlen(cnt_buf));

        snprintf(cnt_buf, 40, "%c     frames: %4d",
                 md_spinner[frame_cnt % (sizeof (hi_spinner) - 1)],
                 frame_cnt++);
        memcpy(&buf[2][8], cnt_buf, strlen(cnt_buf));

        snprintf(cnt_buf, 40, "%c    avg fps: %2.1f",
                 lo_spinner[frame_cnt % (sizeof (lo_spinner) - 1)],
                 (float)frame_cnt / (float)tm_diff_sec);
//                 (double)frame_cnt / (double)tm_diff_nsec);
//                 1e9f / (float)tm_diff_nsec);
        memcpy(&buf[3][8], cnt_buf, strlen(cnt_buf));

        for(unsigned short row = 0; row < ws.ws_row; row++) {
            write(1, buf[row], ws.ws_col);
            if(row+1 < ws.ws_row) {
                fputs(LF CR "\0", stdout);
            }
            fflush(stdout);
        }
        //fflush(stdout);
        //fputs(GO_HOME, stdout);
        nanosleep(&tm_asked, &tm_left);
    }

    // CLEANUP
    if(tcsetattr(0, TCSANOW, &old_term)) { FEPRINTF("restore_term failed"); }
    fputs(SHOW_CURSOR, stdout);
    fputs(RESET_TERM, stdout);
    fflush(stdout);
    return 0;
}
