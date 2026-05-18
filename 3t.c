#define NCURSES_WIDECHAR 1
#include <locale.h>
#include <math.h>    // for sinf(), cosf(), tanf()
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>   // fopen, fprintf, etc
#include <stdlib.h>  // for abs()
#include <string.h>  // memcpy()
#include <time.h>
#include <unistd.h>  // for usleep()
#include <wchar.h>   // for wint_t, wchar_t, etc.
#include "alist.h"


#define LEN(X)     (sizeof (X) / sizeof (X)[0])
#define SWAP(M, N) { M ^= N; \
                     N ^= M; \
                     M ^= N; }
#define M_PIf      3.14159265358979323846f


// -=[ STRUCTS / TYPES ]=------------------------------------------------------
typedef struct vec4 {
    float x, y, z, w;
} vec4;

typedef struct tri {
    vec4 p[3];
} tri;

typedef struct mesh {
    char name[32];
    tri *tris;
    int len;
    float radius;
} mesh;

typedef struct mat4x4 {
    float m[4][4];
} mat4x4;


// -=[ VECTOR AND MATRIX OPERATIONS ]=-----------------------------------------
/* TODO rename as <noun>_<verb> */
void mul_mat_vec(const mat4x4 *m, const vec4 *i, vec4 *o);
void mul_mat_tri(const mat4x4 *m, const tri *t, tri *to);
void add_vec(const vec4 *v1, const vec4 *v2, vec4 *vo);
void sub_vec(const vec4 *v1, const vec4 *v2, vec4 *vo);
void mul_scalar_vec(float f, const vec4 *v, vec4 *vo);
void div_scalar_vec(float f, const vec4 *v, vec4 *vo);
void add_tri_vec(const tri *t, const vec4 *v, tri *to);
/* TODO Rename as tri_surface_normal */
void normal_tri(const tri *t, vec4 *normal);
void normalize_vec4(vec4 *v);
float len_vec4(const vec4 *v);
void cross_prod_vec4(const vec4 *v1, const vec4 *v2, vec4 *vo);
float dot_prod_vec4(const vec4 *v1, const vec4 *v2);

/* TODO I think camera matrix is the more common term than projection matrix? */
void init_projection_mat(float fov_degrees, float aspect, float near,
                         float far, mat4x4 *m);
void init_rotx_mat(float theta, mat4x4 *m);
void init_roty_mat(float theta, mat4x4 *m);
void init_rotz_mat(float theta, mat4x4 *m);
void init_trans_mat(float x, float y, float z, mat4x4 *m);


// -=[ RASTERIZATION FUNCTIONS ]=----------------------------------------------
void draw_line(int x1, int y1, int x2, int y2, const cchar_t *wch);
void draw_tri(const tri *t, const cchar_t *wch);
void fill_tri(const tri *t, const cchar_t *wch);


// -=[ UTILITY FUNCTIONS ]=----------------------------------------------------
void ncurses_startup();
bool load_mesh(const char *path, mesh *m);
int z_cmp(const void *a, const void *b);
short lum_to_pair(const float f);


// -=[ GLOBALS ]=--------------------------------------------------------------
static const short grays[] = { /*0,*/
     232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244,
     245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 15};


