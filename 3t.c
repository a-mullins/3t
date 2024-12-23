// NB: I have conciously chosen to do precious little error handling
// in this program. It is primarily educational. If you choose to use
// portions of this code, do so with that in mind.
#include <locale.h>
#include <math.h>    // for sinf(), cosf(), tanf(), M_PI
#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>  // for abs()
#include <unistd.h>  // for usleep()

#define LEN(XS) (sizeof (XS) / sizeof (XS[0]))

typedef struct vec3 {
    float x, y, z;
} vec3;

typedef struct tri {
    vec3 p[3];
} tri;

typedef struct mat4x4 {
    float m[4][4];
} mat4x4;

// All of these braces are to make gcc -Wmissing-braces happy.
static const tri unit_cube[] = {
    // SOUTH
    {.p = {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}}},
    {.p = {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}},
    // EAST
    {.p = {{1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}},
    {.p = {{1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f}}},
    // NORTH
    {.p = {{1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}}},
    {.p = {{1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}},
    // WEST
    {.p = {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}}},
    {.p = {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}}},
    // TOP
    {.p = {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}}},
    {.p = {{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}}},
    // BOTTOM
    {.p = {{1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}}},
    {.p = {{1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}},
};

void
startup()
{
    // per the advice of `man ncurses`
    setlocale(LC_ALL, "");

    // init ncuruses
    initscr();

    // ncurses options
    cbreak();
    noecho();
    curs_set(0);
    scrollok(stdscr, FALSE);
    keypad(stdscr, TRUE);
}

// adapted from:
//   https://github.com/OneLoneCoder/Javidx9/tree/master/ConsoleGameEngine
// TODO figure out how this works or write own,
//      refactor args to be 0 indexed.
void
draw_line(int x1, int y1, int x2, int y2, char c)
{
    int x, y, dx, dy, dx1, dy1, px, py, xe, ye, i;

    dx = x2 - x1; dy = y2 - y1;
    dx1 = abs(dx); dy1 = abs(dy);
    px = 2 * dy1 - dx1;    py = 2 * dx1 - dy1;

    if (dy1 <= dx1) {
        if (dx >= 0) {
            x = x1; y = y1; xe = x2;
        }
        else {
            x = x2; y = y2; xe = x1;
        }

        mvaddch(y, x, c);
        //Draw(x, y, c, col);

        for (i = 0; x<xe; i++) {
            x = x + 1;
            if (px<0) {
                px = px + 2 * dy1;
            }
            else {
                if ((dx<0 && dy<0) || (dx>0 && dy>0)) y = y + 1; else y = y - 1;
                px = px + 2 * (dy1 - dx1);
            }
            mvaddch(y, x, c);
            // Draw(x, y, c, col);
        }
    }
    else {
        if (dy >= 0) {
            x = x1; y = y1; ye = y2;
        }
        else {
            x = x2; y = y2; ye = y1;
        }

        mvaddch(y, x, c);
        // Draw(x, y, c, col);

        for (i = 0; y<ye; i++) {
            y = y + 1;
            if (py <= 0) {
                py = py + 2 * dx1;
            }
            else {
                if ((dx<0 && dy<0) || (dx>0 && dy>0)) x = x + 1; else x = x - 1;
                py = py + 2 * (dx1 - dy1);
            }
            mvaddch(y, x, c);
            // Draw(x, y, c, col);
        }
    }
}

void
draw_tri(const tri *t, char c)
{
    draw_line(t->p[0].x, t->p[0].y, t->p[1].x, t->p[1].y, c);
    draw_line(t->p[1].x, t->p[1].y, t->p[2].x, t->p[2].y, c);
    draw_line(t->p[2].x, t->p[2].y, t->p[0].x, t->p[0].y, c);
}

// adapted from:
//   https://github.com/OneLoneCoder/Javidx9/tree/master/ConsoleGameEngine
void
mul_mat_vec(const mat4x4 *m, const vec3 *i, vec3 *o) {
    float w;
    o->x = i->x * m->m[0][0] + i->y * m->m[1][0] + i->z * m->m[2][0] + m->m[3][0];
    o->y = i->x * m->m[0][1] + i->y * m->m[1][1] + i->z * m->m[2][1] + m->m[3][1];
    o->z = i->x * m->m[0][2] + i->y * m->m[1][2] + i->z * m->m[2][2] + m->m[3][2];
       w = i->x * m->m[0][3] + i->y * m->m[1][3] + i->z * m->m[2][3] + m->m[3][3];

    if (w != 0.0f) {
	o->x /= w;
        o->y /= w;
        o->z /= w;
    }
}

