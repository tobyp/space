#ifndef PARTICLE_H_
#define PARTICLE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define PI 3.1415962

struct vec2 { float x, y; };

typedef struct _cairo cairo_t;

enum {
	BF_VALID = 1,
};

struct body {
	struct vec2 p;
	struct vec2 v;
	struct vec2 prior_p;
	float mass;
	int flags;

	float r, g, b;
	float radius;
	struct {
		size_t cursor;
		size_t size;
		size_t capacity;
		struct vec2 * points;
	} trail;
};

void body_init(struct body * b, float mass, float x, float y);
void body_recalc(struct body * b);
void body_trail(struct body * body, struct vec2 const* point);
void body_merge(struct body * b, struct body * q);

struct galaxy {
	struct body * bodies;
	size_t bodies_size;
};

void galaxy_init(struct galaxy * galaxy, size_t n, double w, double h);
struct body * galaxy_body_add(struct galaxy * galaxy);
void galaxy_integrate(struct galaxy * galaxy, double delta);
void galaxy_render(cairo_t * ctx, struct galaxy * galaxy);
void galaxy_render_trails(cairo_t * ctx, struct galaxy * galaxy);

#endif
