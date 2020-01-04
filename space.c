#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include "body.h"

struct ui {
	GLFWwindow * window;
	cairo_surface_t * surface;
	cairo_t * ctx;
	int width, height;
	size_t speed_scale;
	struct galaxy galaxy;
	unsigned simulation_flags;
	unsigned render_flags;
	int mode;
	size_t held;
	double pan_start_x, pan_start_y;

	cairo_matrix_t transform; 
};

enum {
	FLAG_SIMULATE = 0x1,
	FLAG_GRID = 0x2,
	FLAG_BOUNCE = 0x4,
};

enum {
	MODE_IDLE,
	MODE_HOLD,
	MODE_PAN,
};

double rnd() { return (double)rand() / RAND_MAX; }

void scale(struct ui * ui, double f) {
	cairo_matrix_t tfm;
	cairo_matrix_init_scale(&tfm, f, f);
	cairo_matrix_multiply(&ui->transform, &ui->transform, &tfm);
}

void rotate(struct ui * ui, double phi) {
	cairo_matrix_t tfm;
	cairo_matrix_init_rotate(&tfm, phi);
	cairo_matrix_multiply(&ui->transform, &ui->transform, &tfm);
}

void pan(struct ui * ui, double dx, double dy) {
	cairo_matrix_t tfm;
	cairo_matrix_init_translate(&tfm, dx, dy);
	cairo_matrix_multiply(&ui->transform, &ui->transform, &tfm);
}

static void glfw_error(int error, char const* message) {
	fprintf(stderr, "[glfw] %d: %s\n", error, message);
}


static void glfw_framebuffer_size_callback(GLFWwindow * window, int w, int h) {
	struct ui * ui = (struct ui *)glfwGetWindowUserPointer(window);
	ui->width = w;
	ui->height = h;
	cairo_xlib_surface_set_size(ui->surface, w, h);
}

static void glfw_key_callback(GLFWwindow * window, int key, int scancode, int action, int mods) {
	struct ui * ui = (struct ui *)glfwGetWindowUserPointer(window);
	if (action == GLFW_RELEASE) {
		if (key == GLFW_KEY_ESCAPE) {
			if (ui->mode == MODE_IDLE) {
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
			else {
				ui->mode = MODE_IDLE;
			}
		}
		else if (key == GLFW_KEY_SPACE) {
			ui->simulation_flags ^= FLAG_SIMULATE;
		}
		else if (key == GLFW_KEY_B) {
			ui->simulation_flags ^= FLAG_BOUNCE;
		}
		else if (key == GLFW_KEY_P && mods & GLFW_MOD_CONTROL) {
			for (struct body * b = ui->galaxy.bodies; b < ui->galaxy.bodies + ui->galaxy.bodies_size; ++b) {
				body_trail_reset(b);
			}
		}
		else if (key == GLFW_KEY_P) {
			ui->render_flags ^= RF_TRAIL;
		}
		else if (key == GLFW_KEY_R) {
			size_t n = 1;
			if (mods & GLFW_MOD_CONTROL) n *= 10;
			if (mods & GLFW_MOD_ALT) n *= 10;
			for (size_t i=0; i<n; ++i) {
				size_t i = galaxy_body_add(&ui->galaxy);
				double x = rnd() * ui->width;
				double y = rnd() * ui->height;
				cairo_device_to_user(ui->ctx, &x, &y);
				body_init(&ui->galaxy.bodies[i], rnd() * 1000.0, x, y, (rnd() - 0.5) * 2.0, (rnd() - 0.5) * 2.0);
			}
		}
		else if (key == GLFW_KEY_T) {

			if (ui->mode == MODE_HOLD) {
				ui->galaxy.bodies[ui->held].flags ^= BF_TRAIL;
			}
		}
		else if (key == GLFW_KEY_X) {
			if (ui->mode == MODE_HOLD) {
				galaxy_body_remove(&ui->galaxy, ui->held);
				ui->mode = MODE_IDLE;
			}
		}
		else if (key == GLFW_KEY_V) {
			ui->render_flags ^= RF_VELOCITY;
		}
	}
	else if (action == GLFW_PRESS || action == GLFW_RELEASE) {
		if (key == GLFW_KEY_UP) {
			if (!(ui->simulation_flags & FLAG_SIMULATE)) ui->simulation_flags |= FLAG_SIMULATE;
			else ui->speed_scale++;
		}
		else if (key == GLFW_KEY_DOWN) {
			if (ui->speed_scale > 1) ui->speed_scale--;
			else if (ui->speed_scale == 1) ui->simulation_flags &= ~FLAG_SIMULATE;
		}
	}
}

static void glfw_mouse_button_callback(GLFWwindow * window, int button, int action, int mods) {
	struct ui * ui = (struct ui *)glfwGetWindowUserPointer(window);
	if (action == GLFW_RELEASE) {
		if (ui->mode == MODE_IDLE) {
			double x, y;
			glfwGetCursorPos(window, &x, &y);
			cairo_device_to_user(ui->ctx, &x, &y);
			ui->held = galaxy_body_get(&ui->galaxy, x, y);
			if (ui->held == (size_t)-1) {
				ui->held = galaxy_body_add(&ui->galaxy);
				body_init(&ui->galaxy.bodies[ui->held], 1000.0, x, y, 0, 0);
			}
			ui->galaxy.bodies[ui->held].flags &= ~BF_SIMULATE;
			ui->mode = MODE_HOLD;
		}
		else if (ui->mode == MODE_HOLD) {
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				ui->galaxy.bodies[ui->held].flags |= BF_SIMULATE;
			}
			else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
				galaxy_body_remove(&ui->galaxy, ui->held);
			}
			ui->held = (size_t)-1;
			ui->mode = MODE_IDLE;
		}
		else if (ui->mode == MODE_PAN) {
			ui->mode = MODE_IDLE;
		}
	}
	else if (action == GLFW_PRESS) {
		if (ui->mode == MODE_IDLE) {
			if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
				ui->mode = MODE_PAN;
				glfwGetCursorPos(window, &ui->pan_start_x, &ui->pan_start_y);
			}
		}
	}
}