int
main()
{
    startup();

    // PERSPECTIVE TRANSFORM
    float near = 0.1f;
    float far = 1000.0f;
    float fov = 90.0f;

    mat4x4 rot_z = {0};
    mat4x4 rot_x = {0};

    int y_max, x_max;
    getmaxyx(stdscr, y_max, x_max);
    char spinner[] = "__--==^^^^==--__";
    unsigned long long frame_cnt = 0;
    //float target_fps = 15;

    while( 1 ) {
        getmaxyx(stdscr, y_max, x_max);
        move(0, 0);
        erase();

        // Transform needs to be recalculated in case the window size changes.
        // The 2x coeff to the aspect ratio is to corret for the fact that
        // characters are not square.
        float aspect = 2 * ((float)y_max / (float)x_max);
        float fov_rad = 1 / tanf(fov * 0.5f / 180.0f * (float)M_PI);
        mat4x4 mat_proj = { .m = {
            {aspect * fov_rad, 0.0f,    0.0f,                       0.0f},
            {0.0f,             fov_rad, 0.0f,                       0.0f},
            {0.0f,             0.0f,    far / (far - near),         1.0f},
            {0.0f,             0.0f,    (-far*near) / (far - near), 0.0f}
        }};

        float theta = (float)frame_cnt / 15.0 / (0.5*M_PI);

        rot_z.m[0][0] = cosf(theta);
        rot_z.m[0][1] = sinf(theta);
        rot_z.m[1][0] = -sinf(theta);
        rot_z.m[1][1] = cosf(theta);
        rot_z.m[2][2] = 1;
        rot_z.m[3][3] = 1;

        rot_x.m[0][0] = 1;
        rot_x.m[1][1] = cosf(theta * 0.5f);
        rot_x.m[1][2] = sinf(theta * 0.5f);
        rot_x.m[2][1] = -sinf(theta * 0.5f);
        rot_x.m[2][2] = cosf(theta * 0.5f);
        rot_x.m[3][3] = 1;

        // Draw unit cube.
        for(size_t i = 0; i < LEN(unit_cube); i++) {
            // we must use seperate vars for each input and output,
            // because mul_mat_vec assumes the input vector doesn't
            // change.
            tri t, projected, translated, rotated_z, rotated_zx;
            t = unit_cube[i];

            mul_mat_vec(&rot_z, &t.p[0], &rotated_z.p[0]);
            mul_mat_vec(&rot_z, &t.p[1], &rotated_z.p[1]);
            mul_mat_vec(&rot_z, &t.p[2], &rotated_z.p[2]);

            mul_mat_vec(&rot_x, &rotated_z.p[0], &rotated_zx.p[0]);
            mul_mat_vec(&rot_x, &rotated_z.p[1], &rotated_zx.p[1]);
            mul_mat_vec(&rot_x, &rotated_z.p[2], &rotated_zx.p[2]);

            //translated = unit_cube[i];
            translated = rotated_zx;
            translated.p[0].z += 2.0f;
            translated.p[1].z += 2.0f;
            translated.p[2].z += 2.0f;

            // Apply perspective transform to each point.
            mul_mat_vec(&mat_proj, &translated.p[0], &projected.p[0]);
            mul_mat_vec(&mat_proj, &translated.p[1], &projected.p[1]);
            mul_mat_vec(&mat_proj, &translated.p[2], &projected.p[2]);

            // Each point has a range of -1 to +1, so it must be
            // scaled into screen space.
            projected.p[0].x += 1.0f; projected.p[0].y += 1.0f;
            projected.p[1].x += 1.0f; projected.p[1].y += 1.0f;
            projected.p[2].x += 1.0f; projected.p[2].y += 1.0f;

            projected.p[0].x *= 0.5f * x_max; projected.p[0].y *= 0.5f * y_max;
            projected.p[1].x *= 0.5f * x_max; projected.p[1].y *= 0.5f * y_max;
            projected.p[2].x *= 0.5f * x_max; projected.p[2].y *= 0.5f * y_max;

            draw_tri(&projected, 'O');
        }

        mvprintw(2, 8, "Hello, world! %c", spinner[frame_cnt % (sizeof (spinner) - 1)]);
        mvprintw(4, 9, "win size: %d col, %d row", x_max, y_max);
        mvprintw(5, 8, "frame_cnt: %lld", frame_cnt);
        mvprintw(6, 12, "theta: %04.2f * pi", (float)theta / M_PI);

        frame_cnt++;
        refresh();
        usleep(66700);
    }

    // CLEANUP
    endwin();
    return 0;
}
