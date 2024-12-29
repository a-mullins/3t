// Nota bene: I have chosen to do little error handling in this
// program. It is primarily educational. If you choose to use portions
// of this code, do so with that in mind.

// This is necessary to enable ncurses wide character support,
// at least with how it is packaged on Arch.
#define _XOPEN_SOURCE_EXTENDED

#include <locale.h>
#include <math.h>    // for sinf(), cosf(), tanf(), M_PI
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>   // fopen, fprintf, etc
#include <stdlib.h>  // for abs()
#include <string.h>  // memcpy()
#include <unistd.h>  // for usleep()
#include <wchar.h>   // for wint_t, wchar_t, etc.
#include "darray.h"

#define LEN(X) sizeof (X) / sizeof (X[0]);
#define SWAP(M, N) { M ^= N; \
                     N ^= M; \
                     M ^= N; }


// -=[ STRUCTS / TYPES ]=------------------------------------------------------
typedef struct vec3 {
    float x, y, z;
} vec3;

typedef struct tri {
    vec3 p[3];
} tri;

typedef struct mesh {
    int len;
    tri *tris;
} mesh;

typedef struct mat4x4 {
    float m[4][4];
} mat4x4;


// -=[ VECTOR AND MATRIX OPERATIONS ]=-----------------------------------------
void
mul_mat_vec(const mat4x4 *m, const vec3 *i, vec3 *o)
{
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


// Multiply each vector of triangle `t` with matrix `m`.
// This is useful for applying a transform to a triangle.
void
mul_mat_tri(const mat4x4 *m, const tri *t, tri *to)
{
    for(short i = 0; i < 3; i++) {
        mul_mat_vec(m, &t->p[i], &to->p[i]);
    }
}


// using out param instead of returning to match mul_mat_vec, etc
void
add_vec(const vec3 *v1, const vec3 *v2, vec3 *vo)
{
    vo->x = v1->x + v2->x;
    vo->y = v1->y + v2->y;
    vo->z = v1->z + v2->z;
}


void
sub_vec(const vec3 *v1, const vec3 *v2, vec3 *vo)
{
    vo->x = v1->x - v2->x;
    vo->y = v1->y - v2->y;
    vo->z = v1->z - v2->z;
}


void
mul_scalar_vec(float f, const vec3 *v, vec3 *vo)
{
    vo->x = v->x * f;
    vo->y = v->y * f;
    vo->z = v->z * f;
}


void
div_scalar_vec(float f, const vec3 *v, vec3 *vo)
{
    vo->x = v->x / f;
    vo->y = v->y / f;
    vo->z = v->z / f;
}


// Add vector `v` to every vector in triangle `t`.
void
add_tri_vec(const tri *t, const vec3 *v, tri *to)
{
    for(short i=0; i<3; i++) {
        to->p[i].x = t->p[i].x + v->x;
        to->p[i].y = t->p[i].y + v->y;
        to->p[i].z = t->p[i].z + v->z;
    }
}


// Calculate the face normal for triange `t`.
void
normal_tri(const tri *t, vec3 *normal) {
    // Find triangle normal.
    vec3 line0, line1;
    sub_vec(&t->p[1], &t->p[0], &line0);
    sub_vec(&t->p[2], &t->p[0], &line1);

    normal->x = line0.y * line1.z - line0.z * line1.y;
    normal->y = line0.z * line1.x - line0.x * line1.z;
    normal->z = line0.x * line1.y - line0.y * line1.x;

    // Normalize the normal vector. :-)
    float l = sqrtf(normal->x*normal->x
                    + normal->y*normal->y
                    + normal->z*normal->z);
    normal->x /= l;
    normal->y /= l;
    normal->z /= l;
}


// -=[ DRAWING FUNCTIONS ]=----------------------------------------------------
// adapted from:
//   https://github.com/OneLoneCoder/Javidx9/tree/master/ConsoleGameEngine
void
draw_line(int x1, int y1, int x2, int y2, const cchar_t *wch)
{
    int x, y, dx, dy, dx1, dy1, px, py, xe, ye, i;

    dx = x2 - x1; dy = y2 - y1;
    dx1 = abs(dx); dy1 = abs(dy);
    px = 2 * dy1 - dx1; py = 2 * dx1 - dy1;

    if (dy1 <= dx1) {
        if (dx >= 0) {
            x = x1; y = y1; xe = x2;
        }
        else {
            x = x2; y = y2; xe = x1;
        }

        mvadd_wch(y, x, wch);

        for (i = 0; x<xe; i++) {
            x = x + 1;
            if (px<0) {
                px = px + 2 * dy1;
            }
            else {
                if ((dx<0 && dy<0) || (dx>0 && dy>0)) {
                    y = y + 1;
                } else {
                    y = y - 1;
                }
                px = px + 2 * (dy1 - dx1);
            }
            mvadd_wch(y, x, wch);
        }
    }
    else {
        if (dy >= 0) {
            x = x1; y = y1; ye = y2;
        }
        else {
            x = x2; y = y2; ye = y1;
        }

        mvadd_wch(y, x, wch);

        for (i = 0; y<ye; i++) {
            y = y + 1;
            if (py <= 0) {
                py = py + 2 * dx1;
            }
            else {
                if ((dx<0 && dy<0) || (dx>0 && dy>0)) {
                    x = x + 1;
                } else {
                    x = x - 1;
                }
                py = py + 2 * (dx1 - dy1);
            }
            mvadd_wch(y, x, wch);
        }
    }
}


void
draw_tri(const tri *t, const cchar_t *wch)
{
    draw_line((int)t->p[0].x, (int)t->p[0].y, (int)t->p[1].x, (int)t->p[1].y, wch);
    draw_line((int)t->p[1].x, (int)t->p[1].y, (int)t->p[2].x, (int)t->p[2].y, wch);
    draw_line((int)t->p[2].x, (int)t->p[2].y, (int)t->p[0].x, (int)t->p[0].y, wch);
}

// adapted from:
//   https://github.com/OneLoneCoder/Javidx9/tree/master/ConsoleGameEngine
// which was adapted from:
//   https://www.avrfreaks.net/sites/default/files/triangles.c
//   (dead link)
void
fill_tri(const tri *t, const cchar_t *wch)
{
    int x1 = (int)t->p[0].x;
    int x2 = (int)t->p[1].x;
    int x3 = (int)t->p[2].x;

    int y1 = (int)t->p[0].y;
    int y2 = (int)t->p[1].y;
    int y3 = (int)t->p[2].y;

    int t1x, t2x, y, minx, maxx, t1xp, t2xp;
    bool changed1 = false;
    bool changed2 = false;
    int signx1, signx2, dx1, dy1, dx2, dy2;
    int e1, e2;
    // Sort vertices
    if (y1>y2) { SWAP(y1, y2); SWAP(x1, x2); }
    if (y1>y3) { SWAP(y1, y3); SWAP(x1, x3); }
    if (y2>y3) { SWAP(y2, y3); SWAP(x2, x3); }

    // Starting points
    t1x = t2x = x1; y = y1;
    dx1 = (int)(x2 - x1);
    if (dx1<0) {
        dx1 = -dx1; signx1 = -1;
    } else {
        signx1 = 1;
    }
    dy1 = (int)(y2 - y1);

    dx2 = (int)(x3 - x1);
    if (dx2<0) {
        dx2 = -dx2; signx2 = -1;
    } else {
        signx2 = 1;
    }
    dy2 = (int)(y3 - y1);

    if (dy1 > dx1) {   // swap values
	SWAP(dx1, dy1);
	changed1 = true;
    }
    if (dy2 > dx2) {   // swap values
	SWAP(dy2, dx2);
	changed2 = true;
    }

    e2 = (int)(dx2 >> 1);
    // Flat top, just process the second half
    if (y1 == y2) { goto next; }
    e1 = (int)(dx1 >> 1);

    for (int i = 0; i < dx1;) {
	t1xp = 0; t2xp = 0;
	if (t1x<t2x) { minx = t1x; maxx = t2x; }
	else { minx = t2x; maxx = t1x; }
	// process first line until y value is about to change
	while (i<dx1) {
	    i++;
	    e1 += dy1;
	    while (e1 >= dx1) {
		e1 -= dx1;
		if (changed1) { t1xp = signx1; }
                else          { goto next1; }
	    }
	    if (changed1) { break; }
	    else t1x += signx1;
	}
	// Move line
    next1:
	// process second line until y value is about to change
	while (1) {
	    e2 += dy2;
	    while (e2 >= dx2) {
		e2 -= dx2;
		if (changed2) { t2xp = signx2; }
		else          { goto next2; }
	    }
	    if (changed2)     { break; }
	    else              { t2x += signx2; }
	}
    next2:
	if (minx>t1x) { minx = t1x; } if (minx>t2x) { minx = t2x; }
	if (maxx<t1x) { maxx = t1x; } if (maxx<t2x) { maxx = t2x; }
        //drawline(minx, maxx, y);    // Draw line from min to max points found on the y
        for(int i = minx; i <= maxx; i++) {
            mvadd_wch(y, i, wch);
        }
	// Now increase y
	if (!changed1) { t1x += signx1; }
	t1x += t1xp;
	if (!changed2) { t2x += signx2; }
	t2x += t2xp;
	y += 1;
	if (y == y2) { break; }

    }
 next:
    // Second half
    dx1 = (int)(x3 - x2);
    if (dx1<0) { dx1 = -dx1; signx1 = -1; }
    else       { signx1 = 1; }
    dy1 = (int)(y3 - y2);
    t1x = x2;

    if (dy1 > dx1) {   // swap values
	SWAP(dy1, dx1);
	changed1 = true;
    }
    else changed1 = false;

    e1 = (int)(dx1 >> 1);

    for (int i = 0; i <= dx1; i++) {
	t1xp = 0; t2xp = 0;
	if (t1x<t2x) { minx = t1x; maxx = t2x; }
	else         { minx = t2x; maxx = t1x; }
	// process first line until y value is about to change
	while (i<dx1) {
	    e1 += dy1;
	    while (e1 >= dx1) {
		e1 -= dx1;
		if (changed1) { t1xp = signx1; break; }//t1x += signx1;
		else          goto next3;
	    }
	    if (changed1) { break; }
	    else   	  { t1x += signx1; }
	    if (i<dx1)    { i++; }
	}
    next3:
	// process second line until y value is about to change
	while (t2x != x3) {
	    e2 += dy2;
	    while (e2 >= dx2) {
		e2 -= dx2;
		if (changed2) { t2xp = signx2; }
		else          { goto next4; }
	    }
	    if (changed2)     { break; }
	    else              { t2x += signx2; }
	}
    next4:

	if (minx>t1x) {minx = t1x;}
        if (minx>t2x) {minx = t2x;}
	if (maxx<t1x) {maxx = t1x;}
        if (maxx<t2x) {maxx = t2x;}
        for(int i = minx; i <= maxx; i++) {
            mvadd_wch(y, i, wch);
        }
	if (!changed1) { t1x += signx1; }
	t1x += t1xp;
	if (!changed2) { t2x += signx2; }
	t2x += t2xp;
	y += 1;
	if (y>y3) { return; }
    }
}


// -=[ UTILITY FUNCTIONS ]=----------------------------------------------------
void
ncurses_startup()
{
    // per the advice of `man ncurses`
    setlocale(LC_ALL, "");

    // init ncuruses
    initscr();

    // ncurses options
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);
    scrollok(stdscr, FALSE);
    keypad(stdscr, TRUE);

    // color
    start_color();
    use_default_colors();
}