// -=[ MAIN ]=-----------------------------------------------------------------
int
main(int argc, char **argv)
{
    if (argc <= 1) {
        printf("usage: %s <model.obj>\n", argv[0]);
        return 1;
    }

    struct alist *meshes = alist_new(sizeof (mesh));
    size_t mesh_i = 0;
    for (int i = 1; i < argc && i < 8; i++) {
        mesh m = {0};
        load_mesh(argv[i], &m);
        alist_push(meshes, &m);
    }

    getchar();
    ncurses_startup();

    // Needed for the perspective transform.
    float near = 0.1f;
    float far = 1000.0f;
    float fov = 75.0f;

    mat4x4 rot_z;
    mat4x4 rot_x;
    mat4x4 trans;

    vec4 camera = {0};

    // Colors
    for (short i = 0; i < (short)LEN(grays); i++)
         init_pair(i+1, grays[i], 0);

    int y_max, x_max;
    getmaxyx(stdscr, y_max, x_max);
    unsigned long long frame_cnt = 0;
    //float target_fps = 15;
    struct alist *tris_to_draw = alist_new(sizeof (tri));

    typedef enum render_mode {WIREFRAME, X_RAY, SHADED, OUTLINED, NUM} render_mode;
    char *render_mode_str[NUM] = {"wireframe", "x-ray", "shaded", "outlined"};
    render_mode mode = WIREFRAME;

    // MAIN LOOP
    while( 1 ) {
        getmaxyx(stdscr, y_max, x_max);

        int key_pressed = 0;
        while((key_pressed = getch()) != ERR) {
            switch(key_pressed) {
            case 'q':
                goto cleanup;
                break;
            case 'm':
                mode = (mode + 1) % NUM;
                if (COLORS < 256) /* TODO temp hack for linux console */
                    mode = WIREFRAME;
                break;
            case KEY_RIGHT:
                mesh_i = (mesh_i + 1) % alist_len(meshes);
                break;
            case KEY_LEFT:
                mesh_i = mesh_i == 0 ? alist_len(meshes) - 1 : mesh_i -1;
                break;
            }
        }

        mesh *m_p = alist_get(meshes, mesh_i);

        // Transform needs to be recalculated in case the window size changes.
        // The coeff to the aspect ratio is to correct for the fact that
        // characters are typically taller than they are wide.
        mat4x4 mat_proj;
        init_projection_mat(fov, 2.0f * ((float)y_max / (float)x_max),
                            near, far, &mat_proj);

        float theta = (float)frame_cnt / 15.0f / (0.5f * M_PIf);
        /* theta = 0.0f; */

        init_rotx_mat(theta, &rot_x);
        init_rotz_mat(theta, &rot_z);

        // Cull.
        // Collect only the triangles we want to draw.
        alist_clear(tris_to_draw, NULL);

        for(int i = 0; i < m_p->len; i++) {
            // TODO we can calculate one world matrix and move it
            // outside of the for loop to drastically reduce the
            // number of calculations.
               
            // we must use seperate vars for each input and output,
            // because mul_mat_vec assumes the input vector doesn't
            // change.
            tri t = *(m_p->tris + i);

            // Rotate around z axis.
            tri rotated_z;
            mul_mat_tri(&rot_z, &t, &rotated_z);

            // Rotate around x axis.
            tri rotated_zx;
            mul_mat_tri(&rot_x, &rotated_z, &rotated_zx);

            // Translate away from camera.            
            tri translated;
            init_trans_mat(0, 0, m_p->radius * 1.7f, &trans);
            mul_mat_tri(&trans, &rotated_zx, &translated);

            // Find triangle normal.
            vec4 normal;
            normal_tri(&translated, &normal);

            // Should this face be drawn?
            /* TODO Use tri barycenter rather than first vertex? */
            vec4 cam_ray;
            sub_vec(&translated.p[0], &camera, &cam_ray);
            
            if(mode == X_RAY || (dot_prod_vec4(&normal, &cam_ray) < 0.0f)) {
                alist_push(tris_to_draw, &translated);
            }
        }

        // Sort triangles by z-depth, so that ones farther away can be
        // drawn before closer ones.
	alist_qsort(tris_to_draw, z_cmp);
        // qsort(tris_to_draw.buf,
        //       tris_to_draw.len,
        //       tris_to_draw.elem_size,
        //       z_cmp);

        // Clear screen before we draw.
        erase();

        // Draw the triangles.
        for(size_t i = 0; i < alist_len(tris_to_draw); i++) {
            tri t = *(tri *)alist_get(tris_to_draw, i);
            vec4 normal;
            normal_tri(&t, &normal);

            // Light tris by global illumination.
            vec4 light = { 0.5f, -0.5f, -1.0f, 0.0f };
            normalize_vec4(&light);
            float light_dp = dot_prod_vec4(&normal, &light);

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

            // This step is to account for the fact that vector space's origin
            // is at the bottom left, and screen space is at the top left.
            mat4x4 T_trans0 = {0}, T_reflect = {0}, T_trans1 = {0};
            tri translated = {0}, reflected = {0};
            init_trans_mat(0, -y_max/2, 0, &T_trans0);
            init_trans_mat(0, y_max/2, 0, &T_trans1);
            T_reflect.m[0][0] = 1;
            T_reflect.m[1][1] = -1;
            T_reflect.m[2][2] = 1;
            T_reflect.m[3][3] = 1;

            mul_mat_tri(&T_trans0, &projected, &translated);
            mul_mat_tri(&T_reflect, &translated, &reflected);
            mul_mat_tri(&T_trans1, &reflected, &translated);
            projected = translated;

            // Finally, we get to draw 'pixels' to our screen.
            cchar_t wch;
            if (mode == SHADED) {
                // attr_set(A_NORMAL, lum_to_pair(light_dp), NULL);
                setcchar(&wch, L"\u2588", A_NORMAL,
                         lum_to_pair(light_dp), NULL);
                fill_tri(&projected, &wch);
            }
            if (mode == OUTLINED) {
                setcchar(&wch, L"\u2588", A_NORMAL,
                         lum_to_pair(light_dp), NULL);
                fill_tri(&projected, &wch);
                setcchar(&wch, L"\u2588", A_NORMAL,
                         lum_to_pair(light_dp * 0.34f), NULL);
                draw_tri(&projected, &wch);
            }
            if (mode == WIREFRAME) {
                setcchar(&wch, L"\u2588", A_NORMAL, 0, NULL);
                draw_tri(&projected, &wch);
            }
            if (mode == X_RAY) {
                setcchar(&wch, L"\u2588", A_NORMAL,
                         lum_to_pair(light_dp < 0.15f ? 0.15f : light_dp),
                         NULL);
                draw_tri(&projected, &wch);
            }
        }
        attr_set(A_NORMAL, 0, NULL);

        mvprintw(0, 2, "mesh name: %s", m_p->name);
        mvprintw(1, 1, "rendermode: %s", render_mode_str[mode]);
        mvprintw(2, 2, "term size: %d col, %d row", x_max, y_max);
        mvprintw(3, 0, "frame count: %lld", frame_cnt);
        mvprintw(4, 3, "theta/pi: %1.2f", theta / M_PIf);
        addch(ACS_PI);

        frame_cnt++;
        refresh();
        // Sleep for 1/30th of a second.
        struct timespec dur = { .tv_sec = 0, .tv_nsec = 33330000 };
        struct timespec rem = { 0 };
        while (nanosleep(&dur, &rem) != 0)
                        dur = rem;
    }

cleanup:
    alist_free(tris_to_draw, NULL);
    // for(size_t i = 0; i < meshes.len; i++) {
    //     mesh *m_p = darray_get(&meshes, i);
    //     free(m_p->tris);
    // }
    alist_free(meshes, NULL);
    endwin();
    return 0;
}


