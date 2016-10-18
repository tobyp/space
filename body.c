#include <tgmath.h>
#include <cairo/cairo.h>
#include <stdlib.h>

#include "body.h"

#define E 2.7182818

#define GRAV 1.0

#define TRAIL_SIZE 1024

void body_init(struct body * b, float mass, coord_t x, coord_t y, coord_t vx, coord_t vy) {
	b->p = (struct vec2){x, y};
	b->v = (struct vec2){vx, vy};
	b->flags |= BF_SIMULATE | BF_TRAIL | BF_EXISTS;
	b->mass = mass;
	b->trail.points = malloc(TRAIL_SIZE * sizeof(struct vec2));
	if (b->trail.points == NULL) abort();
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

#include <stdio.h>

void body_trail(struct body * body, struct vec2 const* point) {
	body->trail.points[body->trail.end] = *point;
	body->trail.end = (body->trail.end + 1) % body->trail.size;
	if (body->trail.start == body->trail.end) {
		body->trail.start = (body->trail.start + 1) % body->trail.size;
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
	b2->flags &= BF_ALLOCATED | BF_TRAIL;
	body_trail(b2, &b1->p);
	body_recalc(b1);
}

size_t galaxy_body_add(struct galaxy * galaxy) {
	for (size_t i=0; i<galaxy->bodies_size; ++i) {
		struct body * b = &galaxy->bodies[i];
		if (!(b->flags & BF_ALLOCATED)) {
			b->flags = BF_ALLOCATED;
			return i;
		}
	}
	size_t new_size = galaxy->bodies_size + 1;
	galaxy->bodies = (struct body *)realloc(galaxy->bodies, new_size * sizeof(struct body));
	if (galaxy->bodies == NULL) abort();
	galaxy->bodies[galaxy->bodies_size].flags = BF_ALLOCATED;
	return galaxy->bodies_size++;
}

void galaxy_body_remove(struct galaxy * galaxy, size_t i) {
	galaxy->bodies[i].flags = BF_ALLOCATED;
}

size_t galaxy_body_get(struct galaxy * galaxy, coord_t x, coord_t y) {
	for (size_t i=0; i<galaxy->bodies_size; ++i) {
		struct body * b = &galaxy->bodies[i];
		if (b->flags & BF_ALLOCATED) {
			coord_t dx = fabs(b->p.x - x);
			if (dx > b->radius) continue;
			coord_t dy = fabs(b->p.y - y);
			if (dy > b->radius) continue;
			if (sqrt(dx * dx + dy * dy) <= b->radius) return i;
		}
	}
	return (size_t)-1;
}

void galaxy_integrate(struct galaxy * galaxy, double delta) {
	for (size_t i=0; i<galaxy->bodies_size; ++i) {
		struct body * b1 = &galaxy->bodies[i];
		if (!(b1->flags & BF_SIMULATE)) continue;

		coord_t ax = 0.0, ay = 0.0;
		for (size_t j=0; j<galaxy->bodies_size; ++j) {
			if (i == j) continue;
			struct body * b2 = &galaxy->bodies[j];
			if (!(b2->flags & BF_SIMULATE)) continue;

			coord_t dx = b2->p.x - b1->p.x;
			coord_t dy = b2->p.y - b1->p.y;
			coord_t dsquared = dx * dx + dy * dy;
			coord_t d = sqrt(dx * dx + dy * dy);

			if (d < b1->radius/1.75 + b2->radius/1.75) {
				body_merge(b1, b2);
				b2->flags &= ~BF_SIMULATE;
			}

			coord_t a = GRAV * b2->mass / dsquared;
			ax += a * dx / d;
			ay += a * dy / d;
		}

		b1->v.x += ax * delta;
		b1->v.y += ay * delta;
		b1->p.x += b1->v.x * delta;
		b1->p.y += b1->v.y * delta;

		if (b1->flags & BF_TRAIL) {
			struct vec2 * old_end = &b1->trail.points[(b1->trail.end - 1) % b1->trail.size];
			if (fabs(old_end->x - b1->p.x) > 2 || fabs(old_end->y - b1->p.y) > 2) {
				body_trail(b1, &b1->p);
			}
		}
	}
}

void galaxy_render(cairo_t * ctx, struct galaxy * galaxy, unsigned render_flags) {
	for (struct body * b = galaxy->bodies; b < galaxy->bodies + galaxy->bodies_size; ++b) {
		if (!(b->flags & BF_ALLOCATED)) continue;

		if (b->flags & BF_EXISTS) {
			if (render_flags & RF_PARTICLE) {
				cairo_set_source_rgba(ctx, b->r, b->g, b->b, 0.9);
				cairo_arc(ctx, b->p.x, b->p.y, b->radius, 0.0, 2.0 * PI);
				cairo_fill(ctx);
			}
			if (render_flags & RF_VELOCITY) {
				cairo_set_source_rgba(ctx, b->r, b->g, b->b, 1.0);
				cairo_move_to(ctx, b->p.x, b->p.y);
				cairo_rel_line_to(ctx, b->v.x, b->v.y);
				cairo_stroke(ctx);
			}
		}
		if (render_flags & RF_TRAIL) {
			cairo_new_path(ctx);
			cairo_set_source_rgba(ctx, b->r, b->g, b->b, 0.8);
			for (size_t i=b->trail.start; (b->trail.start < b->trail.end) ? (i >= b->trail.start && i < b->trail.end) : (i >= b->trail.start || i < b->trail.end); i = (i + 1) % b->trail.size) {
				cairo_line_to(ctx, b->trail.points[i].x, b->trail.points[i].y);
			}
			cairo_stroke(ctx);
		}
	}
}

coord_t bounce(coord_t x, coord_t w) {
	coord_t fits = floor(x/w);
	coord_t rem = x - (fits * w);
	coord_t new_x = rem;
	int ifits = (int)fmod(fits, 2);
	if (ifits) {
		return w - new_x;
	}
	else {
		return new_x;
	}
}

coord_t dbounce(coord_t x, coord_t w) {
	coord_t fits = floor(x/w);
	int ifits = (int)fmod(fits, 2);
	if (ifits) {
		return -1.0;
	}
	else {
		return 1.0;
	}
}

void galaxy_bounce(struct galaxy * galaxy, coord_t x0, coord_t y0, coord_t x1, coord_t y1) {
	coord_t w = x1 - x0;
	coord_t h = y1 - y0;
	for (struct body * b = galaxy->bodies; b < galaxy->bodies + galaxy->bodies_size; ++b) {
		b->p.x = x0 + bounce(b->p.x - x0, w);
		b->p.y = y0 + bounce(b->p.y - y0, h);
	}
}