bool load_mesh(const char *path, mesh *m) {
    FILE *fp = fopen(path, "r");
    if(fp == NULL) {fprintf(stderr, "couldn't open %s", path); return false;}

    size_t vec_cap = 16;
    size_t vec_len = 0;
    vec3 *vecs = calloc(vec_cap, sizeof (vec3));

    char line[80];

    // Scan for verticies.
    vec3 v;
    while(fgets(line, 80, fp) != NULL) {
        if(line[0] != 'v') { continue; }

        sscanf(line, "v %f %f %f", &v.x, &v.y, &v.z);

        if(vec_len + 1 >= vec_cap) {
            vec_cap <<= 1;
            vecs = realloc(vecs, vec_cap * sizeof (vec3));
        }

        *(vecs + vec_len) = v;
        vec_len++;
    }

    rewind(fp);

    size_t tri_cap = 16;
    size_t tri_len = 0;
    tri *tris = calloc(tri_cap, sizeof (tri));

    // Scan for faces.
    tri t;
    int i_x, i_y, i_z;
    while(fgets(line, 80, fp) != NULL) {
        if(line[0] != 'f') { continue; }

        sscanf(line, "f %d %d %d", &i_x, &i_y, &i_z);
        t.p[0] = *(vecs + i_x - 1);
        t.p[1] = *(vecs + i_y - 1);
        t.p[2] = *(vecs + i_z - 1);

        if(tri_len + 1 >= tri_cap) {
            tri_cap <<= 1;
            tris = realloc(tris, tri_cap * sizeof (tri));
        }

        *(tris + tri_len) = t;
        tri_len++;
    }

    m->len = (int)tri_len;
    m->tris = calloc(tri_len, sizeof (tri));
    memcpy(m->tris, tris, tri_len * sizeof (tri));

    return true;
}