static void glfw_cursor_pos_callback(GLFWwindow * window, double x, double y) {
	struct ui * ui = (struct ui *)glfwGetWindowUserPointer(window);
	if (ui->mode == MODE_HOLD) {
		cairo_device_to_user(ui->ctx, &x, &y);
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			ui->galaxy.bodies[ui->held].v.x = x - ui->galaxy.bodies[ui->held].p.x;
			ui->galaxy.bodies[ui->held].v.y = y - ui->galaxy.bodies[ui->held].p.y;
		}
		else {
			ui->galaxy.bodies[ui->held].p.x = x;
			ui->galaxy.bodies[ui->held].p.y = y;
			body_trail_reset(&ui->galaxy.bodies[ui->held]);
		}
	}
	else if (ui->mode == MODE_PAN) {
		pan(ui, x - ui->pan_start_x, y - ui->pan_start_y);
		ui->pan_start_x = x; ui->pan_start_y = y;
	}
}

static void glfw_scroll_callback(GLFWwindow * window, double dx, double dy) {
	struct ui * ui = (struct ui *)glfwGetWindowUserPointer(window);
	if (ui->mode == MODE_HOLD) {
		ui->galaxy.bodies[ui->held].mass *= 1.0 + (dy / 10.0);
		body_recalc(&ui->galaxy.bodies[ui->held]);
	}
	else if (ui->mode == MODE_IDLE) {
		if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
			rotate(ui, dy / 20.0);
		}
		else {
			scale(ui, 1.0 + (dy / 10.0));
		}
	}
}

int ui_init(struct ui * ui, int width, int height) {
	ui->mode = MODE_IDLE;
	ui->held = 0;
	ui->galaxy = (struct galaxy)GALAXY_INIT;
	ui->simulation_flags = 0;
	ui->speed_scale = 1;
	ui->render_flags = RF_PARTICLE | RF_TRAIL;
	ui->transform = (cairo_matrix_t){.xx=1, .yy=1, .x0=-width/2.0, .y0=-height/2.0};

	glfwSetErrorCallback(&glfw_error);
	if (!glfwInit()) {
		fprintf(stderr, "[glfw] Failed to initialize.\n");
		return 1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	ui->window = glfwCreateWindow(width, height, "Space", NULL, NULL);
	if (ui->window == NULL) {
		fprintf(stderr, "[glfw] Failed to create window.\n");
		return 1;
	}
	glfwSetWindowUserPointer(ui->window, ui);

	glfwSetFramebufferSizeCallback(ui->window, &glfw_framebuffer_size_callback);
	glfwSetKeyCallback(ui->window, &glfw_key_callback);
	glfwSetMouseButtonCallback(ui->window, &glfw_mouse_button_callback);
	glfwSetCursorPosCallback(ui->window, &glfw_cursor_pos_callback);
	glfwSetScrollCallback(ui->window, &glfw_scroll_callback);

	int fb_w, fb_h;
	glfwGetFramebufferSize(ui->window, &fb_w, &fb_h);
	Display * x11_display = glfwGetX11Display();
	Window x11_window = glfwGetX11Window(ui->window);
	Visual * vis = DefaultVisual(x11_display, DefaultScreen(x11_display));

	ui->surface = cairo_xlib_surface_create(x11_display, x11_window, vis, fb_w, fb_h);
	cairo_status_t status = cairo_surface_status(ui->surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "[cairo-gl] Failed to create surface: %s\n", cairo_status_to_string(status));
		return 1;
	}

	ui->ctx = cairo_create(ui->surface);

	glfw_framebuffer_size_callback(ui->window, fb_w, fb_h);

	return 0;
}

