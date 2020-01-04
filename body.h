#ifndef PARTICLE_H_
#define PARTICLE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define PI 3.1415962

typedef double coord_t;

struct vec2 { coord_t x, y; };

typedef struct _cairo cairo_t;

enum {
	BF_SIMULATE = 1,
	BF_ALLOCATED = 2,
	BF_TRAIL = 4,
	BF_EXISTS = 8,
};

enum {
	RF_PARTICLE = 0x10,
	RF_TRAIL = 0x20,
	RF_VELOCITY = 0x40,
	RF_GRID = 0x80,
};

struct body {
	struct vec2 p;
	struct vec2 v;
	float mass;

	unsigned flags;

	float r, g, b;
	float radius;
	struct {
		size_t end;
		size_t start;
		size_t size;
		struct vec2 * points;
	} trail;
};

void body_init(struct body * b, float mass, coord_t x, coord_t y, coord_t vx, coord_t vy);
void body_recalc(struct body * b);
void body_trail(struct body * body, struct vec2 const* point);
void body_trail_reset(struct body * body);
void body_merge(struct body * b, struct body * q);

struct galaxy {
	struct body * bodies;
	size_t bodies_size;
};

#define GALAXY_INIT {NULL, 0}

size_t galaxy_body_add(struct galaxy * galaxy);
void galaxy_body_remove(struct galaxy * galaxy, size_t i);
size_t galaxy_body_get(struct galaxy * galaxy, coord_t x, coord_t y);
void galaxy_integrate(struct galaxy * galaxy, double delta);
void galaxy_bounce(struct galaxy * galaxy, coord_t x0, coord_t x1, coord_t y0, coord_t y1);
void galaxy_render(cairo_t * ctx, struct galaxy * galaxy, unsigned render_flags);

#endif
