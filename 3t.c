#include <locale.h>
#include <ncurses.h>
#include <unistd.h>  // for usleep()
#include <stdlib.h>  // for rand()
#include <time.h>    // for time()

#define FLAKES 256

int
main()
{
    // seed random
    srand(time(NULL));
    // set locale, per the advice of `man ncurses`. by passing "" as
    // second arg, we follow whatever is set by the env.
    setlocale(LC_ALL, "");

    // INITIALIZE NCURSES
    initscr();
    start_color();

    // cbreak disables line-buffering, input is available immediately.
    cbreak();
    noecho();
    // hide cursor
    curs_set(0);
    // do not scroll if the cursor falls off the last line.
    scrollok(stdscr, FALSE);
    // if TRUE, flush on interrupt. Feels faster, but confuses
    // ncurses, according to man cur_inopts.
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    // nodelay makes getch() nonblocking, which will want later.
    // nodelay(stdscr, TRUE);

    int y_max, x_max, y_max_old, x_max_old;
    getmaxyx(stdscr, y_max, x_max);
    y_max_old = 0;
    x_max_old = 0;

    // snowflakes
    struct snowflake {
        int x;
        int y;
        int last_moved;
        int stuck;
    };
    struct snowflake flakes[FLAKES] = {0};
    for(int i = 0; i<FLAKES; i++) {
        flakes[i].y = 0;
        //flakes[i].y = (int)((float)4 * ((float)rand() / (float)RAND_MAX));
        flakes[i].x = (int)((float)x_max * ((float)rand() / (float)RAND_MAX));
        flakes[i].last_moved = (int)((float)900 * ((float)rand() / (float)RAND_MAX));
        flakes[i].stuck = 0;
    }

    // snowman
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    attron(COLOR_PAIR(2));
    mvprintw(y_max-6, 12,"_\n");
    mvprintw(y_max-5, 10,"_[_]_\n");
    attroff(COLOR_PAIR(2));
    mvprintw(y_max-4, 11,     "(\")\n");
    mvprintw(y_max-3,  7, "`--( : )--'\n");
    mvprintw(y_max-2,  9,   "(  :  )\n");


    char spinner[] = "__--==^^^^==--__";
    int frame_cnt = 0;
    //float target_fps = 15;
    while( 1 ) {
        move(0, 0);
        // no need for &row, &col, this is a macro.
        getmaxyx(stdscr, y_max, x_max);

        for(int i = 0; i<FLAKES; i++) {
//            if(!flakes[i].stuck && 12 < frame_cnt - flakes[i].last_moved) {
            if(18 < (frame_cnt - flakes[i].last_moved)) {
                flakes[i].last_moved = frame_cnt;
                mvaddch(flakes[i].y, flakes[i].x, ' ');

                // move flake
                float r = (float)rand() / (float)RAND_MAX;
                if(flakes[i].y+1 < y_max && mvinch(flakes[i].y+1, flakes[i].x) == ' ') {
                    flakes[i].y++;
                    if     (r < 0.25) flakes[i].x = (flakes[i].x - 1) % (x_max-1);
                    else if(r > 0.75) flakes[i].x = (flakes[i].x + 1) % (x_max-1);
                }
                if(mvinch(flakes[i].y+2, flakes[i].x) != '.') {
                    mvaddch(flakes[i].y, flakes[i].x, '.');
                }
            }
        }

        // snowflakes
        // if(12 < frame_cnt - last_moved) {
        //     last_moved = frame_cnt;
        //     // erase old snowflake position
        //     mvaddch(fl_y, fl_x, ' ');

            // // move flake
            // float r = (float)rand() / (float)RAND_MAX;
            // if(fl_y+1 < y_max && mvinch(fl_y+1, fl_x) == ' ') {
            //     fl_y++;
            //     if     (r < 0.25) fl_x = (fl_x - 1) % (x_max-1);
            //     else if(r > 0.75) fl_x = (fl_x + 1) % (x_max-1);
            // }

        //     // draw snowflake.
        //     mvaddch(fl_y, fl_x, '.');
        // }

        // mvprintw(3, 8, "fl_y: %d %d %d",
        //          fl_y,
        //          fl_y < y_max,
        //          mvinch(fl_y+1, fl_x) != ' ');

        if(y_max != y_max_old || x_max != x_max_old) {
            // for(int y = 0; y < y_max; y++) {
            //     //mvaddch(y, 0, '0');
            //     //mvaddch(y, x_max-1, 'E');
            //     //     for(int x = 0; x < x_max; x++) {
            //     //         if(x == 0)             mvaddch(y, x, '0');
            //     //         else if (x+1 == x_max) mvaddch(y, x, 'E');
            //     //         else                   addch('.');
            //     //     }
            // };
            init_pair(1, COLOR_GREEN, COLOR_BLACK);
            attron(COLOR_PAIR(1));
            for(int x = 0; x < x_max; mvaddch(y_max-1, x++, '"'));
            attroff(COLOR_PAIR(1));
            y_max_old = y_max;
            x_max_old = x_max;
            refresh();
        }

        mvprintw(2, 8, "Hello, world! %c",
                 spinner[frame_cnt % (sizeof (spinner) - 1)]);
        mvprintw(3, 8, "Merry Christmas!");
        //mvprintw(3, 8, "rows: %3d, cols: %3d", y_max, x_max);
        frame_cnt++;
        refresh();
        usleep(66700);
    }

    // CLEANUP
    endwin();
    return 0;
}