int main(int argc, char * argv[]) {
	struct ui ui;
	if (ui_init(&ui, 800, 600)) {
		fprintf(stderr, "[ui] UI initialization failed.\n");
		return 1;
	}

	double t_last_frame = glfwGetTime();

	while (!glfwWindowShouldClose(ui.window)) {
		double t_frame_start = glfwGetTime();
		double delta = t_frame_start - t_last_frame;

		if (ui.simulation_flags & FLAG_SIMULATE) {
			for (size_t i=0; i<ui.speed_scale; ++i) {
				galaxy_integrate(&ui.galaxy, delta);
				if (ui.simulation_flags & FLAG_BOUNCE) {
					galaxy_bounce(&ui.galaxy, 0, 0, ui.width, ui.height);
				}
			}
		}

		//have to group, else the x server does weird queueing
		cairo_push_group(ui.ctx);

		//clear
		cairo_save(ui.ctx);
		cairo_set_source_rgba(ui.ctx, 0.0, 0.0, 0.0, 1.0);
		cairo_paint(ui.ctx);
		cairo_restore(ui.ctx);

		// galaxy
		cairo_set_matrix(ui.ctx, &ui.transform);
		double center_x = ui.width / 2.0, center_y = ui.height / 2.0;
		cairo_device_to_user_distance(ui.ctx, &center_x, &center_y);
		cairo_translate(ui.ctx, center_x, center_y);

		galaxy_render(ui.ctx, &ui.galaxy, ui.render_flags);

		// UI
		cairo_identity_matrix(ui.ctx);
		if (!(ui.simulation_flags & FLAG_SIMULATE)) {
			cairo_set_source_rgba(ui.ctx, 1.0, 1.0, 1.0, 0.8);
			cairo_rectangle(ui.ctx, ui.width - 48, ui.height - 48, 12, 32);
			cairo_fill(ui.ctx);
			cairo_rectangle(ui.ctx, ui.width - 48 + 18, ui.height - 48, 12, 32);
			cairo_fill(ui.ctx);
		}
		else {
			cairo_set_source_rgba(ui.ctx, 1.0, 1.0, 1.0, 0.8);
			cairo_move_to(ui.ctx, ui.width - 48, ui.height - 48);
			cairo_rel_line_to(ui.ctx, 32, 16);
			cairo_rel_line_to(ui.ctx, -32, 16);
			cairo_close_path(ui.ctx);
			cairo_fill(ui.ctx);
			if (ui.speed_scale > 1) {
				cairo_set_source_rgba(ui.ctx, 0.0, 0.0, 0.0, 0.8);
				cairo_set_font_size(ui.ctx, 16);
				cairo_move_to(ui.ctx, ui.width - 44, ui.height - 26);
				char scale_buf[16];
				snprintf(scale_buf, 16, "%zu", ui.speed_scale);
				cairo_show_text(ui.ctx, scale_buf);
			}
		}

		t_last_frame = t_frame_start;
		//double t_frame_end = glfwGetTime();
		//fprintf(stderr, "frame time %lf s | rate %lf Hz\n", t_frame_end - t_frame_start, 1.0 / delta);

		cairo_pop_group_to_source(ui.ctx);
		cairo_paint(ui.ctx);
		cairo_surface_flush(ui.surface);

		glfwPollEvents();
	}

	cairo_destroy(ui.ctx);
	cairo_surface_destroy(ui.surface);
	glfwTerminate();
}
