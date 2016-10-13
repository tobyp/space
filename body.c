#include <math.h>
#include <cairo/cairo.h>
#include <stdlib.h>

#include "body.h"

#define E 2.7182818

#define GRAV 1.0

void body_init(struct body * b, float mass, float x, float y) {
	b->p = (struct vec2){x, y};
	b->prior_p = (struct vec2){x, y};
	b->v = (struct vec2){0, 0};
	b->flags = 0;
	b->mass = mass;
	body_recalc(b);
}

void body_recalc(struct body * b) {
	b->r = 1.0;
	b->g = 1.0 / (1.0 + b->mass/5000.0);
	b->b = 1.0 / (1.0 + b->mass/500.0);
	b->radius = 10 * log(E + b->mass/5000.0);
}

void body_trail(struct body * body, struct vec2 const* point) {
	body->trail.points[body->trail.cursor++] = *point;
	body->trail.cursor %= body->trail.capacity;
	if (body->trail.size < body->trail.capacity) {
		body->trail.size++;
	}
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
	body_recalc(b1);
}

void galaxy_init(struct galaxy * galaxy, size_t n, double w, double h) {
	galaxy->bodies = (struct body *)realloc(galaxy->bodies, n * sizeof(struct body));
	if (!galaxy->bodies) {
		galaxy->bodies_size = 0;
		return;
	}
	galaxy->bodies_size = n;
	for (size_t i=0; i<n; ++i) {
		struct body * b = &galaxy->bodies[i];
		b->mass = ((double)rand() / RAND_MAX) * 500.0;
		b->p.x = b->prior_p.x = ((double)rand() / RAND_MAX) * w;
		b->p.y = b->prior_p.y = ((double)rand() / RAND_MAX) * h;
		b->v.x = (((double)rand() / RAND_MAX) - 0.5) * 0.1;
		b->v.y = (((double)rand() / RAND_MAX) - 0.5) * 0.1;
		b->flags = BF_VALID;

		body_recalc(b);
	}
}


struct body * galaxy_body_add(struct galaxy * galaxy) {
	galaxy->bodies = (struct body *)realloc(galaxy->bodies, (galaxy->bodies_size + 1) * sizeof(struct body));
	return &galaxy->bodies[galaxy->bodies_size++];
}

void galaxy_integrate(struct galaxy * galaxy, double delta) {
	for (size_t i=0; i<galaxy->bodies_size; ++i) {
		struct body * b1 = &galaxy->bodies[i];
		if (!(b1->flags & BF_VALID)) continue;

		double ax = 0.0, ay = 0.0;
		for (size_t j=0; j<galaxy->bodies_size; ++j) {
			if (i == j) continue;
			struct body * b2 = &galaxy->bodies[j];
			if (!(b2->flags & BF_VALID)) continue;

			double dx = b2->p.x - b1->p.x;
			double dy = b2->p.y - b1->p.y;
			double dsquared = dx * dx + dy * dy;
			double d = sqrt(dx * dx + dy * dy);

			if (d < b1->radius/1.75 + b2->radius/1.75) {
				body_merge(b1, b2);
				b2->flags &= ~BF_VALID;
			}

			double a = GRAV * b2->mass / dsquared;
			ax += a * dx / d;
			ay += a * dy / d;
		}

		b1->v.x += ax * delta;
		b1->v.y += ay * delta;
		b1->p.x += b1->v.x * delta;
		b1->p.y += b1->v.y * delta;
	}
}

void galaxy_render(cairo_t * ctx, struct galaxy * galaxy) {
	for (size_t i=0; i<galaxy->bodies_size; ++i) {
		struct body * b = &galaxy->bodies[i];
		if (b->flags & BF_VALID) {
			cairo_set_source_rgba(ctx, b->r, b->g, b->b, 0.95);
			cairo_arc(ctx, b->p.x, b->p.y, b->radius, 0.0, 2.0 * PI);
			cairo_fill(ctx);
		}
	}
}

void galaxy_render_trails(cairo_t * ctx, struct galaxy * galaxy) {
	for (size_t i=0; i<galaxy->bodies_size; ++i) {
		struct body * b = &galaxy->bodies[i];
		cairo_set_source_rgba(ctx, b->r, b->g, b->b, 0.85);
		cairo_move_to(ctx, b->prior_p.x, b->prior_p.y);
		cairo_line_to(ctx, b->p.x, b->p.y);
		cairo_stroke(ctx);
		b->prior_p = b->p;
	}
}
