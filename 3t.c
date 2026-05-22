#define NCURSES_WIDECHAR 1
#include <locale.h>
#include <math.h>     /* for sinf(), cosf(), tanf() */
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>    /* fopen, fprintf, etc */
#include <stdlib.h>   /* for abs() */
#include <string.h>   /* memcpy() */
#include <time.h>
#include <unistd.h>   /* for access(), usleep() */
#include <wchar.h>    /* for wint_t, wchar_t, etc. */
#include <sys/stat.h> /* struct stat, stat() */
#include "alist.h"


#define LEN(X)		(sizeof (X) / sizeof (X)[0])
#define SWAP(M, N)	{ M ^= N;	\
			  N ^= M;	\
			  M ^= N; }
#define M_PIf		3.14159265358979323846f


/* -=[ STRUCTS / TYPES ]=---------------------------------------------------- */
struct vec {
	union {
		struct { float x, y, z, w; };
		float xs[4];
	};
};

struct tri {
	union {
		struct { struct vec v0; struct vec v1; struct vec v2; };
		struct vec v[3];
	};
};

struct mesh {
	struct alist *tris;
	char name[32];
	float radius;
};

struct matrix {
	float m[4][4];
};


/* -=[ VECTOR AND MATRIX FUNCTIONS ]=---------------------------------------- */
void	matrix_tri_mul(const struct matrix *m, const struct tri *t,
	    struct tri *to);
void	matrix_vec_mul(const struct matrix *m, const struct vec *i,
	    struct vec *o);

void	tri_vec_add(const struct tri *t, const struct vec *v, struct tri *to);
void	tri_normal(const struct tri *t, struct vec *normal);

void	vec_cross_prod(const struct vec *v1, const struct vec *v2,
	    struct vec *vo);
float	vec_dot_prod(const struct vec *v1, const struct vec *v2);
void	vec_add(const struct vec *v1, const struct vec *v2, struct vec *vo);
void	vec_sub(const struct vec *v1, const struct vec *v2, struct vec *vo);
void	vec_scalar_div(const struct vec *v, float f, struct vec *vo);
void	vec_scalar_mul(const struct vec *v, float f, struct vec *vo);
void	vec_normalize(struct vec *v);
float	vec_len(const struct vec *v);

void	init_projection_matrix(float fov_degrees, float aspect, float near,
	    float far, struct matrix *m);
void	init_rotx_matrix(float theta, struct matrix *m);
void	init_roty_matrix(float theta, struct matrix *m);
void	init_rotz_matrix(float theta, struct matrix *m);
void	init_trans_matrix(float x, float y, float z, struct matrix *m);
void	init_reflection_matrix(bool x, bool y, bool z, struct matrix *m);
void	init_id_matrix(struct matrix *m);


/* -=[ RASTERIZATION FUNCTIONS ]=-------------------------------------------- */
void	tri_fill(const struct tri *t, const cchar_t *wch);
void	tri_draw(const struct tri *t, const cchar_t *wch);

void	line_draw(int x1, int y1, int x2, int y2, const cchar_t *wch);


/* -=[ UTILITY FUNCTIONS ]=-------------------------------------------------- */
void	ncurses_startup();
void	init_mesh(struct mesh *m);
int	load_mesh(const char *path, struct mesh *m);
int	z_cmp(const void *a, const void *b);
short	lum_to_pair(const float f);


/* -=[ GLOBALS ]=------------------------------------------------------------ */
static const short grays[] = { /*0,*/
	232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244,
	245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 15};
static enum render_mode {WIREFRAME, X_RAY, SHADED, OUTLINED, NUM}
	mode = SHADED;
static const char *mode_str[NUM] = {"wireframe", "x-ray", "shaded",
	"outlined"};


