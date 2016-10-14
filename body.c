#include <tgmath.h>
#include <cairo/cairo.h>
#include <stdlib.h>

#include "body.h"

#define E 2.7182818

#define GRAV 1.0

#define TRAIL_SIZE 1024

void body_init(struct body * b, float mass, float x, float y, float vx, float vy) {
	b->p = (struct vec2){x, y};
	b->v = (struct vec2){vx, vy};
	b->flags |= BF_SIMULATE | BF_TRAIL;
	b->mass = mass;
	b->trail.points = malloc(TRAIL_SIZE * sizeof(struct vec2));
	b->trail.points[0] = b->p;
	b->trail.size = TRAIL_SIZE;
	b->trail.start = 0;
	b->trail.end = 1;
	body_recalc(b);
}

void body_recalc(struct body * b) {
	b->r = 1.0;
	b->g = 1.0 / (1.0 + b->mass/100000.0);
	b->b = 1.0 / (1.0 + b->mass/10000.0);
	b->radius = 1.0 + log(E + b->mass/250.0);
}

void body_trail(struct body * body, struct vec2 const* point) {
	body->trail.start = (body->trail.start + 1) % body->trail.size;
	body->trail.points[body->trail.start] = *point;
	if (body->trail.start == body->trail.end) {
		body->trail.end = (body->trail.end + 1) % body->trail.size;
	}
}

void body_trail_reset(struct body * body) {
	body->trail.start = 0;
	body->trail.end = 1;
	body->trail.points[0] = body->p;
}

void body_merge(struct body * b1, struct body * b2) {
	double sum_mass = b1->mass + b2->mass;
	double vx = (b1->v.x*b1->mass + b2->v.x*b2->mass) / sum_mass;
	double vy = (b1->v.y*b1->mass + b2->v.y*b2->mass) / sum_mass;
	double px = (b1->p.x*b1->mass + b2->p.x*b2->mass) / sum_mass;
	double py = (b1->p.y*b1->mass + b2->p.y*b2->mass) / sum_mass;
	b1->mass = sum_mass;
	b1->p.x = px;
	b1->p.y = py;
	b1->v.x = vx;
	b1->v.y = vy;
	b2->flags = 0;
	body_recalc(b1);
}

struct body * galaxy_body_add(struct galaxy * galaxy) {
	for (struct body * b = galaxy->bodies; b < galaxy->bodies + galaxy->bodies_size; ++b) {
		if (!(b->flags & BF_ALLOCATED)) {
			b->flags = BF_ALLOCATED;
			return b;
		}
	}
	size_t new_size = galaxy->bodies_size + 1;
	galaxy->bodies = (struct body *)realloc(galaxy->bodies, new_size * sizeof(struct body));
	galaxy->bodies[galaxy->bodies_size].flags = BF_ALLOCATED;
	return &galaxy->bodies[galaxy->bodies_size++];
}

void galaxy_body_remove(struct galaxy * galaxy, struct body * b) {
	b->flags = 0;
}

void galaxy_integrate(struct galaxy * galaxy, double delta) {
	for (size_t i=0; i<galaxy->bodies_size; ++i) {
		struct body * b1 = &galaxy->bodies[i];
		if (!(b1->flags & BF_SIMULATE)) continue;

		double ax = 0.0, ay = 0.0;
		for (size_t j=0; j<galaxy->bodies_size; ++j) {
			if (i == j) continue;
			struct body * b2 = &galaxy->bodies[j];
			if (!(b2->flags & BF_SIMULATE)) continue;

			double dx = b2->p.x - b1->p.x;
			double dy = b2->p.y - b1->p.y;
			double dsquared = dx * dx + dy * dy;
			double d = sqrt(dx * dx + dy * dy);

			if (d < b1->radius/1.75 + b2->radius/1.75) {
				body_merge(b1, b2);
				b2->flags &= ~BF_SIMULATE;
			}

			double a = GRAV * b2->mass / dsquared;
			ax += a * dx / d;
			ay += a * dy / d;
		}

		b1->v.x += ax * delta;
		b1->v.y += ay * delta;
		b1->p.x += b1->v.x * delta;
		b1->p.y += b1->v.y * delta;

		body_trail(b1, &b1->p);
	}
}

void galaxy_render(cairo_t * ctx, struct galaxy * galaxy) {
	for (size_t i=0; i<galaxy->bodies_size; ++i) {
		struct body * b = &galaxy->bodies[i];
		if (b->flags & BF_SIMULATE) {
			cairo_set_source_rgba(ctx, b->r, b->g, b->b, 0.95);
			cairo_arc(ctx, b->p.x, b->p.y, b->radius, 0.0, 2.0 * PI);
			cairo_fill(ctx);
		}
	}
}

#include <stdio.h>

void galaxy_render_trails(cairo_t * ctx, struct galaxy * galaxy) {
	for (size_t i=0; i<galaxy->bodies_size; ++i) {
		struct body * b = &galaxy->bodies[i];
		if (b->flags & BF_SIMULATE) {
			if (b->flags & BF_TRAIL) {
				cairo_set_source_rgba(ctx, b->r, b->g, b->b, 0.85);
				cairo_move_to(ctx, b->trail.points[b->trail.start].x, b->trail.points[b->trail.start].y);
				for (size_t i=(b->trail.start + 1) % b->trail.size; (b->trail.start >= b->trail.end) ? (i >= b->trail.start || i < b->trail.end) : (i >= b->trail.start && i < b->trail.end); i = (i + 1) % b->trail.size) {
					cairo_line_to(ctx, b->trail.points[i].x, b->trail.points[i].y);
					cairo_stroke(ctx);
				}
			}
		}
	}
}

struct body * galaxy_body_get(struct galaxy * galaxy, float x, float y) {
	for (size_t i=0; i<galaxy->bodies_size; ++i) {
		struct body * b = &galaxy->bodies[i];
		if (b->flags & BF_ALLOCATED) {
			float dx = fabs(b->p.x - x);
			if (dx > b->radius) continue;
			float dy = fabs(b->p.y - y);
			if (dy > b->radius) continue;
			if (sqrt(dx * dx + dy * dy) <= b->radius) return b;
		}
	}
	return NULL;
}
