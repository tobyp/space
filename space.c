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

/*
void glfw_window_pos(GLFWwindow *, int, int);
void glfw_window_size(GLFWwindow *, int, int);
void glfw_window_close(GLFWwindow *);
void glfw_window_refresh(GLFWwindow *);
void glfw_window_focus(GLFWwindow *, int);
void glfw_window_iconify(GLFWwindow *, int);
void glfw_mouse_button(GLFWwindow *, int, int, int);
void glfw_cursor_pos(GLFWwindow *, double, double);
void glfw_cursor_enter(GLFWwindow *, int);
void glfw_scroll(GLFWwindow *, double, double);
void glfw_key(GLFWwindow *, int, int, int, int);
void glfw_char(GLFWwindow *, unsigned int);
void glfw_charmods(GLFWwindow *, unsigned int, int);
void glfw_dropfun(GLFWwindow *, int, const char **);
void glfw_monitor(GLFWmonitor *, int);
void glfw_joystick(int, int);*/

int width = 800, height = 600;
cairo_surface_t * surface;
cairo_surface_t * surface_paths;
cairo_t * ctx;
cairo_t * ctx_paths;
size_t speed_scale = 1;
struct galaxy galaxy = GALAXY_INIT;

enum {
	FLAG_SIMULATE = 1,
	FLAG_PATHS = 2,
	FLAG_GRID = 3,
};
int flags;

enum {
	MODE_IDLE,
	MODE_HOLD,
};

int mode = MODE_IDLE;
struct body * held_body = NULL;

double rnd() { return (double)rand() / RAND_MAX; }

static void gl_debug_message(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* message, void const* userParam);
static void glfw_error(int error, char const* message);

void glfw_framebuffer_size(GLFWwindow * window, int w, int h) {
	width = w;
	height = h;
	cairo_gl_surface_set_size(surface, w, h);
	cairo_destroy(ctx_paths);
	cairo_surface_destroy(surface_paths);
	surface_paths = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	ctx_paths = cairo_create(surface_paths);
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
		else if (key == GLFW_KEY_C) {
			if (mode == MODE_IDLE) {
				double x, y;
				glfwGetCursorPos(window, &x, &y);
				held_body = galaxy_body_add(&galaxy);
				body_init(held_body, 1000.0, x, y, 0, 0);
				mode = MODE_HOLD;
			}
		}
		else if (key == GLFW_KEY_P) {
			if (mods & GLFW_MOD_CONTROL) {
				cairo_operator_t op = cairo_get_operator(ctx_paths);
				cairo_set_operator(ctx_paths, CAIRO_OPERATOR_SOURCE);
				cairo_set_source_rgba(ctx_paths, 0.0, 0.0, 0.0, 0.0);
				cairo_paint(ctx_paths);
				cairo_set_operator(ctx_paths, op);
			}
			else {
				flags ^= FLAG_PATHS;
			}
		}
		else if (key == GLFW_KEY_R) {
			size_t n = 1;
			if (mods & GLFW_MOD_CONTROL) {
				n *= 10;
			}
			if (mode & GLFW_MOD_ALT) {
				n *= 10;
			}
			for (size_t i=0; i<n; ++i) {
				struct body * b = galaxy_body_add(&galaxy);
				body_init(b, rnd() * 1000.0, rnd() * width, rnd() * height, (rnd() - 0.5) * 2.0, (rnd() - 0.5) * 2.0);
			}
		}
		else if (key == GLFW_KEY_T) {
			if (mode == MODE_HOLD) {
				held_body->flags ^= BF_TRAIL;
			}
		}
		else if (key == GLFW_KEY_X) {
			if (mode == MODE_HOLD) {
				galaxy_body_remove(&galaxy, held_body);
				mode = MODE_IDLE;
			}
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

void glfw_mouse_button(GLFWwindow * window, int button, int action, int mods) {
	if (action == GLFW_RELEASE) {
		if (mode == MODE_IDLE) {
			double x, y;
			glfwGetCursorPos(window, &x, &y);
			held_body = galaxy_body_get(&galaxy, x, y);
			if (held_body != NULL) {
				held_body->flags &= ~BF_SIMULATE;
			}
			else {
				held_body = galaxy_body_add(&galaxy);
				body_init(held_body, 1000.0, x, y, 0, 0);
			}
			mode = MODE_HOLD;
		}
		else if (mode == MODE_HOLD) {
			held_body->flags |= BF_SIMULATE;
			held_body = NULL;
			mode = MODE_IDLE;
		}
	}
}

void glfw_cursor_pos(GLFWwindow * window, double x, double y) {
	if (mode == MODE_HOLD) {
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			held_body->v.x = x - held_body->p.x;
			held_body->v.y = y - held_body->p.y;
		}
		else {
			held_body->p.x = x;
			held_body->p.y = y;
			body_trail_reset(held_body);
		}
	}
}

void glfw_scroll(GLFWwindow * window, double dx, double dy) {
	if (mode == MODE_HOLD) {
		held_body->mass *= 1.0 + (dy / 10.0);
		body_recalc(held_body);
	}
}

size_t integrate(struct galaxy * galaxy, double delta);
void render(cairo_t * ctx, struct galaxy * galaxy);
void render_paths(cairo_t * ctx, struct galaxy * galaxy);

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

	surface_paths = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	ctx_paths = cairo_create(surface_paths);

	double t_last_frame = glfwGetTime();

	while (!glfwWindowShouldClose(window)) {
		double t_frame_start = glfwGetTime();
		double delta = t_frame_start - t_last_frame;

		if (flags & FLAG_SIMULATE) {
			for (size_t i=0; i<speed_scale; ++i) {
				galaxy_integrate(&galaxy, delta);
			}
		}

		cairo_set_source_rgba(ctx, 0.0, 0.0, 0.0, 1.0);
		cairo_paint(ctx);
		if (flags & FLAG_PATHS) {
			//galaxy_render_trails(ctx_paths, &galaxy);
			//cairo_set_source_surface(ctx, surface_paths, 0.0, 0.0);
			//cairo_paint(ctx);
			galaxy_render_trails(ctx, &galaxy);
		}
		galaxy_render(ctx, &galaxy);

		if (mode == MODE_HOLD) {
			float r = fmax(0, held_body->r * 0.7), g = fmax(0, held_body->g * 0.7), b = fmax(0, held_body->b * 0.7);
			cairo_set_source_rgba(ctx, r, g, b, 0.95);
			cairo_arc(ctx, held_body->p.x, held_body->p.y, held_body->radius, 0.0, 2 * PI);
			cairo_fill(ctx);

			cairo_set_source_rgba(ctx, r, g, b, 1.0);
			cairo_move_to(ctx, held_body->p.x, held_body->p.y);
			cairo_rel_line_to(ctx, held_body->v.x, held_body->v.y);
			cairo_stroke(ctx);
		}

		if (!(flags & FLAG_SIMULATE)) {
			//pause symbol
			cairo_set_source_rgba(ctx, 1.0, 1.0, 1.0, 0.8);
			cairo_rectangle(ctx, width - 48, height - 48, 12, 32);
			cairo_fill(ctx);
			cairo_rectangle(ctx, width - 48 + 18, height - 48, 12, 32);
			cairo_fill(ctx);
		}

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