/*  -=[ MAIN ]=-------------------------------------------------------------- */
int
main(int argc, char **argv)
{
	getchar();
	if (argc <= 1) {
		printf("usage: %s <model.obj>\n", argv[0]);
		return 0;
	}

	/* Load indicated meshes. */
	struct alist *meshes = alist_new(sizeof (struct mesh));
	size_t mesh_i = 0;
	for (int i = 1; i < argc; i++) {
		struct mesh m;
		init_mesh(&m);
		strncpy(m.name, argv[i], sizeof (char) * 32);
		if (load_mesh(argv[i], &m)) {
			fprintf(stderr, "%s: error loading mesh at \"%s\"\n",
				argv[0], argv[1]);
			return 1;
		}
		alist_push(meshes, &m);
	}

	ncurses_startup();

	/* MAIN LOOP */
	struct alist *tris_to_draw = alist_new(sizeof (struct tri));
	unsigned long long frame_cnt = 0;
	for ( ;; ) {
		/* INPUT */
		int key_pressed = 0;
		while ((key_pressed = getch()) != ERR) {
			switch(key_pressed) {
			case 'q':
				goto cleanup;
				break;
			case 'm':
				mode = (mode + 1) % NUM;
				break;
			case KEY_RIGHT:
				mesh_i = (mesh_i + 1) % alist_len(meshes);
				break;
			case KEY_LEFT:
				if (mesh_i == 0)
					mesh_i = alist_len(meshes) - 1;
				else
					mesh_i = mesh_i -1;;
				break;
			}
		}

		int y_max, x_max;
		getmaxyx(stdscr, y_max, x_max);

		/* Needed for the perspective transform. */
		float near = 0.1f;
		float far = 1000.0f;
		float fov = 75.0f;

		struct matrix rot_z;
		struct matrix rot_x;
		struct matrix trans;
		float theta = (float)frame_cnt / 15.0f / (0.5f * M_PIf);

		init_rotz_matrix(theta, &rot_z);
		init_rotx_matrix(theta, &rot_x);

		struct vec camera = {0};

		alist_clear(tris_to_draw, NULL);

		/* For each triangle of mesh... */
		struct mesh *m_p = alist_get(meshes, mesh_i);
		for (size_t i = 0; i < alist_len(m_p->tris); i++) {
			/*
			 * TODO we can calculate one world matrix and move it
			 * outside of the for loop to drastically reduce the
			 * number of calculations.
			 */

			/*
			 * we must use seperate vars for each input and output,
			 * because mul_mat_vec assumes the input vector doesn't
			 * change.
			 */

			/* Rotate around z axis. */
			struct tri rotated_z;
			matrix_tri_mul(&rot_z, alist_get(m_p->tris, i), &rotated_z);

			/* Rotate around x axis. */
			struct tri rotated_zx;
			matrix_tri_mul(&rot_x, &rotated_z, &rotated_zx);

			/* Translate away from camera. */
			struct tri translated;
			init_trans_matrix(0, 0, m_p->radius * 1.7f, &trans);
			matrix_tri_mul(&trans, &rotated_zx, &translated);

			/* Find triangle normal. */
			struct vec normal;
			tri_normal(&translated, &normal);

			/* Should this face be drawn? */
			struct vec cam_ray;
			vec_sub(&translated.v[0], &camera, &cam_ray);

			if (mode == X_RAY
			   || (vec_dot_prod(&normal, &cam_ray) < 0.0f)) {
				alist_push(tris_to_draw, &translated);
			}
		}

		/*
		 * Sort triangles by z-depth, so that ones farther away can be
		 * drawn before closer ones.
		 */
		alist_qsort(tris_to_draw, z_cmp);

		/* Clear screen before we draw. */
		erase();

		/* Draw the triangles. */
		for (size_t i = 0; i < alist_len(tris_to_draw); i++) {
			struct tri t =
			    *(struct tri *)alist_get(tris_to_draw, i);
			struct vec normal;
			tri_normal(&t, &normal);

			/* Light tris by global illumination. */
			struct vec light = { .xs = {0.5f, .75f, -1.0f, 0.0f} };
			vec_normalize(&light);
			float light_dp = vec_dot_prod(&normal, &light);

			/*
			 * Transform needs to be recalculated in case the window
			 * size changes.  The coeff to the aspect ratio is to
			 * correct for the fact that characters are typically
			 * taller than they are wide.
			 */
			struct matrix mat_proj;
			init_projection_matrix(fov,
					       2.0f * ((float)y_max / (float)x_max),
					       near, far, &mat_proj);

			struct tri projected;
			/* Apply perspective transform to each point. */
			matrix_tri_mul(&mat_proj, &t, &projected);

			/*
			 * Each point has a range of -1 to +1 so it must be
			 * scaled into screen space.
			 */
			struct vec offset = { .xs = {1.0f, 1.0f, 0.0f, 0.0f}};
			struct tri shifted;
			tri_vec_add(&projected, &offset, &shifted);

			/* This could be collapsed by using a 3x1 transform */
			/* and scalar multiply. */
			struct matrix T_scaler;
			struct tri scaled;
			init_id_matrix(&T_scaler);
			T_scaler.m[0][0] = 0.5f * (float)x_max;
			T_scaler.m[1][1] = 0.5f * (float)y_max;
			matrix_tri_mul(&T_scaler, &shifted, &scaled);

			/*
			 * This step is to account for the fact that vector
			 * space's origin is at the bottom left, and screen
			 * space is at the top left.
			 */
			struct matrix	T_trans0 = {0};
			struct matrix	T_reflect = {0};
			struct matrix	T_trans1 = {0};
			struct tri	translated = {0};
			struct tri	reflected = {0};
			init_trans_matrix(0, -(float)y_max/2, 0, &T_trans0);
			init_trans_matrix(0, (float)y_max/2, 0, &T_trans1);
			init_reflection_matrix(false, true, false, &T_reflect);

			matrix_tri_mul(&T_trans0, &scaled, &translated);
			matrix_tri_mul(&T_reflect, &translated, &reflected);
			matrix_tri_mul(&T_trans1, &reflected, &translated);

			/* Finally, we get to draw 'pixels' to our screen. */
			cchar_t wch;
			if (mode == SHADED) {
				setcchar(&wch, L"\u2588", A_NORMAL,
					 lum_to_pair(light_dp), NULL);
				tri_fill(&translated, &wch);
			}
			if (mode == OUTLINED) {
				setcchar(&wch, L"\u2588", A_NORMAL,
					 lum_to_pair(light_dp), NULL);
				tri_fill(&translated, &wch);
				setcchar(&wch, L"\u2588", A_NORMAL,
					 lum_to_pair(light_dp * 0.34f), NULL);
				tri_draw(&translated, &wch);
			}
			if (mode == WIREFRAME) {
				setcchar(&wch, L"\u2588", A_NORMAL, 0, NULL);
				tri_draw(&translated, &wch);
			}
			if (mode == X_RAY) {
				setcchar(&wch, L"\u2588", A_NORMAL,
					 lum_to_pair(light_dp < 0.15f ? 0.15f : light_dp),
					 NULL);
				tri_draw(&translated, &wch);
			}
		}
		attr_set(A_NORMAL, 0, NULL);

		mvprintw(0, 2, "mesh name: %s",
			 ((struct mesh *)alist_get(meshes, mesh_i))->name);
		mvprintw(1, 1, "rendermode: %s", mode_str[mode]);
		mvprintw(2, 2, "term size: %d col, %d row", x_max, y_max);
		mvprintw(3, 0, "frame count: %lld", frame_cnt);
		mvprintw(4, 3, "theta/pi: %1.2f", theta / M_PIf);
		addch(ACS_PI);

		frame_cnt++;
		refresh();
		/* Sleep for 1/30th of a second. */
		struct timespec dur = { .tv_sec = 0, .tv_nsec = 33330000 };
		struct timespec rem = { 0 };
		while (nanosleep(&dur, &rem) != 0)
                        dur = rem;
	}

 cleanup:
	alist_free(tris_to_draw, NULL);
	alist_free(meshes, NULL);
	endwin();
	return 0;
}