int
z_cmp(const void *a, const void *b)
{
    tri t1 = *(tri *)a;
    tri t2 = *(tri *)b;

    float t1_z_mid = (t1.p[0].z + t1.p[1].z + t1.p[2].z) / 3.0f;
    float t2_z_mid = (t2.p[0].z + t2.p[1].z + t2.p[2].z) / 3.0f;

    if(t1_z_mid < t2_z_mid) return  1;
    if(t1_z_mid > t2_z_mid) return -1;
    return 0;
}


// -=[ MAIN ]=-----------------------------------------------------------------
int
main()
{
    // Mesh files to load.
    wchar_t *ms_str[] = {L"snowflake.obj", L"cube.obj"};
    int ms_len = LEN(ms_str);
    int ms_i = 0;
    // Yes this is a VLA. I try to avoid them but...
    mesh ms[ms_len];
    for(int i = 0; i<ms_len; i++) {
        char s[32];
        wcstombs(s, ms_str[i], 32);
        load_mesh(s, &ms[i]);
    }

    ncurses_startup();

    // Needed for the perspective transform.
    float near = 0.1f;
    float far = 1000.0f;
    float fov = 86.0f;

    mat4x4 rot_z = {0};
    mat4x4 rot_x = {0};

    vec3 camera = {0};

    // Colors
    short default_pair = 0;
    init_pair(default_pair, -1, -1);
    int shades = 128;
    for(int i = 0; i <= shades && i < COLOR_PAIRS-1; i++) {
        init_color((short)(i + 8),
                   (short)((float)i * (1000.0f / (float)shades)),
                   (short)((float)i * (1000.0f / (float)shades)),
                   (short)((float)i * (1000.0f / (float)shades)));
        init_pair((short)(i+1), (short)(i+8), -1);
    }

    int y_max, x_max;
    getmaxyx(stdscr, y_max, x_max);
    unsigned long long frame_cnt = 0;
    //float target_fps = 15;
    darray tris_to_draw;
    darray_init(&tris_to_draw, sizeof (tri));

    typedef enum render_mode {SHADED, WIREFRAME, OUTLINED, NUM} render_mode;
    wchar_t *render_mode_str[NUM] = {L"shaded", L"wireframe", L"outlined"};
    render_mode mode = SHADED;

    // MAIN LOOP
    while( 1 ) {
        getmaxyx(stdscr, y_max, x_max);

        wint_t key_pressed = 0;
        while(get_wch(&key_pressed) != ERR) {
            switch(key_pressed) {
            case 'q':
                goto cleanup;
                break;
            case 'm':
                mode = (mode + 1) % NUM;
                break;
            case KEY_RIGHT:
                ms_i = (ms_i + 1) % ms_len;
                break;
            case KEY_LEFT:
                if (ms_i == 0) {ms_i = ms_len-1;}
                else           {ms_i = ms_i - 1;}
                break;
            }
        }

        // Transform needs to be recalculated in case the window size changes.
        // The 2x coeff to the aspect ratio is to correct for the fact that
        // characters are not square.
        float aspect = 2 * ((float)y_max / (float)x_max);
        float fov_rad = 1 / tanf(fov * 0.5f / 180.0f * (float)M_PI);
        mat4x4 mat_proj = { .m = {
                {aspect * fov_rad, 0.0f,    0.0f,                       0.0f},
                {0.0f,             fov_rad, 0.0f,                       0.0f},
                {0.0f,             0.0f,    far / (far - near),         1.0f},
                {0.0f,             0.0f,    (-far*near) / (far - near), 0.0f}
            }};

        float theta = (float)frame_cnt / 15.0f / (float)(0.5f*M_PI);

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

        // Cull.
        // Collect only the triangles we want to draw.
        darray_clear(&tris_to_draw);

        for(int i = 0; i < ms[ms_i].len; i++) {
            // we must use seperate vars for each input and output,
            // because mul_mat_vec assumes the input vector doesn't
            // change.
            tri t;
            t = *(ms[ms_i].tris + i);

            // Rotate around z axis.
            tri rotated_z;
            mul_mat_tri(&rot_z, &t, &rotated_z);

            // Rotate around x axis.
            tri rotated_zx;
            mul_mat_tri(&rot_x, &rotated_z, &rotated_zx);

            // Translate away from camera.
            tri translated;
            add_tri_vec(&rotated_zx,
                        &(vec3){.x = 0, .y = 0, .z = 2.0f},
                        &translated);

            // Find triangle normal.
            vec3 normal;
            normal_tri(&translated, &normal);

            // Should this face be drawn?
            float D = normal.x * (translated.p[0].x - camera.x)
                + normal.y * (translated.p[0].y - camera.y)
                + normal.z * (translated.p[0].z - camera.z);

            if(D < 0.0f) {
                darray_push(&tris_to_draw, &translated);
            }
        }

        // Sort triangles by z-depth, so that ones farther away can be
        // drawn before closer ones.
        qsort(tris_to_draw.buf,
              tris_to_draw.len,
              tris_to_draw.elem_size,
              z_cmp);

        // Clear screen before we draw.
        erase();

        // Draw the triangles.
        for(size_t i = 0; i < tris_to_draw.len; i++) {
            tri t = *(tri *)darray_get(&tris_to_draw, i);
            vec3 normal;
            normal_tri(&t, &normal);

            // Light tris by global illumination.
            vec3 light = { 0.0f, 0.0f, -1.0f };
            // normalize light vec
            float l = sqrtf(light.x * light.x
                      + light.y * light.y
                      + light.z * light.z);
            light.x /= l; light.y /= l; light.z /= l;
            float light_dp = light.x * normal.x
                + light.y * normal.y
                + light.z * normal.z;

            tri projected = {0};
            // Apply perspective transform to each point,
            // that is, project triangle from 3d into 2d.
            mul_mat_tri(&mat_proj, &t, &projected);

            // Each point has a range of -1 to +1, so it must be
            // scaled into screen space.
            projected.p[0].x += 1.0f; projected.p[0].y += 1.0f;
            projected.p[1].x += 1.0f; projected.p[1].y += 1.0f;
            projected.p[2].x += 1.0f; projected.p[2].y += 1.0f;

            // This could be collapsed by using a 3x1 transform
            // and scalar multiply.
            projected.p[0].x *= 0.5f * (float)x_max;
            projected.p[0].y *= 0.5f * (float)y_max;
            projected.p[1].x *= 0.5f * (float)x_max;
            projected.p[1].y *= 0.5f * (float)y_max;
            projected.p[2].x *= 0.5f * (float)x_max;
            projected.p[2].y *= 0.5f * (float)y_max;

            short color_pair = (short)(1.0f + (light_dp * (float)shades));
            cchar_t wch_full;
            wchar_t wc_full[] = L"\u2588";
            setcchar(&wch_full, wc_full, A_NORMAL, color_pair, NULL);

            cchar_t wch_blank;
            wchar_t wc_blank[] = L" ";
            setcchar(&wch_blank, wc_blank, A_NORMAL, color_pair, NULL);

            // Finally, we get to draw 'pixels' to our screen.
            if(mode == SHADED) {
                fill_tri(&projected, &wch_full);
            } else if (mode == OUTLINED) {
                fill_tri(&projected, &wch_full);
                draw_tri(&projected, &wch_blank);
            } else {
                draw_tri(&projected, &wch_full);
            }
        }

        wchar_t ws_buf[80] = { 0 };
        swprintf(ws_buf, 80, L"mesh: %ls", ms_str[ms_i]);
        mvaddwstr(0, 7, ws_buf);

        swprintf(ws_buf, 80, L"render mode: %ls", render_mode_str[mode]);
        mvaddwstr(1, 0, ws_buf);

        swprintf(ws_buf, 80, L"term size: %d col, %d row", x_max, y_max);
        mvaddwstr(2, 2, ws_buf);

        swprintf(ws_buf, 80, L"frame count: %d", frame_cnt);
        mvaddwstr(3, 0, ws_buf);

        swprintf(ws_buf, 80, L"\u03B8: %f\u03c0", (float)theta / M_PI);
        mvaddwstr(4, 10, ws_buf);

        frame_cnt++;
        refresh();
        // Sleep for 1/30th of a second.
        usleep(33330);
    }

cleanup:
    for(int i = 0; i < ms_len; i++) {
        free(ms[i].tris);
    }
    endwin();
    return 0;
}
