#define TEST_GLX

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#if defined(TEST_GLX)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#include <GL/glx.h>
#elif defined(TEST_EGL)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#define GLFW_EXPOSE_NATIVE_EGL
#include <EGL/egl.h>
#elif defined(TEST_WGL)
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <windows.h>
#else
#error "No backend defined"
#endif

#include <GLFW/glfw3native.h>

#include <cairo/cairo.h>
#include <cairo/cairo-gl.h>

#include "body.h"

#define WIDTH 800.0
#define HEIGHT 600.0

int width = WIDTH, height = HEIGHT;
cairo_surface_t * surface;
cairo_t * ctx;
size_t speed_scale = 1;
struct galaxy galaxy = GALAXY_INIT;

enum {
	FLAG_SIMULATE = 0x1,
	FLAG_GRID = 0x2,
	FLAG_BOUNCE = 0x4,
};

unsigned flags = 0;
unsigned render_flags = RF_PARTICLE | RF_TRAIL;

enum {
	MODE_IDLE,
	MODE_HOLD,
	MODE_PAN,
};

int mode = MODE_IDLE;
size_t held = 0;

cairo_matrix_t transform = {.xx=1, .yy=1, .x0=-WIDTH/2.0, .y0=-HEIGHT/2.0}; 

double rnd() { return (double)rand() / RAND_MAX; }

void scale(double f) {
	cairo_matrix_t tfm;
	cairo_matrix_init_scale(&tfm, f, f);
	cairo_matrix_multiply(&transform, &transform, &tfm);
}

void rotate(double phi) {
	cairo_matrix_t tfm;
	cairo_matrix_init_rotate(&tfm, phi);
	cairo_matrix_multiply(&transform, &transform, &tfm);
}

void pan(double dx, double dy) {
	cairo_matrix_t tfm;
	cairo_matrix_init_translate(&tfm, dx, dy);
	cairo_matrix_multiply(&transform, &transform, &tfm);
}

void screen_to_view(double * x, double * y) {

}

static void gl_debug_message(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* message, void const* userParam);
static void glfw_error(int error, char const* message);

void glfw_framebuffer_size(GLFWwindow * window, int w, int h) {
	width = w;
	height = h;
	cairo_gl_surface_set_size(surface, w, h);
}

void glfw_key(GLFWwindow * window, int key, int scancode, int action, int mods) {
	if (action == GLFW_RELEASE) {
		if (key == GLFW_KEY_ESCAPE) {
			if (mode == MODE_IDLE) {
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
			else {
				mode = MODE_IDLE;
			}
		}
		else if (key == GLFW_KEY_SPACE) {
			flags ^= FLAG_SIMULATE;
		}
		else if (key == GLFW_KEY_B) {
			flags ^= FLAG_BOUNCE;
		}
		else if (key == GLFW_KEY_P && mods & GLFW_MOD_CONTROL) {
			for (struct body * b = galaxy.bodies; b < galaxy.bodies + galaxy.bodies_size; ++b) {
				body_trail_reset(b);
			}
		}
		else if (key == GLFW_KEY_P) {
			render_flags ^= RF_TRAIL;
		}
		else if (key == GLFW_KEY_R) {
			size_t n = 1;
			if (mods & GLFW_MOD_CONTROL) n *= 10;
			if (mods & GLFW_MOD_ALT) n *= 10;
			for (size_t i=0; i<n; ++i) {
				size_t i = galaxy_body_add(&galaxy);
				double x = rnd() * width;
				double y = rnd() * height;
				cairo_device_to_user(ctx, &x, &y);
				body_init(&galaxy.bodies[i], rnd() * 1000.0, x, y, (rnd() - 0.5) * 2.0, (rnd() - 0.5) * 2.0);
			}
		}
		else if (key == GLFW_KEY_T) {
			if (mode == MODE_HOLD) {
				galaxy.bodies[held].flags ^= BF_TRAIL;
			}
		}
		else if (key == GLFW_KEY_X) {
			if (mode == MODE_HOLD) {
				galaxy_body_remove(&galaxy, held);
				mode = MODE_IDLE;
			}
		}
		else if (key == GLFW_KEY_V) {
			render_flags ^= RF_VELOCITY;
		}
	}
	else if (action == GLFW_PRESS || action == GLFW_RELEASE) {
		if (key == GLFW_KEY_UP) {
			if (!(flags & FLAG_SIMULATE)) flags |= FLAG_SIMULATE;
			else speed_scale++;
		}
		else if (key == GLFW_KEY_DOWN) {
			if (speed_scale > 1) speed_scale--;
			else if (speed_scale == 1) flags &= ~FLAG_SIMULATE;
		}
	}
}

double pan_start_x, pan_start_y;

void glfw_mouse_button(GLFWwindow * window, int button, int action, int mods) {
	if (action == GLFW_RELEASE) {
		if (mode == MODE_IDLE) {
			double x, y;
			glfwGetCursorPos(window, &x, &y);
			cairo_device_to_user(ctx, &x, &y);
			held = galaxy_body_get(&galaxy, x, y);
			if (held == (size_t)-1) {
				held = galaxy_body_add(&galaxy);
				body_init(&galaxy.bodies[held], 1000.0, x, y, 0, 0);
			}
			galaxy.bodies[held].flags &= ~BF_SIMULATE;
			mode = MODE_HOLD;
		}
		else if (mode == MODE_HOLD) {
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				galaxy.bodies[held].flags |= BF_SIMULATE;
			}
			else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
				galaxy_body_remove(&galaxy, held);
			}
			held = (size_t)-1;
			mode = MODE_IDLE;
		}
		else if (mode == MODE_PAN) {
			mode = MODE_IDLE;
		}
	}
	else if (action == GLFW_PRESS) {
		if (mode == MODE_IDLE) {
			if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
				mode = MODE_PAN;
				glfwGetCursorPos(window, &pan_start_x, &pan_start_y);
			}
		}
	}
}