/* -=[ VECTOR AND MATRIX OPERATIONS ]=--------------------------------------- */
/* Multiply each vector of triangle `t` with matrix `m`. */
void
matrix_tri_mul(const struct matrix *m, const struct tri *t, struct tri *to)
{
	for (short i = 0; i < 3; i++) {
		matrix_vec_mul(m, &t->v[i], &to->v[i]);
	}
}


void
matrix_vec_mul(const struct matrix *m, const struct vec *i, struct vec *o)
{
	float w;
	/* TODO verify that this is correct for struct vec. */
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


/* Add vector `v` to every vector in triangle `t`. */
void
tri_vec_add(const struct tri *t, const struct vec *v, struct tri *to)
{
	/* TODO call vec_add instead */
	for (short i=0; i<3; i++) {
		to->v[i].x = t->v[i].x + v->x;
		to->v[i].y = t->v[i].y + v->y;
		to->v[i].z = t->v[i].z + v->z;
		to->v[i].w = t->v[i].w + v->w;
	}
}

void
tri_normal(const struct tri *t, struct vec *normal) {
	/* Find triangle normal. */
	struct vec line0, line1;
	vec_sub(&t->v[1], &t->v[0], &line0);
	vec_sub(&t->v[2], &t->v[0], &line1);

	normal->x = line0.y * line1.z - line0.z * line1.y;
	normal->y = line0.z * line1.x - line0.x * line1.z;
	normal->z = line0.x * line1.y - line0.y * line1.x;

	/* Normalize the normal vector. :-) */
	vec_normalize(normal);
}

void vec_cross_prod(const struct vec *v1, const struct vec *v2, struct vec *vo)
{
	/* TODO Account for w. Or don't. */
	vo->x = v1->y * v2->z - v1->z * v2->y;
	vo->y = v1->z * v2->x - v1->x * v2->z;
	vo->z = v1->x * v2->y - v1->y * v2->x;
}

float vec_dot_prod(const struct vec *v1, const struct vec *v2)
{
	/* TODO Account for w. Or don't. */
	return v1->x*v2->x + v1->y*v2->y + v1->z*v2->z + v1->w*v2->w;
}

void
vec_add(const struct vec *v1, const struct vec *v2, struct vec *vo)
{
	vo->x = v1->x + v2->x;
	vo->y = v1->y + v2->y;
	vo->z = v1->z + v2->z;
	vo->w = v1->w + v2->w;
}

void
vec_sub(const struct vec *v1, const struct vec *v2, struct vec *vo)
{
	vo->x = v1->x - v2->x;
	vo->y = v1->y - v2->y;
	vo->z = v1->z - v2->z;
	vo->w = v1->w - v2->w;
}

void
vec_scalar_mul(const struct vec *v, float f, struct vec *vo)
{
	vo->x = v->x * f;
	vo->y = v->y * f;
	vo->z = v->z * f;
	vo->w = v->w * f;
}

void
vec_scalar_div(const struct vec *v, float f, struct vec *vo)
{
	vo->x = v->x / f;
	vo->y = v->y / f;
	vo->z = v->z / f;
	vo->w = v->w / f;
}

void vec_normalize(struct vec *v)
{
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
	float l = vec_len(v);
	v->x /= l;
	v->y /= l;
	v->z /= l;
}

float vec_len(const struct vec *v) {
	/*
	 * TODO which length operation do mean? here we are pretending it's a
	 * Cartesian vector.
	 */
	return sqrtf(vec_dot_prod(v, v));
}

void
init_projection_matrix(float fov_degrees, float aspect, float near, float far,
		       struct matrix *m)
{
	memset(m, (int)0.0f, sizeof (struct matrix));
	float fov_rad = 1.0f / tanf(fov_degrees * 0.5f / 180.0f * M_PIf);
	m->m[0][0] = aspect * fov_rad;
	m->m[1][1] = fov_rad;
	m->m[2][2] = far / (far - near);
	m->m[3][2] = (far * near) / (far - near);
	m->m[2][3] = 1.0f;
}

void
init_rotx_matrix(float theta, struct matrix *m)
{
	memset(m, (int)0.0f, sizeof (struct matrix));
	m->m[0][0] = 1;
	m->m[1][1] = cosf(theta * 0.5f);
	m->m[1][2] = sinf(theta * 0.5f);
	m->m[2][1] = -sinf(theta * 0.5f);
	m->m[2][2] = cosf(theta * 0.5f);
	m->m[3][3] = 1;
}

void
init_roty_matrix(float theta, struct matrix *m)
{
	/* STUB */
	memset(m, (int)0.0f, sizeof (struct matrix));
	(void)theta; (void)m;
}

void
init_rotz_matrix(float theta, struct matrix *m)
{
	memset(m, (int)0.0f, sizeof (struct matrix));
	m->m[0][0] = cosf(theta);
	m->m[0][1] = sinf(theta);
	m->m[1][0] = -sinf(theta);
	m->m[1][1] = cosf(theta);
	m->m[2][2] = 1;
	m->m[3][3] = 1;
}

void
init_trans_matrix(float x, float y, float z, struct matrix *m)
{
	memset(m, (int)0.0f, sizeof (struct matrix));
	for (int i = 0; i < 3; i++)
		m->m[i][i] = 1.0f;
	m->m[3][0] = x;
	m->m[3][1] = y;
	m->m[3][2] = z;
}

void
init_reflection_matrix(bool x, bool y, bool z, struct matrix *m)
{
	init_id_matrix(m);
	if (x)
		m->m[0][0] = -1;
	if (y)
		m->m[1][1] = -1;
	if (z)
		m->m[2][2] = -1;
}

void
init_id_matrix(struct matrix *m)
{
	memset(m, (int)0.0f, sizeof (struct matrix));
	for (int i = 0; i < 4; i++)
		m->m[i][i] = 1.0f;
}


/* -=[ RASTERIZING FUNCTIONS ]=---------------------------------------------- */
/*
* adapted from:
*   https://github.com/OneLoneCoder/Javidx9/tree/master/ConsoleGameEngine
* adapted from:
*   https://github.com/OneLoneCoder/Javidx9/tree/master/ConsoleGameEngine
* which was adapted from:
*   https://www.avrfreaks.net/sites/default/files/triangles.c
*   (dead link)
*/
void
tri_fill(const struct tri *t, const cchar_t *wch)
{
	int x1 = (int)t->v[0].x;
	int x2 = (int)t->v[1].x;
	int x3 = (int)t->v[2].x;

	int y1 = (int)t->v[0].y;
	int y2 = (int)t->v[1].y;
	int y3 = (int)t->v[2].y;

	int t1x, t2x, y, minx, maxx, t1xp, t2xp;
	bool changed1 = false;
	bool changed2 = false;
	int signx1, signx2, dx1, dy1, dx2, dy2;
	int e1, e2;
	/* Sort vertices */
	if (y1>y2) { SWAP(y1, y2); SWAP(x1, x2); }
	if (y1>y3) { SWAP(y1, y3); SWAP(x1, x3); }
	if (y2>y3) { SWAP(y2, y3); SWAP(x2, x3); }

	/* Starting points */
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

	if (dy1 > dx1) {   /* swap values */
		SWAP(dx1, dy1);
		changed1 = true;
	}
	if (dy2 > dx2) {   /* swap values */
		SWAP(dy2, dx2);
		changed2 = true;
	}

	e2 = (int)(dx2 >> 1);
	/* Flat top, just process the second half */
	if (y1 == y2) { goto next; }
	e1 = (int)(dx1 >> 1);

	for (int i = 0; i < dx1;) {
		t1xp = 0; t2xp = 0;
		if (t1x<t2x) { minx = t1x; maxx = t2x; }
		else { minx = t2x; maxx = t1x; }
		/* process first line until y value is about to change */
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
		/* Move line */
	next1:
		/* process second line until y value is about to change */
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
		/* Draw line from min to max points found on the y */
		/* drawline(minx, maxx, y); */
		for (int i = minx; i <= maxx; i++) {
			mvadd_wch(y, i, wch);
		}
		/* Now increase y */
		if (!changed1) { t1x += signx1; }
		t1x += t1xp;
		if (!changed2) { t2x += signx2; }
		t2x += t2xp;
		y += 1;
		if (y == y2) { break; }

	}
 next:
	/* Second half */
	dx1 = (int)(x3 - x2);
	if (dx1<0) { dx1 = -dx1; signx1 = -1; }
	else       { signx1 = 1; }
	dy1 = (int)(y3 - y2);
	t1x = x2;

	if (dy1 > dx1) {   /* swap values */
		SWAP(dy1, dx1);
		changed1 = true;
	}
	else changed1 = false;

	e1 = (int)(dx1 >> 1);

	for (int i = 0; i <= dx1; i++) {
		t1xp = 0; t2xp = 0;
		if (t1x<t2x) { minx = t1x; maxx = t2x; }
		else         { minx = t2x; maxx = t1x; }
		/* process first line until y value is about to change */
		while (i<dx1) {
			e1 += dy1;
			while (e1 >= dx1) {
				e1 -= dx1;
				if (changed1) { t1xp = signx1; break; } /*t1x += signx1; */
				else          goto next3;
			}
			if (changed1) { break; }
			else             { t1x += signx1; }
			if (i<dx1)    { i++; }
		}
	next3:
		/* process second line until y value is about to change */
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
		for (int i = minx; i <= maxx; i++) {
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

void
tri_draw(const struct tri *t, const cchar_t *wch)
{
	line_draw((int)t->v[0].x, (int)t->v[0].y,
	    (int)t->v[1].x, (int)t->v[1].y, wch);
	line_draw((int)t->v[1].x, (int)t->v[1].y,
	    (int)t->v[2].x, (int)t->v[2].y, wch);
	line_draw((int)t->v[2].x, (int)t->v[2].y,
	    (int)t->v[0].x, (int)t->v[0].y, wch);
}

void
line_draw(int x1, int y1, int x2, int y2, const cchar_t *wch)
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


/* -=[ UTILITY FUNCTIONS ]=-------------------------------------------------- */
void
ncurses_startup()
{
	/* TODO handle errors if, eg, start_color() fails. */

	/* per the advice of `man ncurses` */
	setlocale(LC_ALL, "");

	/* init ncuruses */
	initscr();

	/* ncurses options */
	cbreak();
	noecho();
	nodelay(stdscr, TRUE);
	curs_set(0);
	scrollok(stdscr, FALSE);
	keypad(stdscr, TRUE);

	start_color();
	/* If a terminal has a transparent bg this will keep it transparent: */
	/* use_default_colors(); */

	/* Colors */
	for (short i = 0; i < (short)LEN(grays); i++)
		init_pair(i+1, grays[i], 0);
}

void
init_mesh(struct mesh *m)
{
	m->tris = alist_new(sizeof (struct tri));
	memset(m->name, 0, sizeof (char) * 32);
	m->radius = 0.0f;
}

int
load_mesh(const char *path, struct mesh *m)
{
	/* Does path exist and can it be read? */
	if (access(path, R_OK) != 0)
		return 1;

	/* Is it a regular file? */
	struct stat sb;
	stat(path, &sb);
	if (!S_ISREG(sb.st_mode))
		return 1;

	/* fopen still might fail for a number of reasons. */
	FILE *fp = fopen(path, "r");
	if (fp == NULL)
		return 1;

	/* Scan for verticies. */
	struct alist *vecs = alist_new(sizeof (struct vec));
	float d_max = 0.0f;
	char line[80];
	while (fgets(line, 80, fp) != NULL) {
		if (line[0] != 'v') { continue; }

		struct vec v = { .xs = {0.0f, 0.0f, 0.0f, 1.0f}};
		sscanf(line, "v %f %f %f", &v.x, &v.y, &v.z);

		alist_push(vecs, &v);

		float d = vec_len(&v);
		if (d > d_max)
			d_max = d;
	}
	m->radius = d_max;

	rewind(fp);

	/* Scan for faces. */
	struct tri t;
	size_t i_x, i_y, i_z;
	while (fgets(line, 80, fp) != NULL) {
		if (line[0] != 'f') { continue; }

		sscanf(line, "f %zu %zu %zu", &i_x, &i_y, &i_z);
		t.v0 = *(struct vec *)alist_get(vecs, --i_x);
		t.v1 = *(struct vec *)alist_get(vecs, --i_y);
		t.v2 = *(struct vec *)alist_get(vecs, --i_z);

		alist_push(m->tris, &t);
	}

	alist_free(vecs, NULL);
	return 0;
}

int
z_cmp(const void *a, const void *b)
{
	struct tri t1 = *(struct tri *)a;
	struct tri t2 = *(struct tri *)b;

	float t1_z_mid = (t1.v[0].z + t1.v[1].z + t1.v[2].z) / 3.0f;
	float t2_z_mid = (t2.v[0].z + t2.v[1].z + t2.v[2].z) / 3.0f;

	if (t1_z_mid < t2_z_mid) return  1;
	if (t1_z_mid > t2_z_mid) return -1;
	return 0;
}

short
lum_to_pair(const float f)
{
        if (f < 0.0f) return 1;
        if (f > 1.0f) return LEN(grays);
        return 1 + (short)(f * LEN(grays) - 1);
}
