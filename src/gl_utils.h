
/* ================================================================================================
 * -*- C -*-
 * File: gl_utils.h
 * Created on: 29/08/15
 * Brief: Miscellaneous OpenGL helpers and a tiny GL/GLFW application framework.
 *
 * Source code licensed under the MIT license.
 * Copyright (C) 2015 Guilherme R. Lampert
 *
 * This software is provided "as is" without express or implied
 * warranties. You may freely copy and compile this source into
 * applications you distribute provided that this copyright text
 * is included in the resulting source code.
 * ================================================================================================ */

#ifndef DARKSTONE_GL_UTILS_H
#define DARKSTONE_GL_UTILS_H

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <vectormath.h>

/* ======================================================== */

typedef struct gl_draw_vertex {
	float px, py, pz;  // Position
	float nx, ny, nz;  // Normal vector
	float r,  g,  b;   // Vertex RGB color
	float u,  v;       // Texture coordinates
} gl_draw_vertex_t;

typedef struct gl_vbo {
	GLuint vertCount;  // Size in vertexes
	GLuint indexCount; // Size in indexes
	GLuint vaHandle;   // Vertex Array
	GLuint vbHandle;   // Vertex Buffer
	GLuint ibHandle;   // Index  Buffer
} gl_vbo_t;

typedef struct gl_program {
	GLuint progHandle;
	GLint  u_mvpMatrix;
	GLint  u_renderModeFlag;
} gl_program_t;

typedef struct gl_texture {
	GLuint texHandle;
	GLuint width;
	GLuint height;
	// Texture format is always RGBA!
} gl_texture_t;

// Generic application callback type.
typedef void (* app_callback_f)(void);

typedef struct glfw_app {
	int                windowWidth;
	int                windowHeight;
	const char *       windowTitle;
	float              clearScrColor[4];
	app_callback_f     onInitCallback;
	app_callback_f     onShutdownCallback;
	app_callback_f     onDrawCallback;
	GLFWmousebuttonfun mouseButtonCallback;
	GLFWcursorposfun   mousePosCallback;
	GLFWscrollfun      mouseScrollCallback;
	bool               useCustomCursor;
} glfw_app_t;

/* ========================================================
 * GL/App helpers:
 * ======================================================== */

#ifndef M_PI
	#define M_PI 3.14159265358979323846f
#endif // M_PI

#define DEG_TO_RAD(deg) ((M_PI / 180.0f) * (deg))
#define RAD_TO_DEG(rad) ((180.0f / M_PI) * (rad))

// Prints GL errors to STDOUT.
#define CHECK_GL_ERRORS() check_gl_errors_helper(__func__, __FILE__, __LINE__)
void check_gl_errors_helper(const char * function, const char * filename, int lineNum);

// Load a complete shader program from files and query the uniform locations.
gl_program_t load_gl_program(const char * vsFile, const char * fsFile);
void free_gl_program(gl_program_t * prog);

// Allocate VBO/set vertex format. Index buffer may be null/0.
gl_vbo_t create_gl_vbo(const void * vertexData, int vertCount, const void * indexData, int indexCount);
void setup_gl_vertex_format(void);
void free_gl_vbo(gl_vbo_t * vbo);

// Image loading via STB image (forces GL_RGBA).
gl_texture_t load_gl_texture_from_file(const char * filename);
void free_gl_texture(gl_texture_t * tex);

// Set the window cursor to the custom sword cursor of DarkStone.
// (which is loaded from "cursor24.png", assumed to be at the CWD!)
void set_custom_cursor(void);
void restore_default_cursor(void);

// Set the window title to the given format string. Max 1023 chars!
void set_window_title(const char * fmt, ...) __attribute__((format(printf, 1, 2)));

// Quits with a fatal error. Prints the message to STDERR.
void fatal_error(const char * message, ...) __attribute__((format(printf, 1, 2)));

// Create the application/window instance. Fires the user
// callbacks on success, quits with an error otherwise.
void init_glfw_app(glfw_app_t * app);

// Cleanly exits the application.
void quit_glfw_app(void);

#endif // DARKSTONE_GL_UTILS_H