void glfw_cursor_pos(GLFWwindow * window, double x, double y) {
	if (mode == MODE_HOLD) {
		cairo_device_to_user(ctx, &x, &y);
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			galaxy.bodies[held].v.x = x - galaxy.bodies[held].p.x;
			galaxy.bodies[held].v.y = y - galaxy.bodies[held].p.y;
		}
		else {
			galaxy.bodies[held].p.x = x;
			galaxy.bodies[held].p.y = y;
			body_trail_reset(&galaxy.bodies[held]);
		}
	}
	else if (mode == MODE_PAN) {
		pan(x - pan_start_x, y - pan_start_y);
		pan_start_x = x; pan_start_y = y;
	}
}

void glfw_scroll(GLFWwindow * window, double dx, double dy) {
	if (mode == MODE_HOLD) {
		galaxy.bodies[held].mass *= 1.0 + (dy / 10.0);
		body_recalc(&galaxy.bodies[held]);
	}
	else if (mode == MODE_IDLE) {
		if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
			rotate(dy / 20.0);
		}
		else {
			scale(1.0 + (dy / 10.0));
		}
	}
}

int main(int argc, char * argv[]) {
	glfwSetErrorCallback(&glfw_error);
	if (!glfwInit()) {
		fprintf(stderr, "[glfw] Failed to initialize.\n");
		return 1;
	}

	/* glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE); */
	GLFWwindow * window = glfwCreateWindow(width, height, "Test", NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "[glfw] Failed to create window.\n");
		return 1;
	}
	glfwMakeContextCurrent(window);

	fprintf(stderr, "[glfw] OpenGL context version %d.%d\n", glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR), glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR));

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "[glew] Failed to initialize\n");
		return -1;
	}

	glDebugMessageCallback(&gl_debug_message, 0);
	glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_OTHER_ARB, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
	glfwSwapInterval(1);
	glfwSetFramebufferSizeCallback(window, &glfw_framebuffer_size);
	glfwSetKeyCallback(window, &glfw_key);
	glfwSetMouseButtonCallback(window, &glfw_mouse_button);
	glfwSetCursorPosCallback(window, &glfw_cursor_pos);
	glfwSetScrollCallback(window, &glfw_scroll);

#if defined(TEST_GLX)
	Display * x11_display = glfwGetX11Display();
	GLXContext glx_context = glfwGetGLXContext(window);
	Window x11_window = glfwGetX11Window(window);

	cairo_device_t * device = cairo_glx_device_create(x11_display, glx_context);
	surface = cairo_gl_surface_create_for_window(device, x11_window, width, height);
	cairo_device_destroy(device);
#elif defined(TEST_EGL)
	EGLDisplay egl_display = glfwGetEGLDisplay();
	EGLContext egl_context = glfwGetEGLContext(window);
	EGLSurface egl_surface = glfwGetEGLSurface(window);

	cairo_device_t * device = cairo_egl_device_create(egl_display, egl_context);
	surface = cairo_gl_surface_create_for_egl(device, egl_surface, width, height);
	cairo_device_destroy(device);
#elif defined(TEST_WGL)
	HWND wgl_window = glfwGetWin32Window(window);
	HDC wgl_dc = GetDC(wgl_window);
	HGLRC wgl_context = glfwGetWGLContext(window);

	cairo_device_t * device = cairo_wgl_device_create(wgl_context);
	surface = cairo_gl_surface_create_for_dc(device, wgl_dc, width, height);
	cairo_device_destroy(device);