// -=[ VECTOR AND MATRIX OPERATIONS ]=-----------------------------------------
void
mul_mat_vec(const mat4x4 *m, const vec4 *i, vec4 *o)
{
    float w;
    /* TODO verify that this is correct for vec4. */
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


void
add_vec(const vec4 *v1, const vec4 *v2, vec4 *vo)
{
    vo->x = v1->x + v2->x;
    vo->y = v1->y + v2->y;
    vo->z = v1->z + v2->z;
    vo->w = v1->w + v2->w;
}


void
sub_vec(const vec4 *v1, const vec4 *v2, vec4 *vo)
{
    vo->x = v1->x - v2->x;
    vo->y = v1->y - v2->y;
    vo->z = v1->z - v2->z;
    vo->w = v1->w - v2->w;
}


void
mul_scalar_vec(float f, const vec4 *v, vec4 *vo)
{
    vo->x = v->x * f;
    vo->y = v->y * f;
    vo->z = v->z * f;
    vo->w = v->w * f;
}


void
div_scalar_vec(float f, const vec4 *v, vec4 *vo)
{
    vo->x = v->x / f;
    vo->y = v->y / f;
    vo->z = v->z / f;
    vo->w = v->w / f;
}


// Add vector `v` to every vector in triangle `t`.
void
add_tri_vec(const tri *t, const vec4 *v, tri *to)
{
    /* TODO call add_vec instead */
    for(short i=0; i<3; i++) {
        to->p[i].x = t->p[i].x + v->x;
        to->p[i].y = t->p[i].y + v->y;
        to->p[i].z = t->p[i].z + v->z;
        to->p[i].w = t->p[i].w + v->w;
    }
}


// Calculate the face normal for triange `t`.
void
normal_tri(const tri *t, vec4 *normal) {
    // Find triangle normal.
    vec4 line0, line1;
    sub_vec(&t->p[1], &t->p[0], &line0);
    sub_vec(&t->p[2], &t->p[0], &line1);

    normal->x = line0.y * line1.z - line0.z * line1.y;
    normal->y = line0.z * line1.x - line0.x * line1.z;
    normal->z = line0.x * line1.y - line0.y * line1.x;

    // Normalize the normal vector. :-)
    normalize_vec4(normal);
}

void normalize_vec4(vec4 *v) {
    /*
     * TODO be more specific about which 'normalization' operation this does,
     * since in graphics it can mean a number of different things.
     *
     * For future ref, we are just pretending this is a cartesion vector and
     * ignoring w.
     *
     * ref: https://cs418.cs.illinois.edu/website/text/math2.html
     * (I have Lay on hand but they don't say much about this issue.)
     */
    float l = len_vec4(v);
    v->x /= l;
    v->y /= l;
    v->z /= l;
}

float len_vec4(const vec4 *v) {
    /*
     * TODO which length operation do mean? here we are pretending it's a
     * Cartesian vector.
     */
    return sqrtf(dot_prod_vec4(v, v));
}

void cross_prod_vec4(const vec4 *v1, const vec4 *v2, vec4 *vo) {
    /* TODO Account for w. Or don't. */
    vo->x = v1->y * v2->z - v1->z * v2->y;
    vo->y = v1->z * v2->x - v1->x * v2->z;
    vo->z = v1->x * v2->y - v1->y * v2->x;
}

float dot_prod_vec4(const vec4 *v1, const vec4 *v2) {
    /* TODO Account for w. Or don't. */
    return v1->x*v2->x + v1->y*v2->y + v1->z*v2->z + v1->w*v2->w;
}

void
init_projection_mat(float fov_degrees, float aspect, float near, float far,
                    mat4x4 *m)
{
    memset(m, (int)0.0f, sizeof (mat4x4));
    float fov_rad = 1.0f / tanf(fov_degrees * 0.5f / 180.0f * M_PIf);
    m->m[0][0] = aspect * fov_rad;
    m->m[1][1] = fov_rad;
    m->m[2][2] = far / (far - near);
    m->m[3][2] = (far * near) / (far - near);
    m->m[2][3] = 1.0f;
}

void
init_rotx_mat(float theta, mat4x4 *m)
{
    memset(m, (int)0.0f, sizeof (mat4x4));
    m->m[0][0] = 1;
    m->m[1][1] = cosf(theta * 0.5f);
    m->m[1][2] = sinf(theta * 0.5f);
    m->m[2][1] = -sinf(theta * 0.5f);
    m->m[2][2] = cosf(theta * 0.5f);
    m->m[3][3] = 1;
}

void
init_roty_mat(float theta, mat4x4 *m)
{
    /* STUB */
    memset(m, (int)0.0f, sizeof (mat4x4));
    (void)theta; (void)m;
}

void
init_rotz_mat(float theta, mat4x4 *m)
{
    memset(m, (int)0.0f, sizeof (mat4x4));
    m->m[0][0] = cosf(theta);
    m->m[0][1] = sinf(theta);
    m->m[1][0] = -sinf(theta);
    m->m[1][1] = cosf(theta);
    m->m[2][2] = 1;
    m->m[3][3] = 1;
}

void
init_trans_mat(float x, float y, float z, mat4x4 *m)
{
    memset(m, (int)0.0f, sizeof (mat4x4));
    for (int i = 0; i < 3; i++)
        m->m[i][i] = 1.0f;
    m->m[3][0] = x;
    m->m[3][1] = y;
    m->m[3][2] = z;
}
    

// -=[ RASTERIZING FUNCTIONS ]=------------------------------------------------
// adapted from:
//   https://github.com/OneLoneCoder/Javidx9/tree/master/ConsoleGameEngine
void
draw_line(int x1, int y1, int x2, int y2, const cchar_t *wch)
{
    int x, y, dx, dy, dx1, dy1, px, py, xe, ye;

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

        for ( ; x<xe; ) {
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

        for ( ; y<ye; ) {
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
            else             { t1x += signx1; }
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
    // TODO handle errors if, eg, start_color() fails.

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
    /* If a terminal has a transparent bg, this will keep it transparent. */
    /* use_default_colors(); */
}


bool load_mesh(const char *path, mesh *m) {
    FILE *fp = fopen(path, "r");
    if(fp == NULL) {
        fprintf(stderr, "couldn't open %s", path);
        return false;
    }

    float radius = 0.0f;

    size_t vec_cap = 16;
    size_t vec_len = 0;
    vec4 *vecs = calloc(vec_cap, sizeof (vec4));

    char line[80];

    // Scan for verticies.
    while(fgets(line, 80, fp) != NULL) {
        if(line[0] != 'v') { continue; }

        vec4 v = {0.0f, 0.0f, 0.0f, 1.0f};
        sscanf(line, "v %f %f %f", &v.x, &v.y, &v.z);

        if(vec_len + 1 >= vec_cap) {
            vec_cap <<= 1;
            vecs = realloc(vecs, vec_cap * sizeof (vec4));
        }
        *(vecs + vec_len) = v;
        vec_len++;

        float dist = cbrtf(powf(v.x, 3) + powf(v.y, 3) + powf(v.z, 3));
        if (dist > radius)
            radius = dist;
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

    m->tris = calloc(tri_len, sizeof (tri));
    m->len = (int)tri_len;
    strncpy(m->name, path, 32);
    m->radius = radius;
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

short
lum_to_pair(const float f)
{
        if (f < 0.0f) return 1;
        if (f > 1.0f) return LEN(grays);
        return 1 + (short)(f * LEN(grays) - 1);
}