#endif

	cairo_status_t status = cairo_surface_status(surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "[cairo-gl] Failed to create surface: %s\n", cairo_status_to_string(status));
		return 1;
	}

	ctx = cairo_create(surface);

	double t_last_frame = glfwGetTime();

	while (!glfwWindowShouldClose(window)) {
		double t_frame_start = glfwGetTime();
		double delta = t_frame_start - t_last_frame;

		if (flags & FLAG_SIMULATE) {
			for (size_t i=0; i<speed_scale; ++i) {
				galaxy_integrate(&galaxy, delta);
				if (flags & FLAG_BOUNCE) {
					galaxy_bounce(&galaxy, 0, 0, width, height);
				}
			}
		}

		cairo_set_matrix(ctx, &transform);
		double center_x = width / 2.0, center_y = height / 2.0;
		cairo_device_to_user_distance(ctx, &center_x, &center_y);
		cairo_translate(ctx, center_x, center_y);
		cairo_set_source_rgba(ctx, 0.0, 0.0, 0.0, 1.0);
		cairo_paint(ctx);
		galaxy_render(ctx, &galaxy, render_flags);

		cairo_save(ctx);
		cairo_identity_matrix(ctx);
		if (!(flags & FLAG_SIMULATE)) {
			cairo_set_source_rgba(ctx, 1.0, 1.0, 1.0, 0.8);
			cairo_rectangle(ctx, width - 48, height - 48, 12, 32);
			cairo_fill(ctx);
			cairo_rectangle(ctx, width - 48 + 18, height - 48, 12, 32);
			cairo_fill(ctx);
		}
		else {
			cairo_set_source_rgba(ctx, 1.0, 1.0, 1.0, 0.8);
			cairo_move_to(ctx, width - 48, height - 48);
			cairo_rel_line_to(ctx, 32, 16);
			cairo_rel_line_to(ctx, -32, 16);
			cairo_close_path(ctx);
			cairo_fill(ctx);
			if (speed_scale > 1) {
				cairo_set_source_rgba(ctx, 0.0, 0.0, 0.0, 0.8);
				cairo_set_font_size(ctx, 16);
				cairo_move_to(ctx, width - 44, height - 26);
				char scale_buf[16];
				snprintf(scale_buf, 16, "%zu", speed_scale);
				cairo_show_text(ctx, scale_buf);
			}
		}
		cairo_restore(ctx);

		t_last_frame = t_frame_start;
		double t_frame_end = glfwGetTime();
		//fprintf(stderr, "frame time %lf s | rate %lf Hz\n", t_frame_end - t_frame_start, 1.0 / delta);

		cairo_gl_surface_swapbuffers(surface);
		//glfwSwapBuffers(window);
		glfwPollEvents();
	}

	cairo_destroy(ctx);
	cairo_surface_destroy(surface);
	glfwTerminate();
}

static void gl_debug_message(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* message, void const* userParam) {
	char const* source_string = source == GL_DEBUG_SOURCE_API ? "api" :
		source == GL_DEBUG_SOURCE_WINDOW_SYSTEM ? "window system" :
		source == GL_DEBUG_SOURCE_SHADER_COMPILER ? "shader compiler" :
		source == GL_DEBUG_SOURCE_THIRD_PARTY ? "third party" :
		source == GL_DEBUG_SOURCE_APPLICATION ? "application" :
		"other"; //GL_DEBUG_SOURCE_OTHER
	char const* type_string = type == GL_DEBUG_TYPE_ERROR ? "error" :
		type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR ? "deprecated behavior" :
		type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR ? "undefined behavior" :
		type == GL_DEBUG_TYPE_PORTABILITY ? "portability warning" :
		type == GL_DEBUG_TYPE_PERFORMANCE ? "performance warning" :
		type == GL_DEBUG_TYPE_MARKER ? "marker" :
		type == GL_DEBUG_TYPE_PUSH_GROUP ? "push" :
		type == GL_DEBUG_TYPE_POP_GROUP ? "pop" :
		"other"; //GL_DEBUG_TYPE_OTHER
	char const* severity_string = severity == GL_DEBUG_SEVERITY_LOW ? "low" :
		severity == GL_DEBUG_SEVERITY_MEDIUM ? "medium" :
		severity == GL_DEBUG_SEVERITY_HIGH ? "high" :
		severity == GL_DEBUG_SEVERITY_NOTIFICATION ? "notification" :
		"???";
	fprintf(stderr, "[gl] %s: %s (%s): %s\n", type_string, source_string, severity_string, message);
}

static void glfw_error(int error, char const* message) {
	fprintf(stderr, "[glfw] %d: %s\n", error, message);
}
